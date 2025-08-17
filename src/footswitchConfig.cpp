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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <footswitchConfig.h>
#include <nvsManager.h>
#include <globals.h>
#include <utils.h>
#include <relayControl.h>
#include <commandSender.h>
#include <midiInput.h>

// Global footswitch system configuration
FootswitchSystemConfig fsConfig;

// External preferences object from nvsManager
extern Preferences preferences;

// Footswitch state tracking
struct FootswitchState {
    bool currentState;
    bool lastState;
    unsigned long lastChangeTime;
    unsigned long pressStartTime;
    bool longPressTriggered;
} footswitchStates[MAX_FOOTSWITCHES];

void initFootswitchConfig() {
    // Initialize with default configuration
    memset(&fsConfig, 0, sizeof(fsConfig));
    
    fsConfig.footswitchEnabled = true;
    fsConfig.debounceMs = BUTTON_DEBOUNCE_MS;
    fsConfig.longPressMs = BUTTON_LONGPRESS_MS;
    
    // Set up default configuration name
    strcpy(fsConfig.activeConfig.configName, "Default Configuration");
    fsConfig.activeConfig.configVersion = 1;
    fsConfig.activeConfig.totalMappings = 0;
    
    // Initialize footswitch state tracking
    for (int i = 0; i < MAX_FOOTSWITCHES; i++) {
        footswitchStates[i] = {false, false, 0, 0, false};
    }
    
    // Try to load from NVS
    if (!loadFootswitchConfigFromNVS()) {
        log(LOG_INFO, "Using default footswitch configuration");
        saveFootswitchConfigToNVS(); // Save default config
    }
    
    log(LOG_INFO, "Footswitch configuration initialized");
}

// NVS functions for footswitch configuration
bool loadFootswitchConfigFromNVS() {
    if (!preferences.begin("espnow", true)) {
        log(LOG_WARN, "Failed to open NVS for footswitch config load");
        return false;
    }
    
    bool success = false;
    
    // Load main configuration
    if (preferences.isKey("fs_config")) {
        size_t configSize = preferences.getBytesLength("fs_config");
        if (configSize == sizeof(FootswitchSystemConfig)) {
            size_t bytesRead = preferences.getBytes("fs_config", &fsConfig, configSize);
            if (bytesRead == configSize) {
                success = true;
                logf(LOG_INFO, "Loaded footswitch config: %s (%d mappings)", 
                     fsConfig.activeConfig.configName, fsConfig.activeConfig.totalMappings);
            }
        }
    }
    
    preferences.end();
    return success;
}

void saveFootswitchConfigToNVS() {
    if (!preferences.begin("espnow", false)) {
        log(LOG_ERROR, "Failed to open NVS for footswitch config save");
        return;
    }
    
    fsConfig.activeConfig.lastModified = millis();
    
    size_t bytesWritten = preferences.putBytes("fs_config", &fsConfig, sizeof(FootswitchSystemConfig));
    if (bytesWritten == sizeof(FootswitchSystemConfig)) {
        log(LOG_INFO, "Footswitch configuration saved to NVS");
    } else {
        log(LOG_ERROR, "Failed to save footswitch configuration to NVS");
    }
    
    preferences.end();
}

void clearFootswitchConfigNVS() {
    if (!preferences.begin("espnow", false)) {
        log(LOG_ERROR, "Failed to open NVS for footswitch config clear");
        return;
    }
    
    preferences.remove("fs_config");
    preferences.end();
    
    log(LOG_INFO, "Footswitch configuration cleared from NVS");
}

