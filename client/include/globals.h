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
#include "pairing.h"
// NOTE: If you need config macros, include config.h BEFORE globals.h in your source file.
#include "dataStructs.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define FIRMWARE_VERSION "1.0.0"
#define STORAGE_VERSION 1

extern bool serialOtaTrigger;

#define BOARD_ID 1

extern uint8_t currentAmpChannel;
extern uint8_t ampSwitchPins[MAX_AMPSWITCHS];
extern uint8_t ampButtonPins[MAX_AMPSWITCHS];

// PairingStatus now in pairing.h

enum LogLevel {
  LOG_NONE = 0,
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG
};

extern LogLevel currentLogLevel;

extern uint8_t serverAddress[6];
extern uint8_t clientMacAddress[6];
extern esp_now_peer_info_t peer;
extern struct_message myData;
extern struct_message inData;
extern struct_pairing pairingData;
extern uint8_t currentChannel;
extern bool paired;
extern char deviceName[MAX_PEER_NAME_LEN];
extern bool newDataReceived;
extern bool otaModeRequested;
extern bool enableButtonChecking;

enum StatusLedPattern {
  LED_OFF,
  LED_SINGLE_FLASH,
  LED_DOUBLE_FLASH,
  LED_TRIPLE_FLASH,
  LED_QUAD_FLASH,
  LED_PENTA_FLASH,
  LED_HEXA_FLASH,
  LED_FAST_BLINK,
  LED_SOLID_ON,
  LED_FADE,
  LED_PAIRING, // Alias for fade
  LED_OTA_BLINK // Alias for fast blink
};

extern volatile StatusLedPattern currentLedPattern;
extern volatile unsigned long ledPatternStart;
extern volatile int ledPatternStep;

// LED test mode - when active, prevents data reception from overriding LED patterns
extern unsigned long ledTestModeEnd;

extern bool midiLearnArmed;
extern int midiLearnChannel;
extern uint8_t midiChannelMap[MAX_AMPSWITCHS];
extern uint8_t currentMidiChannel;

extern const unsigned long MIDI_LEARN_TIMEOUT; // 30 seconds
