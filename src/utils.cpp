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
#include <espnow-pairing.h>
#include <dataStructs.h>
#include <commandHandler.h>
#include <config.h>
#include <relayControl.h>
#include <commandSender.h>
#include <cstdarg>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <nvsManager.h>
#include <debug.h>
#include <utils.h>

// External variable declarations
extern unsigned long pairingStartTime;

// Forward declarations for helper functions
const char* getLogLevelString(LogLevel level);
void getUptimeString(char* buffer, size_t bufferSize);

// Forward declarations for command handling functions
void handleSerialCommand(const String& cmd);
bool handleSystemCommands(const String& cmd);
bool handleControlCommands(const String& cmd);
bool handlePairingCommands(const String& cmd);
bool handleRelayCommands(const String& cmd);
bool handleTestCommands(const String& cmd);
bool handleDebugCommands(const String& cmd);
void showUnknownCommand(const String& cmd);

// Forward declarations for help functions
void printHelpMenu();
void printHelpHeader();
void printSystemCommandsHelp();
void printControlCommandsHelp();
void printPairingCommandsHelp();
void printSendCommandsHelp();
void printRelayCommandsHelp();
void printTestCommandsHelp();
void printDebugCommandsHelp();
void printLogLevelsHelp();
void printHelpFooter();

// Forward declarations for remaining utility functions
void printLabeledPeers();

// Forward declarations for help menu functions
void printHelpMenu();
void printHelpHeader();
void printSystemCommandsHelp();
void printControlCommandsHelp();
void printPairingCommandsHelp();
void printRelayCommandsHelp();
void printTestCommandsHelp();
void printDebugCommandsHelp();
void printLogLevelsHelp();
void printHelpFooter();

// Global variables for memory tracking
uint32_t minFreeHeap = UINT32_MAX;

// Utility helper functions for enhanced logging
const char* getLogLevelString(LogLevel level) {
    switch (level) {
        case LOG_NONE:  return "NONE";
        case LOG_ERROR: return "ERROR";
        case LOG_WARN:  return "WARN";
        case LOG_INFO:  return "INFO";
        case LOG_DEBUG: return "DEBUG";
        default:        return "UNKNOWN";
    }
}

void getUptimeString(char* buffer, size_t bufferSize) {
    unsigned long uptime = millis();
    unsigned long seconds = uptime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu", hours, minutes, seconds);
}

// Enhanced logging with timestamps and proper log levels


void log(LogLevel level, const String& msg) {
    if (level <= currentLogLevel) {
        char timestamp[32];
        getUptimeString(timestamp, sizeof(timestamp));
        const char* levelStr = getLogLevelString(level);
        Serial.printf("[%s][%s] %s\n", timestamp, levelStr, msg.c_str());
    }
}

void logf(LogLevel level, const char* format, ...) {
    if (level <= currentLogLevel) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        char timestamp[32];
        getUptimeString(timestamp, sizeof(timestamp));
        const char* levelStr = getLogLevelString(level);
        Serial.printf("[%s][%s] %s\n", timestamp, levelStr, buffer);
    }
}

void logWithTimestamp(LogLevel level, const String& msg) {
    log(level, msg);
}

void printPinConfiguration() {
    logf(LOG_INFO, "=== Pin Configuration ===");
    
#if HAS_RELAY_OUTPUTS
    logf(LOG_INFO, "Relay Output Pins:");
    for (int i = 0; i < MAX_RELAY_CHANNELS && relayOutputPins[i] != 255; i++) {
        logf(LOG_INFO, "  Relay %d: GPIO %d", i + 1, relayOutputPins[i]);
    }
#endif
    
#if HAS_FOOTSWITCH
    logf(LOG_INFO, "Footswitch Input Pins:");
    for (int i = 0; i < 4 && footswitchPins[i] != 255; i++) {
        logf(LOG_INFO, "  Footswitch %d: GPIO %d", i + 1, footswitchPins[i]);
    }
#endif
    
    logf(LOG_INFO, "Pairing LED Pin: GPIO %d", PAIRING_LED_PIN);
    logf(LOG_INFO, "Pairing Button Pin: GPIO %d", PAIRING_BUTTON_PIN);
    logf(LOG_INFO, "========================");
}

void printMAC(const uint8_t* mac, LogLevel level) {
    // Input validation
    if (mac == nullptr) {
        log(LOG_ERROR, "MAC address pointer is null!");
        return;
    }
    
    // Validate log level is within expected range
    if (level < LOG_ERROR || level > LOG_DEBUG) {
        log(LOG_ERROR, "Invalid log level in printMAC");
        return;
    }
    
    if (level <= currentLogLevel) {
        char timestamp[32];
        getUptimeString(timestamp, sizeof(timestamp));
        const char* levelStr = getLogLevelString(level);
        
        // MAC addresses are always 6 bytes - verify this assumption
        Serial.printf("[%s][%s] %02X:%02X:%02X:%02X:%02X:%02X\n", 
                     timestamp, levelStr,
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void readMacAddress(){
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                  baseMac[0], baseMac[1], baseMac[2],
                  baseMac[3], baseMac[4], baseMac[5]);
  } else {
    log(LOG_ERROR, "Failed to read MAC address");
  }
}

// Enhanced serial command handling
void checkSerialCommands() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        handleSerialCommand(cmd);
    }
}

