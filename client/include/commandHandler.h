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
#include "dataStructs.h"
#include <Arduino.h>

void handleCommand(uint8_t commandType, uint8_t value);
void setAmpChannel(uint8_t channel);
void checkAmpChannelButtons();
void handleProgramChange(byte midiChannel, byte program);

// Helper functions for button processing (broken down from large functions)
bool handleMidiLearnTimeout();
void processButtonState(int buttonIndex, uint8_t reading, 
                       unsigned long* lastDebounceTime, uint8_t* lastButtonState,
                       bool* buttonPressed, unsigned long* buttonPressStart,
                       bool* buttonLongPressHandled);
void handleButtonPress(int buttonIndex, bool* buttonPressed, unsigned long* buttonPressStart,
                      bool* buttonLongPressHandled);
void handleButtonHeld(int buttonIndex, unsigned long held);
void handleButtonRelease(int buttonIndex, unsigned long held, bool* buttonPressed,
                        bool* buttonLongPressHandled);

// Shared button functions
void resetMilestoneFlags();
void handleLedFeedback(unsigned long held, const char* buttonName);
void enterChannelSelectMode();
void handleChannelSelection();
void handleChannelSelectAutoSave();
void showChannelConfirmation(uint8_t channel);
void updateChannelConfirmation();