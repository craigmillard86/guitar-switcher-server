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
#include <dataStructs.h>
#include <config.h>

#ifndef MAX_FOOTSWITCHES
#define MAX_FOOTSWITCHES 8
#endif

// Footswitch action types
enum FootswitchActionType {
    FS_ACTION_NONE = 0,
    FS_ACTION_RELAY_TOGGLE = 1,    // Toggle a local relay
    FS_ACTION_RELAY_MOMENTARY = 2, // Momentary relay activation
    FS_ACTION_MIDI_LOCAL = 3,      // Send MIDI locally
    FS_ACTION_MIDI_ESPNOW = 4,     // Send MIDI via ESP-NOW
    FS_ACTION_PROGRAM_CHANGE = 5,  // Send program change to all clients
    FS_ACTION_ALL_OFF = 6,         // Turn all channels off
    FS_ACTION_SCENE_RECALL = 7     // Recall a saved scene
};

// MIDI command types for footswitch actions
enum FootswitchMidiType {
    FS_MIDI_CC = 0,              // Control Change
    FS_MIDI_PC = 1,              // Program Change
    FS_MIDI_NOTE_ON = 2,         // Note On
    FS_MIDI_NOTE_OFF = 3         // Note Off
};

// Individual footswitch mapping
typedef struct {
    uint8_t footswitchIndex;           // Which footswitch (0-7)
    FootswitchActionType actionType;   // What action to perform
    uint8_t targetChannel;             // For relay actions (1-4, 0=all)
    uint8_t midiChannel;               // For MIDI actions (1-16, 0=omni)
    FootswitchMidiType midiType;       // Type of MIDI message
    uint8_t midiData1;                 // First data byte (CC number, note, etc.)
    uint8_t midiData2;                 // Second data byte (value, velocity, etc.)
    uint8_t targetPeerIndex;           // For ESP-NOW actions (index into peers array)
    uint16_t momentaryDuration;        // For momentary actions (ms)
    bool enabled;                      // Whether this mapping is active
    char description[32];              // User-friendly description
} FootswitchMapping;

// Complete footswitch configuration
typedef struct {
    FootswitchMapping mappings[MAX_FOOTSWITCHES];
    uint8_t totalMappings;
    uint8_t configVersion;
    char configName[32];
    uint32_t lastModified;
} FootswitchConfig;

// Scene configuration (snapshot of all relay states)
typedef struct {
    uint8_t sceneIndex;
    char sceneName[32];
    uint8_t relayStates[MAX_RELAY_CHANNELS]; // 0=off, 1=on
    bool enabled;
} SceneConfig;

// Global footswitch system configuration
typedef struct {
    FootswitchConfig activeConfig;
    SceneConfig scenes[8];  // Up to 8 scenes
    uint8_t totalScenes;
    bool footswitchEnabled;
    uint16_t debounceMs;
    uint16_t longPressMs;
} FootswitchSystemConfig;

// Function declarations
void initFootswitchConfig();
bool loadFootswitchConfigFromNVS();
void saveFootswitchConfigToNVS();
void clearFootswitchConfigNVS();
bool executeFootswitchAction(uint8_t footswitchIndex, bool isLongPress = false);
void processFootswitchInput();
String footswitchConfigToJson();
bool footswitchConfigFromJson(const String& json);
void printFootswitchConfig();
