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

// Command sending functions
bool sendCommandToClient(const uint8_t* clientMac, uint8_t commandType, uint8_t commandValue);
bool sendCommandToAllClients(uint8_t commandType, uint8_t commandValue);

// Specific command helpers
bool sendChannelChange(const uint8_t* clientMac, uint8_t channel);
bool sendChannelChangeToAll(uint8_t channel);
bool sendAllChannelsOff(const uint8_t* clientMac);
bool sendAllChannelsOffToAll();
bool sendStatusRequest(const uint8_t* clientMac);
bool sendStatusRequestToAll();

// MIDI forwarding: broadcast a MIDI Program Change (program 0-127 will be interpreted by clients)
bool forwardMidiProgramToAll(uint8_t programNumber);

// Serial command interface
void handleSendCommand(const String& cmd);
void printSendCommandHelp();
