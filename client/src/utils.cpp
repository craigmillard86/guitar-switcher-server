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
#include "utils.h"
#include <espnow-pairing.h>
#include <commandHandler.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "debug.h"
#include "nvsManager.h"

extern unsigned long lastMemoryCheck;

// Global variables for memory tracking
uint32_t minFreeHeap = UINT32_MAX;
// unsigned long lastMemoryCheck = 0; // Removed to avoid multiple definition

// Enhanced logging with timestamps and proper log levels - Memory optimized
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

void blinkLED(uint8_t pin, int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(pin, HIGH);
        delay(delayMs);
        digitalWrite(pin, LOW);
        delay(delayMs);
    }
}

// System status functions
void printSystemStatus() {
    log(LOG_INFO, "=== SYSTEM STATUS ===");
    logf(LOG_INFO, "Firmware Version: %s", FIRMWARE_VERSION);
    logf(LOG_INFO, "Board ID: %d", BOARD_ID);
    
    char uptime[32];
    getUptimeString(uptime, sizeof(uptime));
    logf(LOG_INFO, "Uptime: %s", uptime);
    
    printMemoryInfo();
    printNetworkStatus();
    printAmpChannelStatus();
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
    logf(LOG_INFO, "Current Channel: %u", currentChannel);
    log(LOG_INFO, "Client MAC: ");
    printMAC(clientMacAddress, LOG_INFO);
    log(LOG_INFO, "Server MAC: ");
    printMAC(serverAddress, LOG_INFO);
}

void printAmpChannelStatus() {
    logf(LOG_INFO, "Current Amp Channel: %u", currentAmpChannel);
    logf(LOG_INFO, "Channel Pins: %u,%u,%u,%u", 
         ampSwitchPins[0], ampSwitchPins[1], ampSwitchPins[2], ampSwitchPins[3]);
    logf(LOG_INFO, "Button Pins: %u,%u,%u,%u", 
         ampButtonPins[0], ampButtonPins[1], ampButtonPins[2], ampButtonPins[3]);
}

void printPairingStatus() {
    logf(LOG_INFO, "Pairing Status: %s", getPairingStatusString(pairingStatus));
}

// Enhanced serial command handling
void checkSerialCommands() {
    if (midiLearnChannel >= 0) return; // Block serial commands during MIDI Learn lockout
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        handleSerialCommand(cmd);
    }
}

// Forward declarations for command handler helper functions
bool handleSystemCommands(const String& cmd);
bool handleMIDICommands(const String& cmd);
bool handleControlCommands(const String& cmd);
bool handleTestCommands(const String& cmd);
bool handleDebugCommands(const String& cmd);
bool handleAmpChannelCommands(const String& cmd);
void handlePinCommand();
void showUnknownCommand(const String& cmd);

void handleSerialCommand(const String& cmd) {
    if (cmd.isEmpty()) return;

    // Removed automatic LED flash to prevent interference with test patterns
    
    // Try each command category
    if (handleSystemCommands(cmd)) return;
    if (handleMIDICommands(cmd)) return;
    if (handleControlCommands(cmd)) return;
    if (handleTestCommands(cmd)) return;
    if (handleDebugCommands(cmd)) return;
    if (handleAmpChannelCommands(cmd)) return;
    
    // If no command matched, show unknown command message
    showUnknownCommand(cmd);
}

bool handleSystemCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("help")) {
        printHelpMenu();
        return true;
    } else if (cmd.equalsIgnoreCase("status")) {
        printSystemStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("memory")) {
        printMemoryInfo();
        return true;
    } else if (cmd.equalsIgnoreCase("network")) {
        printNetworkStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("amp")) {
        printAmpChannelStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("pairing")) {
        printPairingStatus();
        return true;
    } else if (cmd.equalsIgnoreCase("pins")) {
        handlePinCommand();
        return true;
    } else if (cmd.equalsIgnoreCase("uptime")) {
        char uptime[32];
        getUptimeString(uptime, sizeof(uptime));
        logf(LOG_INFO, "Uptime: %s", uptime);
        return true;
    } else if (cmd.equalsIgnoreCase("version")) {
        logf(LOG_INFO, "Firmware Version: %s", FIRMWARE_VERSION);
        logf(LOG_INFO, "Storage Version: %d", STORAGE_VERSION);
        return true;
    } else if (cmd.equalsIgnoreCase("buttons")) {
        enableButtonChecking = !enableButtonChecking;
        logf(LOG_INFO, "Button checking %s", enableButtonChecking ? "enabled" : "disabled");
        return true;
    } else if (cmd.equalsIgnoreCase("loglevel")) {
        logf(LOG_INFO, "Current log level: %s (%u)", getLogLevelString(currentLogLevel), (uint8_t)currentLogLevel);
        return true;
    } else if (cmd.equalsIgnoreCase("config")) {
        printClientConfiguration();
        return true;
    }
    return false;
}