void handleSerialCommand(const String& cmd) {
    if (cmd.isEmpty()) return;
    
    // Check for send commands first (they can have spaces)
    if (cmd.startsWith("send ") || cmd.equalsIgnoreCase("sendhelp")) {
        handleSendCommand(cmd);
        return;
    }
    
    // Try each command category
    if (handleSystemCommands(cmd)) return;
    if (handleControlCommands(cmd)) return;
    if (handlePairingCommands(cmd)) return;
    if (handleRelayCommands(cmd)) return;
    if (handleTestCommands(cmd)) return;
    if (handleDebugCommands(cmd)) return;
    
    // If no command matched, show unknown command message
    showUnknownCommand(cmd);
}

bool handleSystemCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("help")) {
        printHelpMenu();
        return true;
    } else if (cmd.equalsIgnoreCase("status")) {
        printDebugInfo();
        return true;
    } else if (cmd.equalsIgnoreCase("memory")) {
        printMemoryAnalysis();
        return true;
    } else if (cmd.equalsIgnoreCase("network")) {
        printNetworkStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("server")) {
        printServerStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("peers")) {
        printLabeledPeers();
        return true;
    } else if (cmd.equalsIgnoreCase("uptime")) {
        char uptime[32];
        getUptimeString(uptime, sizeof(uptime));
        logf(LOG_INFO, "Uptime: %s", uptime);
        return true;
    } else if (cmd.equalsIgnoreCase("version")) {
        logf(LOG_INFO, "Firmware Version: %s", FIRMWARE_VERSION);
        return true;
    } else if (cmd.equalsIgnoreCase("loglevel")) {
        logf(LOG_INFO, "Current log level: %s (%u)", getLogLevelString(currentLogLevel), (uint8_t)currentLogLevel);
        return true;
    } else if (cmd.equalsIgnoreCase("config")) {
        printServerConfiguration();
        return true;
    } else if (cmd.equalsIgnoreCase("pins")) {
        printPinConfiguration();
        return true;
    }
    return false;
}

bool handleControlCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("restart") || cmd.equalsIgnoreCase("reset")) {
        log(LOG_WARN, "Restarting ESP32...");
        delay(1000);
        ESP.restart();
        return true;
    } else if (cmd.equalsIgnoreCase("ota")) {
        serialOtaTrigger = true;
        log(LOG_INFO, "OTA mode triggered");
        return true;
    } else if (cmd.equalsIgnoreCase("config") || cmd.equalsIgnoreCase("webconfig")) {
        serialConfigTrigger = true;
        log(LOG_INFO, "Web configuration mode triggered");
        return true;
    } else if (cmd.startsWith("setlog")) {
        int level = cmd.substring(6).toInt();
        if (level >= 0 && level <= 4) {
            currentLogLevel = (LogLevel)level;
            saveLogLevelToNVS(currentLogLevel);
            logf(LOG_INFO, "Log level set to: %s", getLogLevelString(currentLogLevel));
        } else {
            log(LOG_WARN, "Invalid log level. Use 0-4 (0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG)");
        }
        return true;
    } else if (cmd.equalsIgnoreCase("clearlog")) {
        clearLogLevelNVS();
        currentLogLevel = LOG_INFO;
        log(LOG_INFO, "Log level reset to default (INFO)");
        return true;
    }   else if (cmd.equalsIgnoreCase("clearall")) {
        log(LOG_WARN, "Clearing ALL NVS data and rebooting...");
        clearAllNVS();
        ESP.restart();
        return true;
    } else if (cmd.equalsIgnoreCase("fspress")) {
        footswitchPressed = true;
        log(LOG_INFO, "Footswitch press simulated");
        return true;
    } 
    return false;
}

bool handlePairingCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("pair")) {
        pairingforceStart();
        log(LOG_INFO, "Pairing mode activated");
        return true;
    } else if (cmd.equalsIgnoreCase("b1")) {
        simulateButton1Press();
        log(LOG_INFO, "Button 1 press simulated");
        return true;
    } else if (cmd.equalsIgnoreCase("b2")) {
        simulateButton2Press();
        log(LOG_INFO, "Button 2 press simulated");
        return true;
    } else if (cmd.equalsIgnoreCase("clearpeers")) {
        log(LOG_INFO, "Clearing all peers from NVS...");
        clearPeersNVS();
        ESP.restart();
        return true;
    } else if (cmd.equalsIgnoreCase("pairing")) {
        printPairingStatus();
        return true;
    }
    return false;
}

