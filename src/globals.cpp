
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

#include "globals.h"

uint8_t chan = 4;  // Default channel
volatile bool pairingRequested = false;
bool pairingMode = false;
bool resetMode = false;
bool serialOtaTrigger = false;
PeerInfo labeledPeers[MAX_CLIENTS];
int numLabeledPeers = 0;
LogLevel currentLogLevel = LOG_DEBUG; 
bool footswitchPressed = false;

// Server MIDI state
uint8_t serverMidiChannel = 0; // 0 = omni
uint8_t serverMidiChannelMap[MAX_RELAY_CHANNELS] = {0};
bool serverMidiLearnArmed = false;
int serverMidiLearnTarget = -1;
unsigned long serverMidiLearnStart = 0;
const unsigned long SERVER_MIDI_LEARN_TIMEOUT = 30000; // 30s
unsigned long serverMidiLearnCompleteTime = 0;
const unsigned long SERVER_MIDI_LEARN_COOLDOWN = 750UL; // 0.75s ignore PCs right after learn

// LED Pattern System (matching client)
LedPattern currentLedPattern = LED_OFF;
unsigned long ledPatternStart = 0;
int ledPatternStep = 0;

// Hardware Configuration Variables
char deviceName[MAX_PEER_NAME_LEN] = {0};

#if HAS_RELAY_OUTPUTS
uint8_t relayOutputPins[MAX_RELAY_CHANNELS] = {0}; // Will be set at runtime
uint8_t currentRelayChannel = 0; // No channel active at startup
#endif

#if HAS_FOOTSWITCH
uint8_t footswitchPins[4] = {0}; // Will be set at runtime
#endif

// Extended server button support (defaults: only index 0 active)
uint8_t serverButtonPins[8] = {PAIRING_BUTTON_PIN, 255, 255, 255, 255, 255, 255, 255};
uint8_t serverButtonCount = 1;
uint8_t serverButtonProgramMap[8] = {0,1,2,3,4,5,6,7}; // Default PC numbers