bool handleMIDICommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("midi")) {
        log(LOG_INFO, "=== MIDI INFORMATION ===");
        logf(LOG_INFO, "  Current MIDI Channel: %u (persistent, set via channel select mode)", currentMidiChannel);
        log(LOG_INFO, "  MIDI Thru: Enabled");
        logf(LOG_INFO, "  MIDI Pins - RX: %u, TX: %u", MIDI_RX_PIN, MIDI_TX_PIN);
        log(LOG_INFO, "  Program Change Mapping:");
        for (int i = 0; i < MAX_AMPSWITCHS; i++) {
            logf(LOG_INFO, "    Button %d: PC#%u", i+1, midiChannelMap[i]);
        }
        log(LOG_INFO, "  (Use 'chset' to change MIDI channel, 'midimap' for detailed mapping)");
        return true;
    } else if (cmd.equalsIgnoreCase("midimap")) {
        log(LOG_INFO, "=== MIDI PROGRAM CHANGE MAP ===");
        for (int i = 0; i < MAX_AMPSWITCHS; i++) {
            logf(LOG_INFO, "Button %d: PC#%u", i+1, midiChannelMap[i]);
        }
        log(LOG_INFO, "==============================");
        return true;
    } else if (cmd.equalsIgnoreCase("ch")) {
        logf(LOG_INFO, "Current MIDI Channel: %u (persistent, set via channel select mode)", currentMidiChannel);
        return true;
    } else if (cmd.equalsIgnoreCase("chset")) {
        log(LOG_INFO, "To change MIDI channel: Hold Button 1 for 15s to enter channel select mode, then press to increment channel. Auto-saves after 10s of inactivity.");
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
    } else if (cmd.equalsIgnoreCase("pair")) {
        clearPairingNVS();
        resetPairingToDefaults();
        pairingStatus = PAIR_REQUEST;
        log(LOG_INFO, "Re-pairing requested! Starting discovery from channel 1...");
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
    } else if (cmd.equalsIgnoreCase("clearall")) {
        log(LOG_WARN, "Clearing all NVS data...");
        clearPairingNVS();
        clearLogLevelNVS();
        currentLogLevel = LOG_INFO;
        resetPairingToDefaults();
        log(LOG_INFO, "All NVS data cleared - pairing and log level reset to defaults");
        return true;
    }
    return false;
}

bool handleTestCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("testled")) {
        log(LOG_INFO, "Testing status LED...");
        setStatusLedPattern(LED_TRIPLE_FLASH);
        return true;
    } else if (cmd.equalsIgnoreCase("testpairing")) {
        log(LOG_INFO, "Testing pairing LED...");
        for (int i = 0; i < 5; i++) {
            ledcWrite(LEDC_CHANNEL_0, 512);
            delay(100);
            ledcWrite(LEDC_CHANNEL_0, 0);
            delay(100);
        }
        return true;
    } else if (cmd.equalsIgnoreCase("testbuttons")) {
        log(LOG_INFO, "=== BUTTON TEST ===");
        logf(LOG_INFO, "Button checking enabled: %s", enableButtonChecking ? "YES" : "NO");
        log(LOG_INFO, "Current button states:");
        for (int i = 0; i < MAX_AMPSWITCHS; i++) {
            uint8_t state = digitalRead(ampButtonPins[i]);
            logf(LOG_INFO, "  Button %d (pin %u): %s", i+1, ampButtonPins[i], state ? "HIGH" : "LOW");
        }
        log(LOG_INFO, "==================");
        return true;
    } else if (cmd.equalsIgnoreCase("forcepair")) {
        log(LOG_INFO, "=== FORCING PAIRING MODE ===");
        clearPairingNVS();
        resetPairingToDefaults();
        pairingStatus = PAIR_REQUEST;
        setStatusLedPattern(LED_FADE);
        log(LOG_INFO, "Pairing mode forced - LED should fade");
        return true;
    }
    return false;
}

