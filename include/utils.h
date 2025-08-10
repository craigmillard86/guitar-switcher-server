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
#include <globals.h>

// Enhanced logging functions
void log(LogLevel level, const String& msg);
void logf(LogLevel level, const char* format, ...);
void logWithTimestamp(LogLevel level, const String& msg);
void printMAC(const uint8_t* mac, LogLevel level);

// System utilities
void readMacAddress();
void checkSerialCommands();
void handleSerialCommand(const String& cmd);

// Peer management functions
const char* getPeerName(const uint8_t *mac);
uint8_t* getPeerMacByName(const char* name);
bool addLabeledPeer(const uint8_t *mac, const char *name);
void printLabeledPeers();

// Help menu functions
void printHelpMenu();

// Configuration display functions
void printPinConfiguration();

// Utility helper functions
const char* getLogLevelString(LogLevel level);
void getUptimeString(char* buffer, size_t bufferSize);
