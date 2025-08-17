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
#include <Preferences.h>
#include <globals.h>
#include <utils.h>
#include <dataStructs.h>
#include <nvsManager.h>
#include <nvs_flash.h>
#include <esp_err.h>
#include <nvs.h>
#include <esp_now.h>
#include <espnow-pairing.h>

Preferences preferences;

// NVS initialization and version management
void checkNVS() {
    // First, try to initialize NVS flash
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // NVS partition was truncated and needs to be erased
            log(LOG_WARN, "NVS partition truncated, erasing and reinitializing...");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
            if (err != ESP_OK) {
                logf(LOG_ERROR, "Failed to reinitialize NVS flash: %s", esp_err_to_name(err));
                return;
            }
        } else {
            logf(LOG_ERROR, "Failed to initialize NVS flash: %s", esp_err_to_name(err));
            return;
        }
    }
    
    log(LOG_INFO, "NVS flash initialized successfully");
    
    // Now try to open the namespace
    if (preferences.begin("espnow", true)) {
        int stored_version = preferences.getInt("version", 0);
        preferences.end();

        if (stored_version != STORAGE_VERSION) {
            log(LOG_INFO, "NVS version mismatch, updating...");
            if (preferences.begin("espnow", false)) { // read-write to create/update
                preferences.putInt("version", STORAGE_VERSION);
                preferences.end();
                log(LOG_INFO, "NVS storage version updated");
            } else {
                log(LOG_ERROR, "Failed to update NVS version");
            }
        } else {
            log(LOG_INFO, "NVS version check passed");
        }
    } else {
        // Namespace doesn't exist, create it
        log(LOG_WARN, "NVS namespace 'espnow' not found, creating...");
        if (preferences.begin("espnow", false)) { // read-write to create
            preferences.putInt("version", STORAGE_VERSION);
            preferences.end();
            log(LOG_INFO, "NVS namespace created successfully");
        } else {
            log(LOG_ERROR, "Failed to create NVS namespace");
        }
    }
}

bool initializeNVS() {
    if (preferences.begin("espnow", false)) {
        preferences.putInt("version", STORAGE_VERSION);
        preferences.end();
        log(LOG_INFO, "NVS initialized successfully");
        return true;
    } else {
        log(LOG_ERROR, "Failed to initialize NVS!");
        return false;
    }
}

// Log level management
void saveLogLevelToNVS(LogLevel level) {
    if (preferences.begin("espnow", false)) {
        preferences.putUChar("logLevel", (uint8_t)level);
        preferences.end();
        logf(LOG_INFO, "Log level %s saved to NVS", getLogLevelString(level));
    } else {
        log(LOG_ERROR, "Failed to save log level to NVS");
    }
}

LogLevel loadLogLevelFromNVS() {
    LogLevel level = LOG_INFO; // default
    
    // Try to open preferences - if it fails, NVS isn't ready
    if (preferences.begin("espnow", true)) {
        level = (LogLevel)preferences.getUChar("logLevel", (uint8_t)LOG_INFO);
        preferences.end();
        logf(LOG_DEBUG, "Loaded log level %s from NVS", getLogLevelString(level));
    } else {
        log(LOG_WARN, "Failed to load log level from NVS, using default");
    }
    return level;
}

void clearLogLevelNVS() {
    if (preferences.begin("espnow", false)) {
        preferences.remove("logLevel");
        preferences.end();
        log(LOG_INFO, "Log level cleared from NVS");
    } else {
        log(LOG_ERROR, "Failed to clear log level from NVS");
    }
}

// Server-specific NVS management
void saveServerConfigToNVS() {
    if (preferences.begin("espnow", false)) {
        preferences.putUChar("channel", chan);
        preferences.putUChar("maxClients", MAX_CLIENTS);
        preferences.end();
        log(LOG_INFO, "Server configuration saved to NVS");
    } else {
        log(LOG_ERROR, "Failed to save server config to NVS");
    }
}

bool loadServerConfigFromNVS() {
    if (preferences.begin("espnow", true)) {
        chan = preferences.getUChar("channel", chan);
        preferences.end();
        logf(LOG_INFO, "Loaded server config from NVS - Channel: %u", chan);
        return true;
    } else {
        log(LOG_WARN, "Failed to load server config from NVS, using defaults");
        return false;
    }
}

void clearServerConfigNVS() {
    if (preferences.begin("espnow", false)) {
        preferences.remove("channel");
        preferences.remove("maxClients");
        preferences.end();
        log(LOG_INFO, "Server configuration cleared from NVS");
    } else {
        log(LOG_ERROR, "Failed to clear server config from NVS");
    }
}