bool handleDebugCommands(const String& cmd) {
    if (cmd.startsWith("debug")) {
        String debugCmd = cmd.substring(5);
        debugCmd.trim();
        if (debugCmd.isEmpty()) {
            debugCmd = "debug";
        }
        handleDebugCommand(debugCmd.c_str());
        return true;
    }
    return false;
}

bool handleAmpChannelCommands(const String& cmd) {
    if (cmd.equalsIgnoreCase("off")) {
        setAmpChannel(0);
        log(LOG_INFO, "All amp channels turned off");
        return true;
    } else if (cmd.equalsIgnoreCase("speed")) {
        // Speed test for relay switching
        log(LOG_INFO, "=== RELAY SPEED TEST ===");
        
        unsigned long startTime = micros();
        setAmpChannel(1);
        unsigned long time1 = micros();
        setAmpChannel(0);
        unsigned long time2 = micros();
        setAmpChannel(1);
        unsigned long endTime = micros();
        
        logf(LOG_INFO, "Switch ON time: %lu us", time1 - startTime);
        logf(LOG_INFO, "Switch OFF time: %lu us", time2 - time1);
        logf(LOG_INFO, "Total cycle time: %lu us", endTime - startTime);
        logf(LOG_INFO, "Average per switch: %lu us", (endTime - startTime) / 3);
        
        #ifdef FAST_SWITCHING
        log(LOG_INFO, "Mode: Ultra-Fast (Direct register access)");
        #else
        log(LOG_INFO, "Mode: Standard (digitalWrite)");
        #endif
        
        return true;
    } else if (cmd.equalsIgnoreCase("test")) {
        // Test command to manually toggle the relay
        log(LOG_INFO, "Testing relay - toggling pin state...");
        int currentState = digitalRead(ampSwitchPins[0]);
        logf(LOG_INFO, "Current pin %u state: %s", ampSwitchPins[0], currentState ? "HIGH" : "LOW");
        
        digitalWrite(ampSwitchPins[0], !currentState);
        delay(100); // Small delay to ensure write completes
        
        int newState = digitalRead(ampSwitchPins[0]);
        logf(LOG_INFO, "New pin %u state: %s", ampSwitchPins[0], newState ? "HIGH" : "LOW");
        
        if (newState != !currentState) {
            logf(LOG_ERROR, "Pin state didn't change! Expected %s, got %s", 
                 !currentState ? "HIGH" : "LOW", newState ? "HIGH" : "LOW");
        } else {
            log(LOG_INFO, "Pin toggle successful!");
        }
        return true;
    } else if (cmd.toInt() > 0 && cmd.toInt() <= MAX_AMPSWITCHS) {
        uint8_t ch = cmd.toInt();
        setAmpChannel(ch);
        logf(LOG_INFO, "Amp channel set to %u", ch);
        return true;
    } else if (cmd.length() == 2 && cmd[0] == 'b') {
        int btn = cmd[1] - '0';
        if (btn >= 1 && btn <= MAX_AMPSWITCHS) {
            logf(LOG_INFO, "Simulating button %d press", btn);
            setAmpChannel(btn);
        } else {
            logf(LOG_WARN, "Invalid button number. Use b1-b%d", MAX_AMPSWITCHS);
        }
        return true;
    }
    return false;
}

void handlePinCommand() {
    log(LOG_INFO, "=== PIN ASSIGNMENTS ===");
    
    // Build pin strings using char arrays to avoid String concatenation
    char switchPinsStr[32] = "";
    char buttonPinsStr[32] = "";
    
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        char pinStr[8];
        snprintf(pinStr, sizeof(pinStr), "%u", ampSwitchPins[i]);
        if (i > 0) strcat(switchPinsStr, ",");
        strcat(switchPinsStr, pinStr);
        
        snprintf(pinStr, sizeof(pinStr), "%u", ampButtonPins[i]);
        if (i > 0) strcat(buttonPinsStr, ",");
        strcat(buttonPinsStr, pinStr);
    }
    
    logf(LOG_INFO, "Amp Switch Pins: %s", switchPinsStr);
    logf(LOG_INFO, "Amp Button Pins: %s", buttonPinsStr);
    logf(LOG_INFO, "Status/Pairing LED Pin: %u", PAIRING_LED_PIN);
    logf(LOG_INFO, "MIDI RX Pin: %u", MIDI_RX_PIN);
    logf(LOG_INFO, "MIDI TX Pin: %u", MIDI_TX_PIN);
    log(LOG_INFO, "======================");
}

