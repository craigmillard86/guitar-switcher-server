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
#include <Preferences.h>

esp_now_peer_info_t slave;
// Old button variables - now handled in commandHandler.cpp
// volatile unsigned long buttonPressTime = 0;
// unsigned long lastButtonEvent = 0;
unsigned long pairingStartTime = 0;

Preferences preferences;
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

void savePeers() {
  preferences.begin("espnow", false);

  int validCount = 0;

  logf(LOG_INFO, "Saving up to %d peers to NVS...", numClients);

  for (int i = 0; i < numClients; i++) {
    if (memcmp(clientMacAddresses[i], "\0\0\0\0\0\0", 6) != 0) {
      char key[12];
      sprintf(key, "peer_%d", validCount);  // use validCount index for keys
      preferences.putBytes(key, clientMacAddresses[i], 6);
      char nameKey[16];
      sprintf(nameKey, "peername_%d", validCount);
      preferences.putString(nameKey, getPeerName(clientMacAddresses[i]));
      logf(LOG_INFO, "Saved Peer: %s", getPeerName(clientMacAddresses[i]));
      printMAC(clientMacAddresses[i], LOG_DEBUG);
      validCount++;
    } else {
      logf(LOG_ERROR, "Skipped Peer %d - empty or invalid MAC", i);
    }
  }

  preferences.putInt("numClients", validCount);
  preferences.putInt("version", STORAGE_VERSION);

  preferences.end();
  logf(LOG_INFO, "Saved %d valid peers.", validCount);
  logf(LOG_DEBUG, "Actual numClients in RAM: %d", numClients);
}

void clearPeers(bool fullErase) {
  esp_now_deinit();
  esp_now_init();
  numClients = 0;
  preferences.begin("espnow", false);
  if (fullErase) preferences.clear();
  log(LOG_INFO, "All Peers Removed");
  preferences.putInt("version", STORAGE_VERSION);
  preferences.end();
  ESP.restart();
}

void loadPeers() {
 log(LOG_DEBUG, "Load Peers...");
  preferences.begin("espnow", true);
  
  if (preferences.getInt("version", 0) != STORAGE_VERSION) {
    preferences.end();
    clearPeers(false);
    preferences.begin("espnow", false);
    preferences.putInt("version", STORAGE_VERSION);
    preferences.end();
    return;
  }

  int storedClients = preferences.getInt("numClients", 0);
  logf(LOG_DEBUG, "numClients in NVS: %d", storedClients);
  int loadedClients = 0;

  for (int i = 0; i < storedClients; i++) {
    char key[12];
    char nameKey[16];
    //uint8_t mac[6];
    sprintf(key, "peer_%d", i);
    sprintf(nameKey, "peername_%d", i);
    size_t len = preferences.getBytesLength(key);

    if (len == 6) {
      preferences.getBytes(key, clientMacAddresses[loadedClients], 6);
      String name = preferences.getString(nameKey, "Unknown");
      addLabeledPeer(clientMacAddresses[loadedClients], name.c_str());
      addPeer(clientMacAddresses[loadedClients], false);
      logf(LOG_INFO, "Loading Peer: %s", name.c_str());
      printMAC(clientMacAddresses[loadedClients], LOG_INFO);
      loadedClients++;
    } 
    else 
    {
      logf(LOG_ERROR, "Warning: Key %s missing or corrupted", key);
    }
  }

  numClients = loadedClients; 
  preferences.end();
  logf(LOG_DEBUG, "Actual numClients in RAM: %d", numClients);
}

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
    // Slave already paired.
    log(LOG_DEBUG, "Already Paired");
    return true;
  }
  if (memcmp(peer_addr, "\0\0\0\0\0\0", 6) == 0) {
    log(LOG_DEBUG, "Invalid MAC address — not adding.");
    return false;
  }
  else {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK) {
      // Pair success
      memcpy(clientMacAddresses[numClients], peer_addr, 6);
      numClients++;
      log(LOG_DEBUG, "Pair success");
      if (save) savePeers();
      return true;
    }
    else 
    {
      log(LOG_DEBUG, "Pair failed");
      return false;
    }
  }
} 

