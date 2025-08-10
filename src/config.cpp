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

#include "config.h"
#include "globals.h"
#include "utils.h"
#include <cstring>

// Parse comma-separated pin string into array
uint8_t* parsePinArray(const char* pinString) {
    static uint8_t pins[8] = {255, 255, 255, 255, 255, 255, 255, 255}; // Invalid pin = 255
    
    if (!pinString) return pins;
    
    char buffer[64];
    strncpy(buffer, pinString, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    int index = 0;
    char* token = strtok(buffer, ",");
    
    while (token && index < 8) {
        int pin = atoi(token);
        if (pin >= 0 && pin <= 39) { // Valid ESP32 pin range
            pins[index] = (uint8_t)pin;
        } else {
            pins[index] = 255; // Invalid pin
        }
        index++;
        token = strtok(nullptr, ",");
    }
    
    return pins;
}



void printServerConfiguration() {
    log(LOG_INFO, "=== SERVER CONFIGURATION ===");
    
    logf(LOG_INFO, "Server Type: Guitar Switcher Server");
    
    if (deviceName[0] != '\0') {
        logf(LOG_INFO, "Device Name: %s", deviceName);
    } else {
        log(LOG_INFO, "Device Name: Not set");
    }
    
    logf(LOG_INFO, "Firmware Version: %s", FIRMWARE_VERSION);
    
#if HAS_RELAY_OUTPUTS
    log(LOG_INFO, "Relay Outputs: Enabled");
    logf(LOG_INFO, "Max Relay Channels: %d", MAX_RELAY_CHANNELS);
    
    log(LOG_INFO, "Relay Pins:");
    for (int i = 0; i < MAX_RELAY_CHANNELS && i < 8; i++) {
        if (relayOutputPins[i] != 255) {
            logf(LOG_INFO, "  Pin %d: GPIO %d", i, relayOutputPins[i]);
        }
    }
#else
    log(LOG_INFO, "Relay Outputs: Disabled");
#endif

#if HAS_FOOTSWITCH
    log(LOG_INFO, "Footswitch: Enabled");
    log(LOG_INFO, "Footswitch Pins:");
    for (int i = 0; i < 4; i++) {
        if (footswitchPins[i] != 255) {
            logf(LOG_INFO, "  Pin %d: GPIO %d", i, footswitchPins[i]);
        }
    }
#else
    log(LOG_INFO, "Footswitch: Disabled");
#endif

    logf(LOG_INFO, "Pairing LED Pin: %d", PAIRING_LED_PIN);
    logf(LOG_INFO, "Pairing Button Pin: %d", PAIRING_BUTTON_PIN);
    

    logf(LOG_INFO, "Max Clients: %d", MAX_CLIENTS);
    logf(LOG_INFO, "NVS Namespace: %s", NVS_NAMESPACE);
    logf(LOG_INFO, "Storage Version: %d", STORAGE_VERSION);
    log(LOG_INFO, "=== END CONFIGURATION ===");
}

void initializeServerConfiguration() {
    log(LOG_INFO, "Initializing server configuration...");
    
    // Feed watchdog to prevent timeout during initialization
    yield();
    
#if HAS_RELAY_OUTPUTS
    // Parse and set relay output pins from macro
    log(LOG_DEBUG, "Initializing relay pins...");
    
    uint8_t* relayPins = parsePinArray(RELAY_OUTPUT_PINS);
    
    if (relayPins != nullptr) {
        for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
            relayOutputPins[i] = relayPins[i];
            if (relayOutputPins[i] != 255) {
                pinMode(relayOutputPins[i], OUTPUT);
                digitalWrite(relayOutputPins[i], LOW); // Ensure relays are off at boot
            }
            yield(); // Feed watchdog during loop
        }
        log(LOG_DEBUG, "Relay output pins initialized");
    } else {
        log(LOG_ERROR, "Failed to parse relay pins");
    }
#endif

    yield(); // Feed watchdog before next section

#if HAS_FOOTSWITCH
    // Parse and set footswitch pins from macro
    log(LOG_DEBUG, "Initializing footswitch pins...");
    
    uint8_t* footPins = parsePinArray(FOOTSWITCH_PINS);
    if (footPins != nullptr) {
        for (int i = 0; i < 4; i++) {
            footswitchPins[i] = footPins[i];
            if (footswitchPins[i] != 255) {
                pinMode(footswitchPins[i], INPUT_PULLUP);
            }
        }
        log(LOG_DEBUG, "Footswitch pins initialized");
    } else {
        log(LOG_ERROR, "Failed to parse footswitch pins");
    }
#endif

    // Set device name from macro
    log(LOG_DEBUG, "Setting device name...");
    
    if (deviceName != nullptr && strlen(DEVICE_NAME) > 0) {
        strncpy(deviceName, DEVICE_NAME, MAX_PEER_NAME_LEN-1);
        deviceName[MAX_PEER_NAME_LEN-1] = '\0';
        log(LOG_DEBUG, "Device name set successfully");
    } else {
        log(LOG_ERROR, "Device name setup failed");
    }
    
    // Initialize pairing button and LED
    log(LOG_DEBUG, "Initializing pairing button and LED...");
    
    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PAIRING_LED_PIN, OUTPUT);

    // Optional additional buttons
#ifdef SERVER_BUTTON_PINS
    {
        uint8_t* extra = parsePinArray(SERVER_BUTTON_PINS);
        // Fill serverButtonPins starting at index 0 (overwrite default index 0 with PAIRING_BUTTON_PIN), keep count
        serverButtonPins[0] = PAIRING_BUTTON_PIN;
        serverButtonCount = 1;
        for (int i=0;i<7;i++) {
            if (extra[i] != 255 && extra[i] != PAIRING_BUTTON_PIN) {
                serverButtonPins[serverButtonCount++] = extra[i];
                pinMode(extra[i], INPUT_PULLUP);
            }
        }
        logf(LOG_INFO, "Configured %u server buttons", serverButtonCount);
    }
#else
    serverButtonCount = 1; // only pairing button
#endif
    
    pinMode(PAIRING_LED_PIN, OUTPUT);
    digitalWrite(PAIRING_LED_PIN, LOW);
    
    log(LOG_DEBUG, "Hardware initialization complete");
    
    yield(); // Feed watchdog before final step
    
    printServerConfiguration();
    
    log(LOG_INFO, "Server configuration initialization complete");
}