bool handleRelayCommands(const String& cmd) {
#if HAS_RELAY_OUTPUTS
    if (cmd.equalsIgnoreCase("relay")) {
        printRelayStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("off")) {
        turnOffAllRelays();
        log(LOG_INFO, "All relay channels turned off");
        return true;
    } else if (cmd.startsWith("ch")) {
        // Handle ch1, ch2, ch3, ch4 commands
        int channel = cmd.substring(2).toInt();
        if (channel >= 1 && channel <= MAX_RELAY_CHANNELS) {
            setRelayChannel(channel);
            return true;
        } else {
            logf(LOG_WARN, "Invalid channel: %d (valid: 1-%d)", channel, MAX_RELAY_CHANNELS);
            return true;
        }
    } else if (cmd.equalsIgnoreCase("cycle")) {
        cycleRelays();
        log(LOG_INFO, "Relay cycle test completed");
        return true;
    } else if (cmd.equalsIgnoreCase("speed")) {
        testRelaySpeed();
        return true;
    }
#endif
    return false;
}

bool handleTestCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("testmemory")) {
        log(LOG_INFO, "Running memory test...");
        printMemoryAnalysis();
        return true;
    }
    return false;
}

bool handleDebugCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("debug")) {
        printDebugInfo();
        return true;
    } else if (cmd.equalsIgnoreCase("debugperf")) {
        printPerformanceMetrics();
        return true;
    } else if (cmd.equalsIgnoreCase("debugmemory")) {
        printMemoryAnalysis();
        return true;
    } else if (cmd.equalsIgnoreCase("debugwifi")) {
        printWiFiStats();
        return true;
    } else if (cmd.equalsIgnoreCase("debugespnow")) {
        printESPNowStats();
        return true;
    } else if (cmd.equalsIgnoreCase("debugnvs")) {
        printNVSStats();
        return true;
    } else if (cmd.equalsIgnoreCase("debugreset")) {
        resetPerformanceMetrics();
        return true;
    }
    return false;
}

void showUnknownCommand(const String& cmd) {
    logf(LOG_WARN, "Unknown command: '%s'", cmd.c_str());
    log(LOG_INFO, "Type 'help' for available commands");
}


const char* getPeerName(const uint8_t *mac) {
  logf(LOG_DEBUG, "Number of labeled peers: %d", numLabeledPeers);
    for (int i = 0; i < numLabeledPeers; i++) {
        if (memcmp(mac, labeledPeers[i].mac, 6) == 0) {
            log(LOG_DEBUG, "getPeerName: ");
            printMAC(labeledPeers[i].mac, LOG_DEBUG);
            return labeledPeers[i].name;
        }
    }
    return "Unknown";
}

uint8_t* getPeerMacByName(const char* name) {
  for (int i = 0; i < numLabeledPeers; i++) {
    if (strcmp(labeledPeers[i].name, name) == 0) {
      return labeledPeers[i].mac;
    }
  }
  return nullptr;
}

bool addLabeledPeer(const uint8_t *mac, const char *name) {
      // Check if MAC is already in the list
    for (int i = 0; i < numLabeledPeers; i++) {
        if (memcmp(mac, labeledPeers[i].mac, 6) == 0) {
            // Optional: Update the name if it's changed
            strncpy(labeledPeers[i].name, name, MAX_PEER_NAME_LEN);
            return false;  // Don't add duplicate
        }
    }

    if (numLabeledPeers >= MAX_CLIENTS) return false;
    memcpy(labeledPeers[numLabeledPeers].mac, mac, 6);
    strncpy(labeledPeers[numLabeledPeers].name, name, MAX_PEER_NAME_LEN);
    logf(LOG_DEBUG, "Added labelled Peer: %s", name);
    numLabeledPeers++;
    return true;
}

void printLabeledPeers() {
    log(LOG_INFO, "----- Registered Peers -----");
    for (int i = 0; i < numLabeledPeers; i++) {
        logf(LOG_INFO, "Peer %d: ", i);
        printMAC(labeledPeers[i].mac, LOG_INFO);
        logf(LOG_INFO, " - %s", labeledPeers[i].name);
    }
    log(LOG_INFO, "----------------------------");
}

// Enhanced help system
void printHelpMenu() {
    printHelpHeader();
    printSystemCommandsHelp();
    printControlCommandsHelp();
    printPairingCommandsHelp();
    printSendCommandsHelp();
#if HAS_RELAY_OUTPUTS
    printRelayCommandsHelp();
#endif
    printTestCommandsHelp();
    printDebugCommandsHelp();
    printLogLevelsHelp();
    printHelpFooter();
}