void showUnknownCommand(const String& cmd) {
    logf(LOG_WARN, "Unknown command: '%s'", cmd.c_str());
    log(LOG_INFO, "Type 'help' for available commands");
}

// Helper functions for printing help menu sections
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

void printHelpMenu() {
    printHelpHeader();
    printSystemCommandsHelp();
    printMIDICommandsHelp();
    printControlCommandsHelp();
    printTestCommandsHelp();
    printDebugCommandsHelp();
    printAmpChannelCommandsHelp();
    printLogLevelsHelp();
    printExamplesHelp();
    printHelpFooter();
}

void printHelpHeader() {
    Serial.println(F("\n========== SERIAL COMMANDS =========="));
}

void printSystemCommandsHelp() {
    Serial.println(F("SYSTEM COMMANDS:"));
    Serial.println(F("  help        : Show this help menu"));
    Serial.println(F("  status      : Show complete system status"));
    Serial.println(F("  memory      : Show memory usage"));
    Serial.println(F("  network     : Show network status"));
    Serial.println(F("  amp         : Show amp channel status"));
    Serial.println(F("  pairing     : Show pairing status"));
    Serial.println(F("  pins        : Show pin assignments (amp, button, LED, MIDI)"));
    Serial.println(F("  uptime      : Show system uptime"));
    Serial.println(F("  version     : Show firmware version"));
    Serial.println(F("  buttons     : Toggle button checking on/off"));
    Serial.println(F("  loglevel    : Show current log level"));
    Serial.println(F("  clearlog    : Clear saved log level (reset to default)"));
    Serial.println(F(""));
}

void printMIDICommandsHelp() {
    Serial.println(F("MIDI COMMANDS:"));
    Serial.println(F("  midi        : Show current MIDI configuration and channel"));
    Serial.println(F("  midimap     : Show MIDI Program Change to channel mapping"));
    Serial.println(F("  ch          : Show the current MIDI channel (persistent, set via channel select mode)"));
    Serial.println(F("  chset       : Print instructions for entering channel select mode"));
    Serial.println(F(""));
}

void printControlCommandsHelp() {
    Serial.println(F("CONTROL COMMANDS:"));
    Serial.println(F("  restart     : Reboot the device"));
    Serial.println(F("  ota         : Enter OTA update mode"));
    Serial.println(F("  pair        : Clear pairing and re-pair"));
    Serial.println(F("  setlogN     : Set log level (N=0-4)"));
    Serial.println(F("  clearall    : Clear all NVS data (pairing + log level)"));
    Serial.println(F(""));
}

void printTestCommandsHelp() {
    Serial.println(F("TEST COMMANDS:"));
    Serial.println(F("  testled     : Test status LED"));
    Serial.println(F("  testpairing : Test pairing LED"));
    Serial.println(F("  testbuttons : Show current button states"));
    Serial.println(F("  forcepair   : Force pairing mode (for testing LED fade)"));
    Serial.println(F(""));
}

void printDebugCommandsHelp() {
    Serial.println(F("DEBUG COMMANDS:"));
    Serial.println(F("  debug       : Show complete debug info"));
    Serial.println(F("  debugperf   : Show performance metrics"));
    Serial.println(F("  debugmemory : Show memory analysis"));
    Serial.println(F("  debugwifi   : Show WiFi stats"));
    Serial.println(F("  debugespnow : Show ESP-NOW stats"));
    Serial.println(F("  debugtask   : Show task stats"));
    Serial.println(F("  debughelp   : Show debug commands"));
    Serial.println(F(""));
}

void printAmpChannelCommandsHelp() {
    Serial.println(F("AMP CHANNEL COMMANDS:"));
    Serial.println(F("  1-4         : Switch to amp channel 1-4"));
    Serial.println(F("  b1-b4       : Simulate button press 1-4"));
    Serial.println(F("  off         : Turn all channels off"));
    Serial.println(F("  test        : Test relay pin toggle"));
    Serial.println(F("  speed       : Measure switching speed"));
    Serial.println(F(""));
}

