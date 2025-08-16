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
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include <esp_wifi_types.h>
#include <globals.h>
#include <utils.h>
#include <dataStructs.h>
#include <espnow.h>
#include <espnow-pairing.h>
#include <otaManager.h>
#include <nvsManager.h>
#include <debug.h>
#include <commandHandler.h>
#include <config.h>
#include <relayControl.h>
#include <midiInput.h>
#include <nvsManager.h>

struct_message outgoingSetpoints;
struct_message outgoingCommand;
MessageType messageType;

int counter = 0;
bool lastFootswitchState = false;

void setupWiFiChannel() {
  WiFi.mode(WIFI_STA);
  //esp_wifi_set_channel(4,WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);
  //WiFi.begin();
  //Force espnow to use a specific channel
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    wifi_second_chan_t secondChan = WIFI_SECOND_CHAN_NONE;
  esp_wifi_set_channel(chan, secondChan);
  ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
  log(LOG_INFO, "Server MAC Address: ");
  readMacAddress();
  chan = WiFi.channel();
  logf(LOG_INFO, "Wi-Fi Channel: %d", WiFi.channel());
}

void readDataToSend() {
  outgoingSetpoints.msgType = DATA;
  outgoingSetpoints.id = 0;
  outgoingSetpoints.readingId = counter++;
  outgoingSetpoints.commandType = 0;
  outgoingSetpoints.commandValue = 0;
  outgoingSetpoints.targetChannel = 0;
  outgoingSetpoints.timestamp = millis();
}

void setup() {

  delay(5000);
  // Initialize Serial Monitor
  Serial.begin(115200);

  //Setup LED
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(PAIRING_LED_PIN, LEDC_CHANNEL_0);

  unsigned long serialWaitStart = millis();
  log(LOG_INFO, "Enter 'ota' within 10 seconds to enter OTA mode...");

  while (millis() - serialWaitStart < 10000) {
    checkSerialCommands();
    delay(10);
  if (serialOtaTrigger) break;
  }

  if (checkOtaTrigger() || serialOtaTrigger) {
    updatePairingLED();
    startOTA();
    return;
  }

  checkNVS();
  
  // Initialize server configuration
  initializeServerConfiguration();
  
  // Load server configuration from NVS (including channel)
  loadServerConfigFromNVS();
  
  // Load log level from NVS and initialize performance metrics
  currentLogLevel = loadLogLevelFromNVS();
  initializePerformanceMetrics();
  
  setupWiFiChannel();
  
  // Save the actual channel being used to NVS for consistency
  saveServerConfigToNVS();
  
  setupPairingButtonAndLED();  // This will now use the new system
  initESP_NOW();
  loadPeersFromNVS();
  initMidiInput();
  loadServerMidiConfigFromNVS();
  loadServerButtonPcMapFromNVS();
}
void prepareChannelChangeCommand() {
  outgoingCommand.msgType = COMMAND;
  outgoingCommand.id = 0;
  outgoingCommand.commandType = PROGRAM_CHANGE;
  outgoingCommand.commandValue = 1;
}
void loop() {
  unsigned long loopStart = millis();
  
  // Check buttons for pairing/LED functions
  checkPairingButtons();
  
  // Handle LED patterns
  updateLedPatterns();
  
  // Update footswitch state
  updateFootswitchState();
  // Poll MIDI input (non-blocking)
  processMidiInput();


  if (footswitchPressed && !lastFootswitchState) {
    prepareChannelChangeCommand();
    for (int i = 0; i < numLabeledPeers; i++) {
      esp_now_send(labeledPeers[i].mac, (uint8_t *)&outgoingCommand, sizeof(outgoingCommand));
      logf(LOG_INFO, "Footswitch pressed: sent channel change command to peer %s", labeledPeers[i].name);
    }
  }

  lastFootswitchState = footswitchPressed;

  // Removed continuous data sending - only send commands when needed
  
  checkPairingButtons();     // New sophisticated button handling
  checkPairingTimeout();
  updatePairingLED();        // Now using advanced LED patterns
  checkSerialCommands();
  // (Optional) future: MIDI learn timeout handling could go here
  
  // Update performance metrics
  unsigned long loopTime = millis() - loopStart;
  updatePerformanceMetrics(loopTime);
}