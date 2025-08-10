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
#include "config.h"

#if HAS_RELAY_OUTPUTS

// Relay control functions
void setRelayChannel(uint8_t channel);
void turnOffAllRelays();
uint8_t getCurrentRelayChannel();

// Relay testing functions
void testRelaySpeed();
void cycleRelays();

// Relay status functions
void printRelayStatus();

#endif

// Always available footswitch functions
void updateFootswitchState();
bool isFootswitchPressed(uint8_t footswitchIndex = 0);