void printLogLevelsHelp() {
    Serial.println(F("LOG LEVELS:"));
    Serial.println(F("  0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG"));
    Serial.println(F(""));
}

void printExamplesHelp() {
    Serial.println(F("EXAMPLES:"));
    Serial.println(F("  setlog3     : Show info and above logs"));
    Serial.println(F("  2           : Switch to channel 2"));
    Serial.println(F("  b3          : Simulate button 3 press"));
    Serial.println(F("  status      : Show system status"));
    Serial.println(F("  debug       : Show debug information"));
}

void printHelpFooter() {
    Serial.println(F("=====================================\n"));
}

// Utility functions - Memory optimized
const char* getLogLevelString(LogLevel level) {
    switch (level) {
        case LOG_NONE: return "NONE";
        case LOG_ERROR: return "ERROR";
        case LOG_WARN: return "WARN";
        case LOG_INFO: return "INFO";
        case LOG_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

const char* getPairingStatusString(PairingStatus status) {
    switch (status) {
        case NOT_PAIRED: return "NOT_PAIRED";
        case PAIR_REQUEST: return "PAIR_REQUEST";
        case PAIR_REQUESTED: return "PAIR_REQUESTED";
        case PAIR_PAIRED: return "PAIR_PAIRED";
        default: return "UNKNOWN";
    }
}

// Helper function to reset pairing to default state
void resetPairingToDefaults() {
    // Reset server address to broadcast for pairing discovery
    serverAddress[0] = 0xFF;
    serverAddress[1] = 0xFF;
    serverAddress[2] = 0xFF;
    serverAddress[3] = 0xFF;
    serverAddress[4] = 0xFF;
    serverAddress[5] = 0xFF;
    // Reset channel to 1 to start discovery from the beginning
    currentChannel = 1;
    pairingStatus = NOT_PAIRED;
}

void getUptimeString(char* buffer, size_t bufferSize) {
    // Input validation
    if (buffer == nullptr || bufferSize == 0) {
        log(LOG_ERROR, "Invalid buffer parameters in getUptimeString");
        return;
    }
    
    // Ensure minimum buffer size for basic output
    if (bufferSize < 8) {
        log(LOG_ERROR, "Buffer too small for uptime string");
        if (bufferSize > 0) buffer[0] = '\0';  // Null terminate if possible
        return;
    }
    
    unsigned long uptime = millis();
    unsigned long seconds = uptime / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    int result;
    if (days > 0) {
        result = snprintf(buffer, bufferSize, "%lud %02lu:%02lu:%02lu", days, hours % 24, minutes % 60, seconds % 60);
    } else if (hours > 0) {
        result = snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
    } else {
        result = snprintf(buffer, bufferSize, "%02lu:%02lu", minutes, seconds % 60);
    }
    
    // Check for snprintf truncation or error
    if (result < 0) {
        log(LOG_ERROR, "Error formatting uptime string");
        buffer[0] = '\0';
    } else if (result >= (int)bufferSize) {
        logf(LOG_WARN, "Uptime string truncated: needed %d bytes, had %zu", result, bufferSize);
    }
}

uint32_t getFreeHeap() {
    uint32_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (freeHeap < minFreeHeap) {
        minFreeHeap = freeHeap;
    }
    return freeHeap;
}

uint32_t getMinFreeHeap() {
    return minFreeHeap;
}

void setStatusLedPattern(StatusLedPattern pattern) {
    // If pairing or OTA mode is active, ignore other patterns
    if (pairingStatus == PAIR_REQUEST || pairingStatus == PAIR_REQUESTED) {
        currentLedPattern = LED_FADE;
        ledPatternStart = millis();
        ledPatternStep = 0;
        return;
    }
    if (serialOtaTrigger) {
        currentLedPattern = LED_FAST_BLINK;
        return;
    }
    currentLedPattern = pattern;
    ledPatternStart = millis();
    ledPatternStep = 0;
}

void updateStatusLED() {
    static uint16_t fadeValue = 0;
    static int8_t fadeDirection = 1;
    static unsigned long lastUpdate = 0;
    static bool ledState = LOW;
    static unsigned long lastFadeUpdate = 0;
    static StatusLedPattern lastPattern = LED_OFF;
    static PairingStatus lastPairingStatus = NOT_PAIRED;
    unsigned long now = millis();

    // Detect pairing status changes
    if (lastPairingStatus != pairingStatus) {
        // Reset LED to OFF when pairing completes successfully
        if (pairingStatus == PAIR_PAIRED && lastPairingStatus != PAIR_PAIRED) {
            currentLedPattern = LED_OFF;
            ledPatternStart = now;
            ledPatternStep = 0;
            ledcWrite(LEDC_CHANNEL_0, 0);
        }
        
        lastPairingStatus = pairingStatus;
    }

    // Only override patterns during active pairing phases, not when paired
    if (pairingStatus == PAIR_REQUEST || pairingStatus == PAIR_REQUESTED) {
        if (currentLedPattern != LED_FADE) {
            currentLedPattern = LED_FADE;
            ledPatternStart = now;
            ledPatternStep = 0;
        }
    } else if (serialOtaTrigger) {
        if (currentLedPattern != LED_FAST_BLINK) {
            currentLedPattern = LED_FAST_BLINK;
            ledPatternStart = now;
            ledPatternStep = 0;
        }
    }

    // Reset fade variables when pattern changes to fade
    if (currentLedPattern == LED_FADE && lastPattern != LED_FADE) {
        fadeValue = 0;
        fadeDirection = 1;
        lastFadeUpdate = now;
    }
    lastPattern = currentLedPattern;

    switch (currentLedPattern) {
        case LED_SINGLE_FLASH:
            if (ledPatternStep == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
                if (now - ledPatternStart > 80) { ledPatternStep = 1; ledPatternStart = now; }
            } else if (ledPatternStep == 1) {
                ledcWrite(LEDC_CHANNEL_0, 0);
                if (now - ledPatternStart > 120) { currentLedPattern = LED_OFF; }
            }
            break;
        case LED_DOUBLE_FLASH:
            if (ledPatternStep == 0 || ledPatternStep == 2) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
                if (now - ledPatternStart > 60) { ledPatternStep++; ledPatternStart = now; }
            } else if (ledPatternStep == 1 || ledPatternStep == 3) {
                ledcWrite(LEDC_CHANNEL_0, 0);
                if (now - ledPatternStart > 60) { ledPatternStep++; ledPatternStart = now; }
            } else {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_TRIPLE_FLASH:
            if (ledPatternStep % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            if (now - ledPatternStart > 50) {
                ledPatternStep++;
                ledPatternStart = now;
            }
            if (ledPatternStep > 5) {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_QUAD_FLASH:
            if (ledPatternStep % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            if (now - ledPatternStart > 50) {
                ledPatternStep++;
                ledPatternStart = now;
            }
            if (ledPatternStep > 7) {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_PENTA_FLASH:
            if (ledPatternStep % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            if (now - ledPatternStart > 50) {
                ledPatternStep++;
                ledPatternStart = now;
            }
            if (ledPatternStep > 9) {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_HEXA_FLASH:
            if (ledPatternStep % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 8191);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            if (now - ledPatternStart > 50) {
                ledPatternStep++;
                ledPatternStart = now;
            }
            if (ledPatternStep > 11) {
                currentLedPattern = LED_OFF;
            }
            break;
        case LED_FAST_BLINK:
            ledcWrite(LEDC_CHANNEL_0, (now / 100) % 2 ? 8191 : 0);
            break;
        case LED_SOLID_ON:
            ledcWrite(LEDC_CHANNEL_0, 8191);
            break;
        case LED_FADE:
            // Smooth 2-second fade cycle - continuous fade from bottom to top and back
            if (now - lastFadeUpdate > 20) { // Update every 20ms for smooth fade
                fadeValue += fadeDirection * 20; // Smaller increment for smooth fade
                
                if (fadeValue >= 8191) {
                    fadeValue = 8191;
                    fadeDirection = -1; // Start fading down
                } else if (fadeValue <= 0) {
                    fadeValue = 0;
                    fadeDirection = 1; // Start fading up
                }
                
                ledcWrite(LEDC_CHANNEL_0, fadeValue);
                lastFadeUpdate = now;
            }
            break;
        case LED_OFF:
        default:
            ledcWrite(LEDC_CHANNEL_0, 0);
            break;
    }
}