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

// Debug and monitoring functions
void printDebugInfo();
void printPerformanceMetrics();
void printTaskStats();
void printWiFiStats();
void printESPNowStats();

// Memory monitoring
void updateMemoryStats();
void printMemoryLeakInfo();

// Performance monitoring
struct PerformanceMetrics {
    unsigned long loopCount;
    unsigned long lastLoopTime;
    unsigned long maxLoopTime;
    unsigned long minLoopTime;
    unsigned long totalLoopTime;
    unsigned long startTime;
};

extern PerformanceMetrics perfMetrics;

// Debug commands
void handleDebugCommand(const char* cmd);
void printDebugHelp();

// Performance monitoring functions
void updatePerformanceMetrics(unsigned long loopTime); 