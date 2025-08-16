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
#include "config.h"
#include "globals.h"
#include <Arduino.h>

// Enhanced logging functions
void log(LogLevel level, const String& msg);
void logf(LogLevel level, const char* format, ...);
void logWithTimestamp(LogLevel level, const String& msg);
void printMAC(const uint8_t *mac, LogLevel level);
void blinkLED(uint8_t pin, int times, int delayMs);
void printStruct(const struct_message& data);

// System status and debug functions
void printSystemStatus();
void printMemoryInfo();
void printNetworkStatus();
void printAmpChannelStatus();
void printPairingStatus();

// Enhanced serial command handling
void checkSerialCommands();
void printHelpMenu();
void handleSerialCommand(const String& cmd);

// Command handler helper functions (broken down from large functions)
bool handleSystemCommands(const String& cmd);
bool handleMIDICommands(const String& cmd);
bool handleControlCommands(const String& cmd);
bool handleTestCommands(const String& cmd);
bool handleDebugCommands(const String& cmd);
bool handleAmpChannelCommands(const String& cmd);
void handlePinCommand();
void showUnknownCommand(const String& cmd);

// Help menu helper functions
void printHelpHeader();
void printSystemCommandsHelp();
void printMIDICommandsHelp();
void printControlCommandsHelp();
void printTestCommandsHelp();
void printDebugCommandsHelp();
void printAmpChannelCommandsHelp();
void printLogLevelsHelp();
void printExamplesHelp();
void printHelpFooter();

// Utility functions - Memory optimized versions
const char* getLogLevelString(LogLevel level);
const char* getPairingStatusString(PairingStatus status);
void getUptimeString(char* buffer, size_t bufferSize);

uint32_t getFreeHeap();
uint32_t getMinFreeHeap();

// Client configuration functions
void printClientConfiguration();

// Pairing helper functions
void resetPairingToDefaults();

void setStatusLedPattern(StatusLedPattern pattern);
void updateStatusLED();