void printHelpHeader() {
    Serial.println(F("\n========== ESP SERVER COMMANDS =========="));
}

void printSystemCommandsHelp() {
    Serial.println(F("SYSTEM COMMANDS:"));
    Serial.println(F("  help        : Show this help menu"));
    Serial.println(F("  status      : Show complete system status"));
    Serial.println(F("  memory      : Show memory usage"));
    Serial.println(F("  network     : Show network status"));
    Serial.println(F("  server      : Show server status"));
    Serial.println(F("  peers       : Show registered peers"));
    Serial.println(F("  uptime      : Show system uptime"));
    Serial.println(F("  version     : Show firmware version"));
    Serial.println(F("  loglevel    : Show current log level"));
    Serial.println(F("  config      : Show server configuration"));
    Serial.println(F("  pins        : Show pin assignments"));
    Serial.println(F(""));
}

void printControlCommandsHelp() {
    Serial.println(F("CONTROL COMMANDS:"));
    Serial.println(F("  restart     : Reboot the device"));
    Serial.println(F("  ota         : Enter OTA update mode"));
    Serial.println(F("  webconfig   : Enter web configuration mode"));
    Serial.println(F("  setlogN     : Set log level (N=0-4)"));
    Serial.println(F("  clearlog    : Clear saved log level (reset to default)"));
    Serial.println(F("  clearall    : Clear ALL NVS data (factory reset)"));
    Serial.println(F("  fspress     : Simulate footswitch press"));
    Serial.println(F(""));
}

void printPairingCommandsHelp() {
    Serial.println(F("PAIRING COMMANDS:"));
    Serial.println(F("  pair        : Start pairing mode"));
    Serial.println(F("  clearpeers  : Clear all peers from NVS"));
    Serial.println(F("  pairing     : Show pairing status"));
    Serial.println(F(""));
}

void printSendCommandsHelp() {
    Serial.println(F("SEND COMMANDS (to paired clients):"));
    Serial.println(F("  send channel <0-4>           : Send channel change to all clients"));
    Serial.println(F("  send channel <0-4> <client>  : Send channel change to specific client"));
    Serial.println(F("  send off                     : Turn off all channels on all clients"));
    Serial.println(F("  send off <client>            : Turn off all channels on specific client"));
    Serial.println(F("  send raw <type> <value>      : Send raw command to all clients"));
    Serial.println(F("  send status                  : Show paired clients"));
    Serial.println(F("  send help                    : Show detailed send command help"));
    Serial.println(F("  sendhelp                     : Show send command help"));
    Serial.println(F(""));
}

void printRelayCommandsHelp() {
#if HAS_RELAY_OUTPUTS
    Serial.println(F("RELAY COMMANDS:"));
    Serial.println(F("  relay       : Show relay status"));
    Serial.println(F("  off         : Turn off all relays"));
    for (int i = 1; i <= MAX_RELAY_CHANNELS; i++) {
        Serial.printf("  ch%d         : Activate relay channel %d\n", i, i);
    }
    Serial.println(F("  cycle       : Cycle through all relays"));
    Serial.println(F("  speed       : Test relay switching speed"));
    Serial.println(F(""));
#endif
}

void printTestCommandsHelp() {
    Serial.println(F("TEST COMMANDS:"));
    Serial.println(F("  testmemory  : Run memory test"));
    Serial.println(F(""));
}

void printDebugCommandsHelp() {
    Serial.println(F("DEBUG COMMANDS:"));
    Serial.println(F("  debug       : Show complete debug info"));
    Serial.println(F("  debugperf   : Show performance metrics"));
    Serial.println(F("  debugmemory : Show memory analysis"));
    Serial.println(F("  debugwifi   : Show WiFi stats"));
    Serial.println(F("  debugespnow : Show ESP-NOW stats"));
    Serial.println(F("  debugnvs    : Show NVS statistics"));
    Serial.println(F("  debugreset  : Reset performance metrics"));
    Serial.println(F(""));
}

void printLogLevelsHelp() {
    Serial.println(F("LOG LEVELS:"));
    Serial.println(F("  0 = OFF     : No logging"));
    Serial.println(F("  1 = ERROR   : Error messages only"));
    Serial.println(F("  2 = WARN    : Warnings and errors"));
    Serial.println(F("  3 = INFO    : Info, warnings, and errors (default)"));
    Serial.println(F("  4 = DEBUG   : All messages including debug"));
    Serial.println(F(""));
}

void printHelpFooter() {
    Serial.println(F("Examples:"));
    Serial.println(F("  setlog3     : Set log level to INFO"));
    Serial.println(F("  setlog4     : Set log level to DEBUG"));
    Serial.println(F("=====================================\n"));
}