// Peer management (extracted from espnow-pairing.cpp)
void savePeersToNVS() {
    preferences.begin("espnow", false);
    int validCount = 0;
    logf(LOG_INFO, "Saving up to %d peers to NVS...", numLabeledPeers);
    for (int i = 0; i < numLabeledPeers; i++) {
        if (memcmp(labeledPeers[i].mac, "\0\0\0\0\0\0", 6) != 0) {
            char key[12];
            sprintf(key, "peer_%d", validCount);
            preferences.putBytes(key, labeledPeers[i].mac, 6);
            char nameKey[16];
            sprintf(nameKey, "peername_%d", validCount);
            preferences.putString(nameKey, labeledPeers[i].name);
            logf(LOG_INFO, "Saved Peer: %s", labeledPeers[i].name);
            printMAC(labeledPeers[i].mac, LOG_DEBUG);
            validCount++;
        } else {
            logf(LOG_ERROR, "Skipped Peer %d - empty or invalid MAC", i);
        }
    }
    preferences.putInt("numClients", validCount);
    preferences.putInt("version", STORAGE_VERSION);
    preferences.end();
    logf(LOG_INFO, "Saved %d peers to NVS", validCount);
}

void loadPeersFromNVS() {
    preferences.begin("espnow", true);
    
    int storedClients = preferences.getInt("numClients", 0);
    logf(LOG_DEBUG, "numClients in NVS: %d", storedClients);
    
    // Reset counters - addPeer will manage them
    numClients = 0;
    numLabeledPeers = 0;
    
    for (int i = 0; i < storedClients && i < MAX_CLIENTS; i++) {
        char key[12];
        sprintf(key, "peer_%d", i);
        uint8_t tempMac[6];
        size_t bytesRead = preferences.getBytes(key, tempMac, 6);
        
        if (bytesRead == 6) {
            if (memcmp(tempMac, "\0\0\0\0\0\0", 6) != 0) {
                // Load the peer name first
                char nameKey[16];
                sprintf(nameKey, "peername_%d", i);
                String peerName = preferences.getString(nameKey, "Unknown");
                
                // Use the existing addPeer function to add to ESP-NOW and manage arrays
                // Set save=false since we're loading from NVS, not adding new peers
                if (addPeer(tempMac, false)) {
                    // Update the peer name in labeledPeers (addPeer might not have the correct name)
                    for (int j = 0; j < numLabeledPeers; j++) {
                        if (memcmp(labeledPeers[j].mac, tempMac, 6) == 0) {
                            strncpy(labeledPeers[j].name, peerName.c_str(), MAX_PEER_NAME_LEN);
                            break;
                        }
                    }
                    logf(LOG_DEBUG, "Loaded and added peer (%s) from NVS to ESP-NOW", peerName.c_str());
                    printMAC(tempMac, LOG_DEBUG);
                } else {
                    logf(LOG_ERROR, "Failed to add peer from NVS to ESP-NOW");
                    printMAC(tempMac, LOG_ERROR);
                }
            }
        }
    }
    
    preferences.end();
    logf(LOG_INFO, "Loaded %d peers from NVS", numClients);
}

// --- Server MIDI persistence ---
bool loadServerMidiConfigFromNVS() {
    if (!preferences.begin("espnow", true)) {
        log(LOG_WARN, "Failed to open NVS for MIDI config load");
        return false;
    }
    serverMidiChannel = preferences.getUChar("srv_midi_ch", 0); // 0=omni
    size_t mapSize = sizeof(serverMidiChannelMap);
    uint8_t temp[MAX_RELAY_CHANNELS];
    bool haveMap = false;
    if (preferences.isKey("srv_midi_map")) {
        size_t len = preferences.getBytesLength("srv_midi_map");
        if (len == mapSize) {
            preferences.getBytes("srv_midi_map", temp, mapSize);
            memcpy(serverMidiChannelMap, temp, mapSize);
            haveMap = true;
        }
    }
    preferences.end();
    logf(LOG_INFO, "Loaded server MIDI channel %u%s", serverMidiChannel, haveMap?" with map":" (default map)");
    return true;
}

void saveServerMidiChannelToNVS() {
    if (!preferences.begin("espnow", false)) return;
    preferences.putUChar("srv_midi_ch", serverMidiChannel);
    preferences.end();
    logf(LOG_INFO, "Saved server MIDI channel %u", serverMidiChannel);
}

void saveServerMidiMapToNVS() {
    if (!preferences.begin("espnow", false)) return;
    preferences.putBytes("srv_midi_map", serverMidiChannelMap, sizeof(serverMidiChannelMap));
    preferences.end();
    log(LOG_INFO, "Saved server MIDI map");
}

