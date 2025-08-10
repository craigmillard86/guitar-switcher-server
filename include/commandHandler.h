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

// Button control functions
void checkPairingButtons();
void handleLedFeedback(unsigned long held, const char* buttonName);
void updateLedPatterns();

// Button simulation functions
void simulateButton1Press();
void simulateButton2Press();

// Helper functions for button processing
void processButtonState(int buttonIndex, uint8_t reading,
                       unsigned long* lastDebounceTime, uint8_t* lastButtonState,
                       bool* buttonPressed, unsigned long* buttonPressStart,
                       bool* buttonLongPressHandled);

void handleButtonPress(int buttonIndex, bool* buttonPressed, unsigned long* buttonPressStart,
                      bool* buttonLongPressHandled);

void handleButtonHeld(int buttonIndex, unsigned long held);

void handleButtonRelease(int buttonIndex, unsigned long held, bool* buttonPressed,
                        bool* buttonLongPressHandled);
