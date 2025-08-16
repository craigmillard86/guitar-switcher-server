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
#include <Arduino.h>
#pragma once

// Client Type Definitions
enum ClientType {
    CLIENT_AMP_SWITCHER,
    CLIENT_CUSTOM
};

// Default client type if not specified
#ifndef CLIENT_TYPE
#define CLIENT_TYPE AMP_SWITCHER
#endif

// Client Type Configuration
#if CLIENT_TYPE == AMP_SWITCHER
    #define CLIENT_TYPE_ENUM CLIENT_AMP_SWITCHER
    #define HAS_AMP_SWITCHING true
    
    #ifndef MAX_AMPSWITCHS
    #define MAX_AMPSWITCHS 2
    #endif
    
    #ifndef AMP_SWITCH_PINS
    #define AMP_SWITCH_PINS "4,5"
    #endif
    
    #ifndef AMP_BUTTON_PINS
    #define AMP_BUTTON_PINS "9,10"
    #endif

#else // CUSTOM
    #define CLIENT_TYPE_ENUM CLIENT_CUSTOM
    #define HAS_AMP_SWITCHING false
#endif

// Device name configuration
#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32_CLIENT"
#endif

// Pin assignments and hardware config

#ifndef PAIRING_LED_PIN
#define PAIRING_LED_PIN 8
#endif
#ifndef MIDI_RX_PIN
#define MIDI_RX_PIN 6
#endif
#ifndef MIDI_TX_PIN
#define MIDI_TX_PIN 7
#endif
#ifndef LEDC_CHANNEL_0
#define LEDC_CHANNEL_0 0
#endif
#ifndef LEDC_TIMER_13_BIT
#define LEDC_TIMER_13_BIT 13
#endif
#ifndef LEDC_BASE_FREQ
#define LEDC_BASE_FREQ 1000
#endif
#ifndef PAIRING_LED_BLINK
#define PAIRING_LED_BLINK 100
#endif
#ifndef PAIRING_RETRY_DELAY
#define PAIRING_RETRY_DELAY 300
#endif
#ifndef MAX_CHANNEL
#define MAX_CHANNEL 13
#endif
#ifndef MAX_PEER_NAME_LEN
#define MAX_PEER_NAME_LEN 32
#endif
#ifndef NVS_NAMESPACE
#define NVS_NAMESPACE "pairing"
#endif
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 100 // Button debounce duration in ms
#endif
#ifndef BUTTON_LONGPRESS_MS
#define BUTTON_LONGPRESS_MS 5000 // Button long-press duration in ms
#endif

// Function declarations
uint8_t* parsePinArray(const char* pinString);
String getClientTypeString();
void printClientConfiguration();
void initializeClientConfiguration(); 