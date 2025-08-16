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

#define MAX_PEER_NAME_LEN 32

enum MessageType { PAIRING, DATA, COMMAND };
enum CommandType { 
    PROGRAM_CHANGE = 0,     // MIDI program change - Type 0
    RESERVED1 = 1,           // (formerly CHANNEL_CHANGE) reserved to keep enum values stable
    ALL_CHANNELS_OFF = 2,    // Turn all channels off - Type 2
    STATUS_REQUEST = 3       // Request current status - Type 3
};

typedef struct struct_message {
    uint8_t msgType;           // MessageType
    uint8_t id;                // Message ID for tracking
    uint8_t commandType;       // CommandType for command messages
    uint8_t commandValue;      // Command parameter (channel number, etc.)
    uint8_t targetChannel;     // Which amp channel to control (1-4, 0=all off)
    unsigned int readingId;    // Message sequence number
    uint32_t timestamp;        // Timestamp for message ordering
} struct_message;

typedef struct struct_pairing {
    uint8_t msgType;
    uint8_t id;
    uint8_t macAddr[6];
    uint8_t channel;
    char name[MAX_PEER_NAME_LEN];
} struct_pairing;

