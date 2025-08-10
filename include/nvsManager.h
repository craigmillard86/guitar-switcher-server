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

// NVS initialization and version management
void checkNVS();
bool initializeNVS();

// Log level management
void saveLogLevelToNVS(LogLevel level);
LogLevel loadLogLevelFromNVS();
void clearLogLevelNVS();

// Server-specific NVS management
void saveServerConfigToNVS();
bool loadServerConfigFromNVS();
void clearServerConfigNVS();

// Peer management (server-specific)
void savePeersToNVS();
void loadPeersFromNVS();
void clearPeersNVS();

// General NVS utilities
void clearAllNVS();
void printNVSStats();
