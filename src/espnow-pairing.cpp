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


esp_now_peer_info_t slave;
// Old button variables - now handled in commandHandler.cpp
// volatile unsigned long buttonPressTime = 0;
// unsigned long lastButtonEvent = 0;
unsigned long pairingStartTime = 0;


uint8_t clientMacAddresses[MAX_CLIENTS][6];
int numClients = 0;

bool addPeer(const uint8_t *peer_addr, bool save = true);

// Old interrupt-based button handling - now replaced with polling system
/*
void IRAM_ATTR pairingButtonISR() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastButtonEvent > DEBOUNCE_TIME) {
    lastButtonEvent = currentMillis;
    if (buttonPressTime == 0) {
      buttonPressTime = currentMillis;
    }
  }
}
*/

void setupPairingButtonAndLED() {
  pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);
  // No longer using interrupt-based handling - using polling in checkPairingButtons()
  pinMode(PAIRING_LED_PIN, OUTPUT);
  digitalWrite(PAIRING_LED_PIN, LOW);
  
  // Initialize LED system
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(PAIRING_LED_PIN, LEDC_CHANNEL_0);
}

// Old button handling function - replaced with sophisticated system in commandHandler.cpp
/*
void handlePairingButtonPress() {
  if (buttonPressTime != 0) {
    if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
      if ((millis() - buttonPressTime) > RESET_HOLD_TIME) {
        resetMode = true;
        // Wait for button release to actually clear peers
      }
    } else {
      if (resetMode) {
        //clearPeers(true);
        log(LOG_DEBUG, "Peers cleared (long press).");
      } else {
        pairingRequested = true;
        pairingMode = true;
        pairingStartTime = millis();
        log(LOG_DEBUG, "Pairing mode enabled (short press).");
      }
      buttonPressTime = 0;
      resetMode = false;
    }
  }
}
*/

void pairingforceStart(){
  pairingRequested = true;
  pairingMode = true;
  pairingStartTime = millis();
  log(LOG_DEBUG, "Pairing mode enabled (forced).");
}


// Centralized in nvsManager.cpp
#include <nvsManager.h>




void checkPairingTimeout() {
  if (pairingMode && (millis() - pairingStartTime > PAIRING_TIMEOUT_MS)) {
    pairingMode = false;
    log(LOG_INFO, "Pairing mode DISABLED (timeout)");
  }
}

void updatePairingLED() {
    static uint16_t fadeValue = 0;
    static int8_t fadeDirection = 1;
    static unsigned long lastUpdate = 0;
    static bool ledState = LOW;
    static unsigned long lastFadeUpdate = 0;
    unsigned long now = millis();

    // Only override patterns during active pairing phases, not when paired
    if (pairingMode) {
        if (currentLedPattern != LED_FADE) {
            currentLedPattern = LED_FADE;
            ledPatternStart = now;
            ledPatternStep = 0;
        }
    } else if (serialOtaTrigger) {
        if (currentLedPattern != LED_FAST_BLINK) {
            currentLedPattern = LED_FAST_BLINK;
            ledPatternStart = now;
            ledPatternStep = 0;
        }
    }

    switch (currentLedPattern) {
        case LED_SINGLE_FLASH:
            if (ledPatternStep == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
                if (now - ledPatternStart > 80) { ledPatternStep = 1; ledPatternStart = now; }
            } else if (ledPatternStep == 1) {
                ledcWrite(LEDC_CHANNEL_0, 0);
                if (now - ledPatternStart > 120) { currentLedPattern = LED_OFF; }
            }
            break;
        case LED_DOUBLE_FLASH:
            if (ledPatternStep == 0 || ledPatternStep == 2) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
                if (now - ledPatternStart > 60) { ledPatternStep++; ledPatternStart = now; }
            } else if (ledPatternStep == 1 || ledPatternStep == 3) {
                ledcWrite(LEDC_CHANNEL_0, 0);
                if (now - ledPatternStart > 60) { ledPatternStep++; ledPatternStart = now; }
            } else {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_TRIPLE_FLASH:
            if (ledPatternStep % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            if (now - ledPatternStart > 50) {
                ledPatternStep++;
                ledPatternStart = now;
            }
            if (ledPatternStep > 5) {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_FAST_BLINK:
            ledcWrite(LEDC_CHANNEL_0, (now / 100) % 2 ? 8191 : 0);
            break;
        case LED_SOLID_ON:
            ledcWrite(LEDC_CHANNEL_0, 8191);
            break;
        case LED_FADE:
            // Smooth 2-second fade cycle - continuous fade from bottom to top and back
            if (now - lastFadeUpdate > 20) { // Update every 20ms for smooth fade
                static bool started = false;
                
                if (!started) {
                    fadeValue = 0; // Start from bottom (off)
                    fadeDirection = 1; // Start fading up
                    started = true;
                }
                
                fadeValue += fadeDirection * 20; // Smaller increment for smooth fade
                
                if (fadeValue >= 8191) {
                    fadeValue = 8191;
                    fadeDirection = -1; // Start fading down
                } else if (fadeValue <= 0) {
                    fadeValue = 0;
                    fadeDirection = 1; // Start fading up
                }
                
                ledcWrite(LEDC_CHANNEL_0, fadeValue);
                lastFadeUpdate = now;
            }
            break;
        case LED_OFF:
        default:
            ledcWrite(LEDC_CHANNEL_0, 0);
            break;
    }
}

bool isPeerAlreadyAdded(const uint8_t *mac_addr) {
  for (int i = 0; i < numClients; i++) {
    if (memcmp(clientMacAddresses[i], mac_addr, 6) == 0) return true;
  }
  return false;
}

bool addPeer(const uint8_t *peer_addr, bool save) {      // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);
  slave.channel = chan; // pick a channel
  slave.encrypt = 0; // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists || numClients >= MAX_CLIENTS) {
    log(LOG_DEBUG, "Already Paired");
    return true;
  }
  if (memcmp(peer_addr, "\0\0\0\0\0\0", 6) == 0) {
    log(LOG_DEBUG, "Invalid MAC address — not adding.");
    return false;
  }
  esp_err_t addStatus = esp_now_add_peer(peer);
  if (addStatus == ESP_OK) {
    // Pair success
    memcpy(clientMacAddresses[numClients], peer_addr, 6);
    numClients++;
    // Also add to labeledPeers if not present
    bool found = false;
    for (int i = 0; i < numLabeledPeers; i++) {
      if (memcmp(labeledPeers[i].mac, peer_addr, 6) == 0) {
        found = true;
        break;
      }
    }
    if (!found && numLabeledPeers < MAX_CLIENTS) {
      memcpy(labeledPeers[numLabeledPeers].mac, peer_addr, 6);
      // Use pairingData.name if available, else empty string
      extern struct_pairing pairingData;
      strncpy(labeledPeers[numLabeledPeers].name, pairingData.name, MAX_PEER_NAME_LEN);
      numLabeledPeers++;
    }
    // Keep numClients and numLabeledPeers in sync
    if (numLabeledPeers < numClients) numLabeledPeers = numClients;
    log(LOG_DEBUG, "Pair success");
    if (save) savePeersToNVS();
    return true;
  } else {
    log(LOG_DEBUG, "Pair failed");
    return false;
  }
} 

