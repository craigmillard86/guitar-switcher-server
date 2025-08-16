// Copyright (c) Craig Millard and contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <Arduino.h>
#include "config.h"
#include "pairing.h"
#include "espnow-pairing.h"
#include "globals.h"
#include "utils.h"
#include "nvsManager.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <espnow.h>

unsigned long currentMillis = millis();
unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 10000;        // Interval at which to publish sensor readings
unsigned long start;                // used to measure Pairing time
unsigned int readingId = 0;   

void updatePairingLED() {
  // This function is no longer used - LED control moved to updateStatusLED()
  // Keeping this as a stub to avoid compilation errors
}

void addPeer(const uint8_t* mac_addr, uint8_t chan) {
    log(LOG_DEBUG, "Adding peer to ESP-NOW...");
    
    // Set the WiFi channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
    logf(LOG_DEBUG, "WiFi channel set to %u", chan);

    // Remove the peer first to avoid duplicates
    esp_now_del_peer(mac_addr);

    // Prepare the peer info struct
    memset(&peer, 0, sizeof(esp_now_peer_info_t));
    peer.channel = chan;
    peer.encrypt = false;
    memcpy(peer.peer_addr, mac_addr, 6);

    // Try to add the peer
    if (esp_now_add_peer(&peer) == ESP_OK) {
        log(LOG_INFO, "Peer added successfully");
        printMAC(mac_addr, LOG_INFO);

        // Only save if this is a new or changed server
        bool shouldSave = false;
        for (int i = 0; i < 6; i++) {
            if (serverAddress[i] != mac_addr[i]) {
                shouldSave = true;
                break;
            }
        }
        if (!shouldSave && currentChannel != chan) {
            shouldSave = true;
        }

        // Update RAM copy in any case
        memcpy(serverAddress, mac_addr, 6);
        currentChannel = chan;

        if (shouldSave) {
            saveServerToNVS(mac_addr, chan);
            log(LOG_DEBUG, "Server info saved to NVS");
        } else {
            log(LOG_DEBUG, "Server info unchanged, not saving to NVS");
        }
    } else {
        log(LOG_ERROR, "Failed to add peer!");
        printMAC(mac_addr, LOG_ERROR);
    }
}

PairingStatus autoPairing(){
  switch(pairingStatus) {
    case PAIR_REQUEST:
      logf(LOG_INFO, "Starting pairing on channel %u", currentChannel);

      // set WiFi channel   
      ESP_ERROR_CHECK(esp_wifi_set_channel(currentChannel,  WIFI_SECOND_CHAN_NONE));
      initESP_NOW();
    
      // set pairing data to send to the server
      pairingData.msgType = PAIRING;
      pairingData.id = BOARD_ID;     
      pairingData.channel = currentChannel;
      pairingData.macAddr[0] = clientMacAddress[0];
      pairingData.macAddr[1] = clientMacAddress[1];
      pairingData.macAddr[2] = clientMacAddress[2];
      pairingData.macAddr[3] = clientMacAddress[3];
      pairingData.macAddr[4] = clientMacAddress[4];
      pairingData.macAddr[5] = clientMacAddress[5];
      strncpy(pairingData.name, deviceName, MAX_PEER_NAME_LEN);
      
      // add peer and send request
      addPeer(serverAddress, currentChannel);
      esp_now_send(serverAddress, (uint8_t *) &pairingData, sizeof(pairingData));
      log(LOG_DEBUG, "Pairing request sent");
      previousMillis = millis();
      pairingStatus = PAIR_REQUESTED;
      break;

    case PAIR_REQUESTED:
      // time out to allow receiving response from server
      currentMillis = millis();
      if(currentMillis - previousMillis > 1000) {
        previousMillis = currentMillis;
        // time out expired,  try next channel
        currentChannel ++;
        if (currentChannel > MAX_CHANNEL){
          currentChannel = 1;
        }   
        logf(LOG_DEBUG, "Pairing timeout, trying channel %u", currentChannel);
        pairingStatus = PAIR_REQUEST;
      }
    break;

    case PAIR_PAIRED:
      // nothing to do here 
    break;
  }
  return pairingStatus;
}  
