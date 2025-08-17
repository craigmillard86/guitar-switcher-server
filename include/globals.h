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
#pragma once

#include <Arduino.h>
#include <dataStructs.h>
#include "config.h"

#define OTA_BUTTON_PIN 0          // Use existing button (e.g., GPIO0)
#define OTA_HOLD_TIME 2000

#define FIRMWARE_VERSION "1.0.0"
extern uint8_t chan;  // Shared Wi-Fi channel

#define RESET_HOLD_TIME 5000
#define DEBOUNCE_TIME 200
extern volatile bool pairingRequested;
extern bool pairingMode;
extern bool resetMode;
extern bool serialOtaTrigger;
extern bool serialConfigTrigger;
#define MAX_CLIENTS 10
#define STORAGE_VERSION 1
extern uint8_t clientMacAddresses[MAX_CLIENTS][6];
extern int numClients;
extern PeerInfo labeledPeers[MAX_CLIENTS];
extern int numLabeledPeers;

#define PAIRING_LED_PIN 2              // Use actual pin you're using
#define LEDC_CHANNEL_0  0              // LEDC channel (0–15)
#define LEDC_TIMER_13_BIT 13           // Resolution (0–8191)
#define LEDC_BASE_FREQ 1000            // Hz

// LED Pattern System (matching client)
enum LedPattern {
    LED_OFF,
    LED_SINGLE_FLASH,
    LED_DOUBLE_FLASH,
    LED_TRIPLE_FLASH,
    LED_FAST_BLINK,
    LED_SOLID_ON,
    LED_FADE
};

extern LedPattern currentLedPattern;
extern unsigned long ledPatternStart;
extern int ledPatternStep;

// Hardware Configuration Variables
extern char deviceName[MAX_PEER_NAME_LEN];

#if HAS_RELAY_OUTPUTS
extern uint8_t relayOutputPins[MAX_RELAY_CHANNELS];
extern uint8_t currentRelayChannel;
#endif

#if HAS_FOOTSWITCH
extern uint8_t footswitchPins[4];
#endif

// Extended server button support
extern uint8_t serverButtonPins[8];    // Up to 8 configurable buttons (including pairing/mode button at index 0)
extern uint8_t serverButtonCount;      // Actual number configured
extern uint8_t serverButtonProgramMap[8]; // Program Change mapping per button (outgoing PC when pressed)

enum LogLevel {
  LOG_NONE = 0,
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG
};

extern LogLevel currentLogLevel;

extern bool footswitchPressed;

// Server MIDI configuration / state (mirrors client semantics)
extern uint8_t serverMidiChannel;              // Selected inbound MIDI channel (1-16, 0 = omni)
extern uint8_t serverMidiChannelMap[MAX_RELAY_CHANNELS]; // Program -> channel mapping (per relay index)
extern bool serverMidiLearnArmed;              // Armed to learn next Program Change
extern int serverMidiLearnTarget;              // Target relay index for learning (-1 none)
extern unsigned long serverMidiLearnStart;     // Timestamp when learn started
extern const unsigned long SERVER_MIDI_LEARN_TIMEOUT; // Timeout ms
extern unsigned long serverMidiLearnCompleteTime; // When last learn finished (cooldown)
extern const unsigned long SERVER_MIDI_LEARN_COOLDOWN; // Cooldown after learn

// Persistence helpers
bool loadServerMidiConfigFromNVS();
void saveServerMidiChannelToNVS();
void saveServerMidiMapToNVS();
void saveServerButtonPcMapToNVS();
bool loadServerButtonPcMapFromNVS();


