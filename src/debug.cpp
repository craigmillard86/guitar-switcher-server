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
#include <Arduino.h>
#include <globals.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <utils.h>
#include <debug.h>
#include <nvsManager.h>

// Global variables for memory tracking
extern uint32_t minFreeHeap;

// Performance metrics (matching client structure)
PerformanceMetrics perfMetrics = {0, 0, 0, ULONG_MAX, 0, 0};

// Debug and monitoring functions (matching client naming conventions)
void printDebugInfo() {
    log(LOG_INFO, "=== DEBUG INFORMATION ===");
    printSystemStatus();
    printPerformanceMetrics();
    printMemoryAnalysis();
    printWiFiStats();
    printESPNowStats();
    printNVSStats();
    log(LOG_INFO, "========================");
}

void printPerformanceMetrics() {
    log(LOG_INFO, "=== PERFORMANCE METRICS ===");
    
    char uptime[32];
    getUptimeString(uptime, sizeof(uptime));
    logf(LOG_INFO, "Uptime: %s", uptime);
    
    logf(LOG_INFO, "Loop Count: %lu", perfMetrics.loopCount);
    if (perfMetrics.loopCount > 0) {
        logf(LOG_INFO, "Last Loop Time: %lu ms", perfMetrics.lastLoopTime);
        logf(LOG_INFO, "Max Loop Time: %lu ms", perfMetrics.maxLoopTime);
        logf(LOG_INFO, "Min Loop Time: %lu ms", perfMetrics.minLoopTime);
        logf(LOG_INFO, "Avg Loop Time: %lu ms", perfMetrics.totalLoopTime / perfMetrics.loopCount);
    }
    
    logf(LOG_INFO, "CPU Frequency: %u MHz", getCpuFrequencyMhz());
    logf(LOG_INFO, "Flash Size: %u bytes", ESP.getFlashChipSize());
    logf(LOG_INFO, "Free Heap: %u bytes", getFreeHeap());
    
    log(LOG_INFO, "==========================");
}

void printMemoryAnalysis() {
    log(LOG_INFO, "=== MEMORY ANALYSIS ===");
    
    uint32_t freeHeap = getFreeHeap();
    uint32_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t usedHeap = totalHeap - freeHeap;
    float usagePercent = (float)usedHeap / totalHeap * 100.0;
    
    logf(LOG_INFO, "Total Heap: %u bytes", totalHeap);
    logf(LOG_INFO, "Used Heap: %u bytes (%.1f%%)", usedHeap, usagePercent);
    logf(LOG_INFO, "Free Heap: %u bytes", freeHeap);
    logf(LOG_INFO, "Min Free Heap: %u bytes", minFreeHeap);
    
    // Additional heap analysis
    logf(LOG_INFO, "Largest Free Block: %u bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    
    // Memory leak detection
    if (minFreeHeap < (totalHeap * 0.2)) { // Less than 20% free at minimum
        log(LOG_WARN, "Potential memory leak detected - very low minimum free heap");
    }
    
    log(LOG_INFO, "======================");
}

void printWiFiStats() {
    log(LOG_INFO, "=== WIFI STATISTICS ===");
    
    logf(LOG_INFO, "WiFi Mode: %d", WiFi.getMode());
    logf(LOG_INFO, "Current Channel: %u", chan);
    
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        log(LOG_INFO, "MAC Address:");
        printMAC(baseMac, LOG_INFO);
    } else {
        log(LOG_ERROR, "Failed to read MAC address");
    }
    
    // WiFi power and sleep info
    wifi_ps_type_t ps_type;
    if (esp_wifi_get_ps(&ps_type) == ESP_OK) {
        logf(LOG_INFO, "Power Save Mode: %d", ps_type);
    }
    
    log(LOG_INFO, "======================");
}

void printESPNowStats() {
    log(LOG_INFO, "=== ESP-NOW STATISTICS ===");
    
    logf(LOG_INFO, "Connected Peers: %d/%d", numLabeledPeers, MAX_CLIENTS);
    logf(LOG_INFO, "Total Clients: %d", numClients);
    logf(LOG_INFO, "Pairing Mode: %s", pairingMode ? "ENABLED" : "DISABLED");
    
    if (pairingMode) {
        extern unsigned long pairingStartTime;
        unsigned long elapsed = millis() - pairingStartTime;
        unsigned long remaining = (PAIRING_TIMEOUT_MS > elapsed) ? (PAIRING_TIMEOUT_MS - elapsed) / 1000 : 0;
        logf(LOG_INFO, "Pairing Timeout: %lu seconds remaining", remaining);
    }
    
    logf(LOG_INFO, "Footswitch Status: %s", footswitchPressed ? "PRESSED" : "RELEASED");
    logf(LOG_INFO, "OTA Trigger: %s", serialOtaTrigger ? "ACTIVE" : "INACTIVE");
    
    log(LOG_INFO, "==========================");
}

// System status and monitoring functions
void printSystemStatus() {
    log(LOG_INFO, "=== SYSTEM STATUS ===");
    logf(LOG_INFO, "Firmware Version: %s", FIRMWARE_VERSION);
    
    char uptime[32];
    getUptimeString(uptime, sizeof(uptime));
    logf(LOG_INFO, "Uptime: %s", uptime);
    
    printMemoryInfo();
    printNetworkStatus();
    printServerStatus();
    printPairingStatus();
    log(LOG_INFO, "===================");
}

