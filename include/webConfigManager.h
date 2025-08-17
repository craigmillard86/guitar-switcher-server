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
#include <WebServer.h>

// Configuration mode trigger
bool checkConfigTrigger();
void startConfigurationMode();
void startConfigurationAP();

// Web server handlers
void handleConfigRoot();
void handleConfigAPI();
void handleConfigSave();
void handleConfigLoad();
void handleConfigTest();
void handleConfigExport();
void handleConfigImport();
void handleSystemStatus();
void handleReboot();

// Configuration mode states
enum ConfigMode {
    CONFIG_MODE_DISABLED = 0,
    CONFIG_MODE_ACTIVE = 1,
    CONFIG_MODE_TESTING = 2
};

extern ConfigMode currentConfigMode;
extern WebServer configServer;
