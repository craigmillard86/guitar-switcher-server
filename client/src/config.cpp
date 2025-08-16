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
#include "utils.h"
#include <cstring>

// Global configuration variables
ClientType currentClientType = CLIENT_TYPE_ENUM;

// Pin arrays are defined in globals.cpp
// These are just references to the existing arrays

// Parse pin array from string (e.g., "4,5,6,7")
uint8_t* parsePinArray(const char* pinString) {
    static uint8_t pins[16]; // Max 16 pins
    int pinCount = 0;
    
    char* str = strdup(pinString);
    char* token = strtok(str, ",");
    
    while (token != NULL && pinCount < 16) {
        pins[pinCount++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    free(str);
    return pins;
}

String getClientTypeString() {
    switch (currentClientType) {
        case CLIENT_AMP_SWITCHER:
            return "AMP_SWITCHER";
        case CLIENT_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

void printClientConfiguration() {
    log(LOG_INFO, "=== CLIENT CONFIGURATION ===");
    logf(LOG_INFO, "Client Type: %s", getClientTypeString());
    logf(LOG_INFO, "Device Name: %s", deviceName);
    
#if HAS_AMP_SWITCHING
    log(LOG_INFO, "Amp Switching: Enabled");
    logf(LOG_INFO, "Max Amp Switches: %d", MAX_AMPSWITCHS);
    // Print ampSwitchPins as comma-separated string with bounds checking
    char switchPinsStr[64] = "";
    size_t switchStrLen = 0;
    const size_t maxPins = min(MAX_AMPSWITCHS, 8);  // Use min for safety
    for (int i = 0; i < maxPins; i++) {
        char pinStr[8];
        int written = snprintf(pinStr, sizeof(pinStr), "%d", ampSwitchPins[i]);
        if (written < 0 || written >= (int)sizeof(pinStr)) {
            logf(LOG_ERROR, "Pin string formatting error at index %d", i);
            break;
        }
        
        // Check if we have space for the pin string plus comma
        size_t needed = strlen(pinStr) + (i < MAX_AMPSWITCHS - 1 ? 1 : 0);
        if (switchStrLen + needed >= sizeof(switchPinsStr) - 1) {
            log(LOG_WARN, "Switch pins string buffer overflow prevented");
            break;
        }
        
        strcat(switchPinsStr, pinStr);
        switchStrLen += strlen(pinStr);
        if (i < MAX_AMPSWITCHS - 1) {
            strcat(switchPinsStr, ",");
            switchStrLen++;
        }
    }
    logf(LOG_INFO, "Amp Switch Pins: %s", switchPinsStr);
    
    // Print ampButtonPins as comma-separated string with bounds checking
    char buttonPinsStr[64] = "";
    size_t buttonStrLen = 0;
    for (int i = 0; i < MAX_AMPSWITCHS && i < 8; i++) {  // Extra bounds check
        char pinStr[8];
        int written = snprintf(pinStr, sizeof(pinStr), "%d", ampButtonPins[i]);
        if (written < 0 || written >= (int)sizeof(pinStr)) {
            logf(LOG_ERROR, "Button pin string formatting error at index %d", i);
            break;
        }
        
        // Check if we have space for the pin string plus comma
        size_t needed = strlen(pinStr) + (i < MAX_AMPSWITCHS - 1 ? 1 : 0);
        if (buttonStrLen + needed >= sizeof(buttonPinsStr) - 1) {
            log(LOG_WARN, "Button pins string buffer overflow prevented");
            break;
        }
        
        strcat(buttonPinsStr, pinStr);
        buttonStrLen += strlen(pinStr);
        if (i < MAX_AMPSWITCHS - 1) {
            strcat(buttonPinsStr, ",");
            buttonStrLen++;
        }
    }
    logf(LOG_INFO, "Amp Button Pins: %s", buttonPinsStr);
#else
    log(LOG_INFO, "Amp Switching: Disabled");
#endif
    log(LOG_INFO, "==========================");
}

void initializeClientConfiguration() {
    log(LOG_INFO, "Initializing client configuration...");
    
#if HAS_AMP_SWITCHING
    // Parse and set amp switch pins from macro
    uint8_t* switchPins = parsePinArray(AMP_SWITCH_PINS);
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        ampSwitchPins[i] = switchPins[i];
    }
    uint8_t* buttonPins = parsePinArray(AMP_BUTTON_PINS);
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        ampButtonPins[i] = buttonPins[i];
        pinMode(ampButtonPins[i], INPUT_PULLUP);
        pinMode(ampSwitchPins[i], OUTPUT);
        digitalWrite(ampSwitchPins[i], LOW); // Ensure relays are off at boot
    }
    digitalWrite(ampSwitchPins[0], HIGH);
    log(LOG_DEBUG, "Amp switching pins initialized");
#endif
    // Set device name from macro
    strncpy(deviceName, DEVICE_NAME, MAX_PEER_NAME_LEN-1);
    deviceName[MAX_PEER_NAME_LEN-1] = '\0';
    printClientConfiguration();
} 