void printMemoryInfo() {
    uint32_t freeHeap = getFreeHeap();
    uint32_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    uint32_t usedHeap = totalHeap - freeHeap;
    float usagePercent = (float)usedHeap / totalHeap * 100.0;
    
    logf(LOG_INFO, "Memory - Free: %uB, Used: %uB (%.1f%%)", freeHeap, usedHeap, usagePercent);
    logf(LOG_INFO, "Min Free Heap: %uB", minFreeHeap);
}

void printNetworkStatus() {
    logf(LOG_INFO, "WiFi Mode: %d", WiFi.getMode());
    logf(LOG_INFO, "Current Channel: %u", chan);
    log(LOG_INFO, "Server MAC Address:");
    
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        printMAC(baseMac, LOG_INFO);
    } else {
        log(LOG_ERROR, "Failed to read MAC address");
    }
}

void printServerStatus() {
    logf(LOG_INFO, "Connected Peers: %d/%d", numLabeledPeers, MAX_CLIENTS);
    logf(LOG_INFO, "Total Clients: %d", numClients);
    logf(LOG_INFO, "Footswitch Status: %s", footswitchPressed ? "PRESSED" : "RELEASED");
    logf(LOG_INFO, "OTA Trigger: %s", serialOtaTrigger ? "ACTIVE" : "INACTIVE");
}

void printPairingStatus() {
    logf(LOG_INFO, "Pairing Mode: %s", pairingMode ? "ENABLED" : "DISABLED");
    if (pairingMode) {
        extern unsigned long pairingStartTime;
        unsigned long elapsed = millis() - pairingStartTime;
        unsigned long remaining = (PAIRING_TIMEOUT_MS > elapsed) ? (PAIRING_TIMEOUT_MS - elapsed) / 1000 : 0;
        logf(LOG_INFO, "Pairing Timeout: %lu seconds remaining", remaining);
    }
}

// Memory monitoring functions
void updateMemoryStats() {
    uint32_t currentFree = esp_get_free_heap_size();
    if (currentFree < minFreeHeap) {
        minFreeHeap = currentFree;
    }
}

uint32_t getFreeHeap() {
    uint32_t freeHeap = esp_get_free_heap_size();
    if (freeHeap < minFreeHeap) {
        minFreeHeap = freeHeap;
    }
    return freeHeap;
}

uint32_t getMinFreeHeap() {
    return minFreeHeap;
}

// Performance monitoring functions (matching client)
void initializePerformanceMetrics() {
    perfMetrics.loopCount = 0;
    perfMetrics.lastLoopTime = 0;
    perfMetrics.maxLoopTime = 0;
    perfMetrics.minLoopTime = ULONG_MAX;
    perfMetrics.totalLoopTime = 0;
    perfMetrics.startTime = millis();
    log(LOG_DEBUG, "Performance metrics initialized");
}

void updatePerformanceMetrics(unsigned long loopTime) {
    perfMetrics.loopCount++;
    perfMetrics.lastLoopTime = loopTime;
    perfMetrics.totalLoopTime += loopTime;
    
    if (loopTime > perfMetrics.maxLoopTime) {
        perfMetrics.maxLoopTime = loopTime;
    }
    
    if (loopTime < perfMetrics.minLoopTime) {
        perfMetrics.minLoopTime = loopTime;
    }
    
    // Update memory stats as well
    updateMemoryStats();
}

void resetPerformanceMetrics() {
    perfMetrics.loopCount = 0;
    perfMetrics.lastLoopTime = 0;
    perfMetrics.maxLoopTime = 0;
    perfMetrics.minLoopTime = ULONG_MAX;
    perfMetrics.totalLoopTime = 0;
    perfMetrics.startTime = millis();
    log(LOG_INFO, "Performance metrics reset");
}

// Debug command handling (simplified version of client's)
void handleDebugCommand(const char* cmd) {
    if (strcmp(cmd, "debug") == 0) {
        printDebugInfo();
    } else if (strcmp(cmd, "debugperf") == 0) {
        printPerformanceMetrics();
    } else if (strcmp(cmd, "debugmemory") == 0) {
        printMemoryAnalysis();
    } else if (strcmp(cmd, "debugwifi") == 0) {
        printWiFiStats();
    } else if (strcmp(cmd, "debugespnow") == 0) {
        printESPNowStats();
    } else if (strcmp(cmd, "debugnvs") == 0) {
        printNVSStats();
    } else if (strcmp(cmd, "debugreset") == 0) {
        resetPerformanceMetrics();
    } else {
        logf(LOG_WARN, "Unknown debug command: %s", cmd);
        printDebugHelp();
    }
}

void printDebugHelp() {
    log(LOG_INFO, "=== DEBUG COMMANDS ===");
    log(LOG_INFO, "debug       - Complete debug info");
    log(LOG_INFO, "debugperf   - Performance metrics");
    log(LOG_INFO, "debugmemory - Memory analysis");
    log(LOG_INFO, "debugwifi   - WiFi statistics");
    log(LOG_INFO, "debugespnow - ESP-NOW statistics");
    log(LOG_INFO, "debugnvs    - NVS statistics");
    log(LOG_INFO, "debugreset  - Reset performance metrics");
    log(LOG_INFO, "=====================");
}
