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
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <globals.h>
#include <datastructs.h>
#include <utils.h>
#include <espnow-pairing.h>


uint8_t clientMacAddress[6];

struct_message incomingReadings;

struct_pairing pairingData;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  logf(LOG_DEBUG, "Last Packet Send Status: %s to ", 
       status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  printMAC(mac_addr, LOG_DEBUG);
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  logf(LOG_DEBUG, "%d bytes of new data received.", len);
  String payload;
  uint8_t type = incomingData[0];       // first message byte is the type of message 
  switch (type) {
  case COMMAND :
    if (strcmp(getPeerName(mac_addr), "Unknown") == 0) {
        log(LOG_INFO, "Rejected DATA from unknown MAC: ");
        printMAC(mac_addr, LOG_INFO);
        return;
    }
     memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
     logf(LOG_DEBUG, "ID: %d", incomingReadings.id);
     logf(LOG_DEBUG, "Command Type: %d", incomingReadings.commandType);
     logf(LOG_DEBUG, "Command Value: %d", incomingReadings.commandValue);
     break;
  case DATA :                           // the message is data type
      if (strcmp(getPeerName(mac_addr), "Unknown") == 0) {
        log(LOG_INFO, "Rejected DATA from unknown MAC: ");
        printMAC(mac_addr, LOG_INFO);
        return;
    }
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
     logf(LOG_DEBUG, "ID: %d", incomingReadings.id);
     logf(LOG_DEBUG, "Reading ID: %d", incomingReadings.readingId);
     log(LOG_DEBUG, "Event send:");

    break;
  
  case PAIRING:                            // the message is a pairing request 
   if (!pairingMode) {
      log(LOG_INFO, "Pairing not enabled - ignored.");
      return;
    }  
    memcpy(&pairingData, incomingData, sizeof(pairingData));
    logf(LOG_DEBUG, "Pairing message type: %d", pairingData.msgType);
    logf(LOG_DEBUG, "Pairing ID: %d", pairingData.id);
    log(LOG_INFO, "Pairing request from MAC Address: ");
    printMAC(pairingData.macAddr, LOG_INFO);
    logf(LOG_INFO, "Named: %s", pairingData.name);
  logf(LOG_INFO, "Client was on channel: %d", pairingData.channel);

    clientMacAddress[0] = pairingData.macAddr[0];
    clientMacAddress[1] = pairingData.macAddr[1];
    clientMacAddress[2] = pairingData.macAddr[2];
    clientMacAddress[3] = pairingData.macAddr[3];
    clientMacAddress[4] = pairingData.macAddr[4];
    clientMacAddress[5] = pairingData.macAddr[5];

    if (pairingData.id > 0) {     // do not replay to server itself
      if (pairingData.msgType == PAIRING) { 
        pairingData.id = 0;       // 0 is server
        log(LOG_INFO, "Pairing MAC Address: ");
        printMAC(clientMacAddress, LOG_INFO);
        esp_wifi_get_mac(WIFI_IF_STA, pairingData.macAddr);
        pairingData.channel = chan;
        logf(LOG_INFO, "Server instructs client to switch to channel: %d", chan);
        addLabeledPeer(clientMacAddress,pairingData.name);
        addPeer(clientMacAddress, true);  // Add to ESP-NOW peer list first
        esp_err_t result = esp_now_send(clientMacAddress, (uint8_t *) &pairingData, sizeof(pairingData));
        logf(LOG_INFO, "esp_now_send result: %s (0x%04X)", esp_err_to_name(result), result);
      }  
    }  
    break; 
  }
}

void initESP_NOW(){
    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
      log(LOG_ERROR, "Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
} 