bool executeFootswitchAction(uint8_t footswitchIndex, bool isLongPress) {
    if (footswitchIndex >= fsConfig.activeConfig.totalMappings) {
        logf(LOG_WARN, "Invalid footswitch index: %d", footswitchIndex);
        return false;
    }
    
    FootswitchMapping* mapping = &fsConfig.activeConfig.mappings[footswitchIndex];
    
    if (!mapping->enabled) {
        logf(LOG_DEBUG, "Footswitch %d is disabled", footswitchIndex);
        return false;
    }
    
    logf(LOG_INFO, "Executing footswitch %d action: %s", 
         footswitchIndex, mapping->description);
    
    switch (mapping->actionType) {
        case FS_ACTION_RELAY_TOGGLE:
            #if HAS_RELAY_OUTPUTS
            if (mapping->targetChannel > 0 && mapping->targetChannel <= MAX_RELAY_CHANNELS) {
                // toggleRelay(mapping->targetChannel - 1);
                logf(LOG_INFO, "Toggle relay %d", mapping->targetChannel);
                return true;
            }
            #endif
            break;
            
        case FS_ACTION_RELAY_MOMENTARY:
            #if HAS_RELAY_OUTPUTS
            if (mapping->targetChannel > 0 && mapping->targetChannel <= MAX_RELAY_CHANNELS) {
                // momentaryRelay(mapping->targetChannel - 1, mapping->momentaryDuration);
                logf(LOG_INFO, "Momentary relay %d for %dms", 
                     mapping->targetChannel, mapping->momentaryDuration);
                return true;
            }
            #endif
            break;
            
        case FS_ACTION_MIDI_LOCAL:
            // Send MIDI locally
            logf(LOG_INFO, "Send local MIDI: Ch%d Type%d Data1:%d Data2:%d", 
                 mapping->midiChannel, mapping->midiType, 
                 mapping->midiData1, mapping->midiData2);
            return true;
            
        case FS_ACTION_MIDI_ESPNOW:
            // Send MIDI via ESP-NOW
            if (mapping->targetPeerIndex < numLabeledPeers) {
                logf(LOG_INFO, "Send ESP-NOW MIDI to peer %d: Ch%d Type%d Data1:%d Data2:%d", 
                     mapping->targetPeerIndex, mapping->midiChannel, mapping->midiType,
                     mapping->midiData1, mapping->midiData2);
                return true;
            }
            break;
            
        case FS_ACTION_PROGRAM_CHANGE:
            logf(LOG_INFO, "Send program change %d on channel %d", 
                 mapping->midiData1, mapping->midiChannel);
            return true;
            
        case FS_ACTION_ALL_OFF:
            #if HAS_RELAY_OUTPUTS
            // Turn all relays off
            logf(LOG_INFO, "All relays off");
            return true;
            #endif
            break;
            
        case FS_ACTION_SCENE_RECALL:
            if (mapping->midiData1 < fsConfig.totalScenes && 
                fsConfig.scenes[mapping->midiData1].enabled) {
                logf(LOG_INFO, "Recall scene %d: %s", 
                     mapping->midiData1, fsConfig.scenes[mapping->midiData1].sceneName);
                return true;
            }
            break;
            
        default:
            logf(LOG_WARN, "Unknown footswitch action type: %d", mapping->actionType);
            return false;
    }
    
    return false;
}

void processFootswitchInput() {
    #if HAS_FOOTSWITCH
    if (!fsConfig.footswitchEnabled) return;
    
    unsigned long currentTime = millis();
    
    for (int i = 0; i < MAX_FOOTSWITCHES; i++) {
        // Read current state (assuming active low buttons)
        bool currentState = !digitalRead(footswitchPins[i]);
        
        // Debounce logic
        if (currentState != footswitchStates[i].lastState) {
            footswitchStates[i].lastChangeTime = currentTime;
        }
        
        if ((currentTime - footswitchStates[i].lastChangeTime) > fsConfig.debounceMs) {
            if (currentState != footswitchStates[i].currentState) {
                footswitchStates[i].currentState = currentState;
                
                if (currentState) {
                    // Button pressed
                    footswitchStates[i].pressStartTime = currentTime;
                    footswitchStates[i].longPressTriggered = false;
                } else {
                    // Button released
                    if (!footswitchStates[i].longPressTriggered) {
                        // Short press
                        executeFootswitchAction(i, false);
                    }
                }
            }
            
            // Check for long press
            if (currentState && !footswitchStates[i].longPressTriggered) {
                if ((currentTime - footswitchStates[i].pressStartTime) >= fsConfig.longPressMs) {
                    footswitchStates[i].longPressTriggered = true;
                    executeFootswitchAction(i, true);
                }
            }
        }
        
        footswitchStates[i].lastState = currentState;
    }
    #endif
}

