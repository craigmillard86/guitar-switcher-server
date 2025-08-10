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

#pragma once
#include <Arduino.h>

#define HAS_FOOTSWITCH true
#define HAS_RELAY_OUTPUTS true

#ifndef MAX_RELAY_CHANNELS
#define MAX_RELAY_CHANNELS 4
#endif

#ifndef RELAY_OUTPUT_PINS
#define RELAY_OUTPUT_PINS "6,7"
#endif

#ifndef FOOTSWITCH_PINS
#define FOOTSWITCH_PINS "4,5"
#endif

// Device name configuration
#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32_SERVER"
#endif

// Pin assignments and hardware config
#ifndef PAIRING_LED_PIN
#define PAIRING_LED_PIN 2
#endif

#ifndef PAIRING_BUTTON_PIN
#define PAIRING_BUTTON_PIN 0
#endif

// LEDC Configuration
#ifndef LEDC_CHANNEL_0
#define LEDC_CHANNEL_0 0
#endif

#ifndef LEDC_TIMER_13_BIT
#define LEDC_TIMER_13_BIT 13
#endif

#ifndef LEDC_BASE_FREQ
#define LEDC_BASE_FREQ 1000
#endif


// MIDI UART configuration (5-pin DIN IN -> opto to RX). Adjust as needed.
#ifndef MIDI_UART_NUM
#define MIDI_UART_NUM UART_NUM_1
#endif

#ifndef MIDI_UART_RX_PIN
#define MIDI_UART_RX_PIN 9   // Set to the GPIO wired to MIDI IN (RO output of optocoupler)
#endif

#ifndef MIDI_BAUD_RATE
#define MIDI_BAUD_RATE 31250
#endif

#ifndef ENABLE_MIDI_INPUT
#define ENABLE_MIDI_INPUT 1   // Set to 0 to compile without MIDI input
#endif

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 10
#endif

#ifndef MAX_PEER_NAME_LEN
#define MAX_PEER_NAME_LEN 32
#endif

// Timing configurations
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 100
#endif

#ifndef BUTTON_LONGPRESS_MS
#define BUTTON_LONGPRESS_MS 5000
#endif

#ifndef PAIRING_TIMEOUT_MS
#define PAIRING_TIMEOUT_MS 30000
#endif

// Performance settings
#ifndef EVENT_INTERVAL_MS
#define EVENT_INTERVAL_MS 5000
#endif

// Storage settings
#ifndef NVS_NAMESPACE
#define NVS_NAMESPACE "server"
#endif

#ifndef STORAGE_VERSION
#define STORAGE_VERSION 1
#endif

// Function declarations
uint8_t* parsePinArray(const char* pinString);
String getServerTypeString();
void printServerConfiguration();
void initializeServerConfiguration();