void clearPeersNVS() {
    if (preferences.begin("espnow", false)) {
        // Clear peers by setting numClients to 0 and overwriting with empty data
        // This avoids remove() issues while achieving the same result
        uint8_t emptyMAC[6] = {0,0,0,0,0,0};
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            char peerKey[12];
            char nameKey[16];
            sprintf(peerKey, "peer_%d", i);
            sprintf(nameKey, "peername_%d", i);
            
            // Overwrite with empty data instead of removing
            preferences.putBytes(peerKey, emptyMAC, 6);
            preferences.putString(nameKey, "");
        }
        
        // Set peer count to zero
        preferences.putInt("numClients", 0);
        preferences.end();
        
        // Remove peers from ESP-NOW peer list
        for (int i = 0; i < numClients; i++) {
            esp_now_del_peer(clientMacAddresses[i]);
        }
        
        // Clear in-memory peer data immediately
        numClients = 0;
        numLabeledPeers = 0;
        memset(clientMacAddresses, 0, sizeof(clientMacAddresses));
        memset(labeledPeers, 0, sizeof(labeledPeers));
        
        // Save the cleared state to ensure consistency
        savePeersToNVS();
        
        log(LOG_INFO, "All peers cleared from NVS, memory, and ESP-NOW peer list");
    } else {
        log(LOG_ERROR, "Failed to clear peers from NVS");
    }
}

// General NVS utilities
void clearAllNVS() {
    if (preferences.begin("espnow", false)) {
        preferences.clear();
        preferences.end();
        log(LOG_WARN, "All NVS data cleared");
    } else {
        log(LOG_ERROR, "Failed to clear all NVS data");
    }
}

void printNVSStats() {
    log(LOG_INFO, "=== NVS STATISTICS ===");
    
    if (preferences.begin("espnow", true)) {
        int version = preferences.getInt("version", 0);
        int storedClients = preferences.getInt("numClients", 0);
        uint8_t logLevel = preferences.getUChar("logLevel", (uint8_t)LOG_INFO);
        uint8_t channel = preferences.getUChar("channel", 1);
        
        logf(LOG_INFO, "Storage Version: %d", version);
        logf(LOG_INFO, "Stored Peers: %d", storedClients);
        logf(LOG_INFO, "Saved Log Level: %s (%u)", getLogLevelString((LogLevel)logLevel), logLevel);
        logf(LOG_INFO, "Saved Channel: %u", channel);
        
        // Calculate used space (approximate)
        size_t usedEntries = preferences.freeEntries();
        logf(LOG_INFO, "Available NVS Entries: %zu", usedEntries);
        
        preferences.end();
    } else {
        log(LOG_ERROR, "Failed to access NVS for statistics");
    }
    
    log(LOG_INFO, "======================");
}

// --- Server Button PC Map persistence ---
void saveServerButtonPcMapToNVS() {
    if (!preferences.begin("espnow", false)) {
        log(LOG_ERROR, "Failed to open NVS for button PC map save");
        return;
    }
    preferences.putBytes("srv_btn_pc_map", serverButtonProgramMap, sizeof(serverButtonProgramMap));
    preferences.putUChar("srv_btn_count", serverButtonCount);
    preferences.end();
    logf(LOG_INFO, "Saved %u button PC mappings", serverButtonCount);
}

bool loadServerButtonPcMapFromNVS() {
    if (!preferences.begin("espnow", true)) {
        log(LOG_WARN, "Failed to open NVS for button PC map load");
        return false;
    }
    bool ok = false;
    if (preferences.isKey("srv_btn_pc_map")) {
        size_t len = preferences.getBytesLength("srv_btn_pc_map");
        if (len == sizeof(serverButtonProgramMap)) {
            preferences.getBytes("srv_btn_pc_map", serverButtonProgramMap, len);
            ok = true;
        }
    }
    if (preferences.isKey("srv_btn_count")) {
        uint8_t cnt = preferences.getUChar("srv_btn_count", serverButtonCount);
        if (cnt > 0 && cnt <= 8) serverButtonCount = cnt; // clamp
    }
    preferences.end();
    if (ok) logf(LOG_INFO, "Loaded button PC map (count=%u)", serverButtonCount); else log(LOG_INFO, "No saved button PC map - using defaults");
    return ok;
}

// --- Footswitch Configuration Management ---
// Note: These functions are now implemented in footswitchConfig.cpp
// Keeping declarations here for compatibility but they forward to the actual implementations