String footswitchConfigToJson() {
    JsonDocument doc;
    
    // Main configuration
    doc["configName"] = fsConfig.activeConfig.configName;
    doc["configVersion"] = fsConfig.activeConfig.configVersion;
    doc["footswitchEnabled"] = fsConfig.footswitchEnabled;
    doc["debounceMs"] = fsConfig.debounceMs;
    doc["longPressMs"] = fsConfig.longPressMs;
    doc["totalMappings"] = fsConfig.activeConfig.totalMappings;
    doc["lastModified"] = fsConfig.activeConfig.lastModified;
    
    // Mappings
    JsonArray mappings = doc["mappings"].to<JsonArray>();
    for (int i = 0; i < fsConfig.activeConfig.totalMappings; i++) {
        FootswitchMapping* mapping = &fsConfig.activeConfig.mappings[i];
        JsonObject mappingObj = mappings.add<JsonObject>();
        
        mappingObj["footswitchIndex"] = mapping->footswitchIndex;
        mappingObj["actionType"] = mapping->actionType;
        mappingObj["targetChannel"] = mapping->targetChannel;
        mappingObj["midiChannel"] = mapping->midiChannel;
        mappingObj["midiType"] = mapping->midiType;
        mappingObj["midiData1"] = mapping->midiData1;
        mappingObj["midiData2"] = mapping->midiData2;
        mappingObj["targetPeerIndex"] = mapping->targetPeerIndex;
        mappingObj["momentaryDuration"] = mapping->momentaryDuration;
        mappingObj["enabled"] = mapping->enabled;
        mappingObj["description"] = mapping->description;
    }
    
    // Scenes
    JsonArray scenes = doc["scenes"].to<JsonArray>();
    for (int i = 0; i < fsConfig.totalScenes; i++) {
        SceneConfig* scene = &fsConfig.scenes[i];
        if (!scene->enabled) continue;
        
        JsonObject sceneObj = scenes.add<JsonObject>();
        
        sceneObj["sceneIndex"] = scene->sceneIndex;
        sceneObj["sceneName"] = scene->sceneName;
        sceneObj["enabled"] = scene->enabled;
        
        JsonArray relayStates = sceneObj["relayStates"].to<JsonArray>();
        for (int j = 0; j < MAX_RELAY_CHANNELS; j++) {
            relayStates.add(scene->relayStates[j]);
        }
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

bool footswitchConfigFromJson(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        logf(LOG_ERROR, "JSON parsing failed: %s", error.c_str());
        return false;
    }
    
    // Backup current config
    FootswitchSystemConfig backup = fsConfig;
    
    try {
        // Load main configuration
        if (doc["configName"].is<const char*>()) {
            strncpy(fsConfig.activeConfig.configName, doc["configName"], 32);
        }
        if (doc["configVersion"].is<int>()) {
            fsConfig.activeConfig.configVersion = doc["configVersion"];
        }
        if (doc["footswitchEnabled"].is<bool>()) {
            fsConfig.footswitchEnabled = doc["footswitchEnabled"];
        }
        if (doc["debounceMs"].is<int>()) {
            fsConfig.debounceMs = doc["debounceMs"];
        }
        if (doc["longPressMs"].is<int>()) {
            fsConfig.longPressMs = doc["longPressMs"];
        }
        
        // Load mappings
        if (doc["mappings"].is<JsonArray>()) {
            JsonArray mappings = doc["mappings"];
            fsConfig.activeConfig.totalMappings = min((int)mappings.size(), MAX_FOOTSWITCHES);
            
            for (int i = 0; i < fsConfig.activeConfig.totalMappings; i++) {
                JsonObject mapping = mappings[i];
                FootswitchMapping* fsMapping = &fsConfig.activeConfig.mappings[i];
                
                fsMapping->footswitchIndex = mapping["footswitchIndex"] | i;
                fsMapping->actionType = (FootswitchActionType)(mapping["actionType"] | FS_ACTION_NONE);
                fsMapping->targetChannel = mapping["targetChannel"] | 0;
                fsMapping->midiChannel = mapping["midiChannel"] | 1;
                fsMapping->midiType = (FootswitchMidiType)(mapping["midiType"] | FS_MIDI_CC);
                fsMapping->midiData1 = mapping["midiData1"] | 0;
                fsMapping->midiData2 = mapping["midiData2"] | 127;
                fsMapping->targetPeerIndex = mapping["targetPeerIndex"] | 0;
                fsMapping->momentaryDuration = mapping["momentaryDuration"] | 100;
                fsMapping->enabled = mapping["enabled"] | true;
                
                const char* desc = mapping["description"];
                if (desc) {
                    strncpy(fsMapping->description, desc, 32);
                } else {
                    snprintf(fsMapping->description, 32, "Footswitch %d", i + 1);
                }
            }
        }
        
        // Load scenes
        if (doc["scenes"].is<JsonArray>()) {
            JsonArray scenes = doc["scenes"];
            fsConfig.totalScenes = min((int)scenes.size(), 8);
            
            for (int i = 0; i < fsConfig.totalScenes; i++) {
                JsonObject scene = scenes[i];
                SceneConfig* sceneConfig = &fsConfig.scenes[i];
                
                sceneConfig->sceneIndex = scene["sceneIndex"] | i;
                sceneConfig->enabled = scene["enabled"] | false;
                
                const char* sceneName = scene["sceneName"];
                if (sceneName) {
                    strncpy(sceneConfig->sceneName, sceneName, 32);
                } else {
                    snprintf(sceneConfig->sceneName, 32, "Scene %d", i + 1);
                }
                
                if (scene["relayStates"].is<JsonArray>()) {
                    JsonArray relayStates = scene["relayStates"];
                    for (int j = 0; j < MAX_RELAY_CHANNELS && j < (int)relayStates.size(); j++) {
                        sceneConfig->relayStates[j] = relayStates[j] | 0;
                    }
                }
            }
        }
        
        log(LOG_INFO, "Footswitch configuration loaded from JSON");
        return true;
        
    } catch (...) {
        // Restore backup on error
        fsConfig = backup;
        log(LOG_ERROR, "Error loading footswitch configuration, restored backup");
        return false;
    }
}

void printFootswitchConfig() {
    log(LOG_INFO, "=== Footswitch Configuration ===");
    logf(LOG_INFO, "Config: %s (v%d)", fsConfig.activeConfig.configName, fsConfig.activeConfig.configVersion);
    logf(LOG_INFO, "Enabled: %s", fsConfig.footswitchEnabled ? "Yes" : "No");
    logf(LOG_INFO, "Debounce: %dms, Long Press: %dms", fsConfig.debounceMs, fsConfig.longPressMs);
    logf(LOG_INFO, "Total Mappings: %d", fsConfig.activeConfig.totalMappings);
    
    for (int i = 0; i < fsConfig.activeConfig.totalMappings; i++) {
        FootswitchMapping* mapping = &fsConfig.activeConfig.mappings[i];
        logf(LOG_INFO, "  FS%d: %s (%s)", i + 1, mapping->description, 
             mapping->enabled ? "Enabled" : "Disabled");
    }
    
    logf(LOG_INFO, "Total Scenes: %d", fsConfig.totalScenes);
    for (int i = 0; i < fsConfig.totalScenes; i++) {
        SceneConfig* scene = &fsConfig.scenes[i];
        if (scene->enabled) {
            logf(LOG_INFO, "  Scene%d: %s", i + 1, scene->sceneName);
        }
    }
}
