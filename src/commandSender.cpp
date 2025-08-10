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
#include "commandSender.h"
#include <esp_now.h>
#include <globals.h>
#include <utils.h>
#include <espnow-pairing.h>

static unsigned int outgoingReadingId = 0;

// Send a command to a specific client
bool sendCommandToClient(const uint8_t* clientMac, uint8_t commandType, uint8_t commandValue) {
    if (clientMac == nullptr) {
        log(LOG_ERROR, "Cannot send command: client MAC is null");
        return false;
    }
    
    // Check if client is known
    if (strcmp(getPeerName(clientMac), "Unknown") == 0) {
        log(LOG_WARN, "Cannot send command to unknown client MAC:");
        printMAC(clientMac, LOG_WARN);
        return false;
    }
    
    // Prepare command message
    struct_message commandMsg;
    commandMsg.msgType = COMMAND;
    commandMsg.id = 0; // Server ID
    commandMsg.commandType = commandType;
    commandMsg.commandValue = commandValue;
    commandMsg.targetChannel = commandValue; // For channel commands, target matches value
    commandMsg.readingId = ++outgoingReadingId;
    commandMsg.timestamp = millis();
    
    // Debug logging - show what we're actually sending
    logf(LOG_DEBUG, "DEBUG: Sending commandType=%u, commandValue=%u", commandType, commandValue);
    logf(LOG_DEBUG, "DEBUG: STATUS_REQUEST enum value = %u", STATUS_REQUEST);
    logf(LOG_DEBUG, "DEBUG: commandMsg.commandType=%u, commandMsg.commandValue=%u", commandMsg.commandType, commandMsg.commandValue);
    
    // Send the command
    esp_err_t result = esp_now_send(clientMac, (uint8_t*)&commandMsg, sizeof(commandMsg));
    
    if (result == ESP_OK) {
        logf(LOG_INFO, "Command sent successfully - Type: %u, Value: %u to:", commandType, commandValue);
        printMAC(clientMac, LOG_INFO);
        logf(LOG_INFO, "Client: %s", getPeerName(clientMac));
        return true;
    } else {
        logf(LOG_ERROR, "Failed to send command: %s", esp_err_to_name(result));
        return false;
    }
}

// Send a command to all paired clients
bool sendCommandToAllClients(uint8_t commandType, uint8_t commandValue) {
    if (numClients == 0) {
        log(LOG_WARN, "No clients paired - cannot send command");
        return false;
    }
    
    bool allSuccess = true;
    int successCount = 0;
    
    logf(LOG_INFO, "Sending command to %d clients - Type: %u, Value: %u", numClients, commandType, commandValue);
    
    for (int i = 0; i < numClients; i++) {
        if (sendCommandToClient(clientMacAddresses[i], commandType, commandValue)) {
            successCount++;
        } else {
            allSuccess = false;
        }
        delay(10); // Small delay between sends to avoid overwhelming
    }
    
    logf(LOG_INFO, "Command broadcast complete - %d/%d successful", successCount, numClients);
    return allSuccess;
}

// Helper function to send channel change command
bool sendChannelChange(const uint8_t* clientMac, uint8_t channel) {
    logf(LOG_INFO, "Sending program change command: channel %u", channel);
    return sendCommandToClient(clientMac, PROGRAM_CHANGE, channel);
}

// Helper function to send channel change to all clients
bool sendChannelChangeToAll(uint8_t channel) {
    logf(LOG_INFO, "Broadcasting program change command: channel %u", channel);
    return sendCommandToAllClients(PROGRAM_CHANGE, channel);
}


// Helper function to send all channels off command
bool sendAllChannelsOff(const uint8_t* clientMac) {
    log(LOG_INFO, "Sending program change command: all channels off (channel 0)");
    return sendCommandToClient(clientMac, PROGRAM_CHANGE, 0);
}

// Helper function to send all channels off to all clients
bool sendAllChannelsOffToAll() {
    log(LOG_INFO, "Broadcasting program change command: all channels off (channel 0)");
    return sendCommandToAllClients(PROGRAM_CHANGE, 0);
}

// Helper function to send status request command
bool sendStatusRequest(const uint8_t* clientMac) {
    log(LOG_INFO, "Sending status request command");
    return sendCommandToClient(clientMac, STATUS_REQUEST, 0);
}

// Helper function to send status request to all clients
bool sendStatusRequestToAll() {
    log(LOG_INFO, "Broadcasting status request command");
    return sendCommandToAllClients(STATUS_REQUEST, 0);
}

// Broadcast an incoming MIDI Program Change number over ESP-NOW.
// The raw program number is placed in commandValue (PROGRAM_CHANGE) for client mapping logic.
bool forwardMidiProgramToAll(uint8_t programNumber) {
    logf(LOG_INFO, "Forwarding MIDI Program Change %u to all clients", programNumber);
    return sendCommandToAllClients(PROGRAM_CHANGE, programNumber);
}

// Serial command interface for sending commands (refactored for clarity)
void handleSendCommand(const String& cmd) {
    String command = cmd; command.trim(); command.toLowerCase();

    if (command == "sendhelp") { printSendCommandHelp(); return; }

    // ---- SEND COMMAND GROUP ----
    if (command.startsWith("send ")) {
        String params = command.substring(5); params.trim();
        if (params.isEmpty()) { log(LOG_WARN, "Missing send command parameters"); printSendCommandHelp(); return; }

        int firstSpace = params.indexOf(' ');
        if (firstSpace == -1) {
            if (params == "help") { printSendCommandHelp(); }
            else if (params == "status") {
                if (numClients == 0) log(LOG_INFO, "No clients paired");
                else {
                    log(LOG_INFO, "=== PAIRED CLIENTS ===");
                    for (int i=0;i<numClients;i++) { logf(LOG_INFO, "Client %d: %s", i, getPeerName(clientMacAddresses[i])); Serial.print("  MAC: "); printMAC(clientMacAddresses[i], LOG_INFO); }
                    log(LOG_INFO, "=====================");
                }
            }
            else if (params == "statusreq") { sendStatusRequestToAll(); }
            else if (params == "off") { sendAllChannelsOffToAll(); }
            else { logf(LOG_WARN, "Unknown send command: %s", params.c_str()); printSendCommandHelp(); }
            return;
        }

        String subCmd = params.substring(0, firstSpace);
        String args = params.substring(firstSpace+1); args.trim();

        if (subCmd == "channel" || subCmd == "progch" || subCmd == "pc") {
            int spacePos = args.indexOf(' ');
            if (spacePos == -1) {
                int channel = args.toInt();
                if (channel >=0 && channel <=4) sendChannelChangeToAll(channel);
                else log(LOG_WARN, "Invalid channel number. Use 0-4 (0=off,1-4=channels)");
            } else {
                int channel = args.substring(0, spacePos).toInt();
                int clientIndex = args.substring(spacePos+1).toInt();
                if (channel <0 || channel >4) { log(LOG_WARN, "Invalid channel number"); return; }
                if (clientIndex <0 || clientIndex >= numClients) { logf(LOG_WARN, "Invalid client index. Use 0-%d", numClients-1); return; }
                sendChannelChange(clientMacAddresses[clientIndex], channel);
            }
        } else if (subCmd == "off") {
            if (args.isEmpty()) sendAllChannelsOffToAll();
            else { int clientIndex = args.toInt(); if (clientIndex<0||clientIndex>=numClients){ logf(LOG_WARN,"Invalid client index. Use 0-%d", numClients-1); return;} sendAllChannelsOff(clientMacAddresses[clientIndex]); }
        } else if (subCmd == "statusreq") {
            if (args.isEmpty()) sendStatusRequestToAll();
            else { int clientIndex = args.toInt(); if (clientIndex<0||clientIndex>=numClients){ logf(LOG_WARN,"Invalid client index. Use 0-%d", numClients-1); return;} sendStatusRequest(clientMacAddresses[clientIndex]); }
        } else if (subCmd == "pcraw") {
            if (args.isEmpty()) { log(LOG_WARN, "Missing program number. Use: send pcraw <0-127>"); return; }
            int program = args.toInt(); if (program<0||program>127){ log(LOG_WARN,"Invalid program number 0-127"); return; }
            forwardMidiProgramToAll((uint8_t)program);
        } else if (subCmd == "raw") {
            int fs = args.indexOf(' '); if (fs==-1){ log(LOG_WARN,"Format: send raw <type> <value> [client]"); return; }
            int commandType = args.substring(0,fs).toInt(); String remaining = args.substring(fs+1); remaining.trim();
            int ss = remaining.indexOf(' '); int commandValue; int clientIndex=-1;
            if (ss==-1) commandValue = remaining.toInt(); else { commandValue = remaining.substring(0,ss).toInt(); clientIndex = remaining.substring(ss+1).toInt(); }
            if (clientIndex==-1) sendCommandToAllClients(commandType, commandValue);
            else { if (clientIndex<0||clientIndex>=numClients){ logf(LOG_WARN,"Invalid client index. Use 0-%d", numClients-1); return;} sendCommandToClient(clientMacAddresses[clientIndex], commandType, commandValue); }
        } else {
            logf(LOG_WARN, "Unknown send command: %s", subCmd.c_str()); printSendCommandHelp();
        }
        return; // done with send group
    }

    // ---- MIDI COMMAND GROUP ----
    else if (command.startsWith("midi")) {
        String params = command.substring(4); params.trim();
        if (params.isEmpty() || params == "help") {
            log(LOG_INFO, "MIDI Commands:");
            log(LOG_INFO, "  midi ch <1-16|0>     - Set server MIDI channel (0=omni)");
            log(LOG_INFO, "  midi map             - Show current program map");
            log(LOG_INFO, "  midi map <idx> <pc>  - Set map entry (0-based relay index) to program number");
            log(LOG_INFO, "  midi reset           - Reset MIDI map to defaults (all 0)");
            log(LOG_INFO, "  midi info            - Detailed MIDI status & duplicates");
            log(LOG_INFO, "  midi save            - Save channel & map to NVS");
            return;
        }
        int space = params.indexOf(' '); String sub = (space==-1)?params:params.substring(0,space); String rest = (space==-1)?"":params.substring(space+1); rest.trim();
        if (sub == "ch") {
            int ch = rest.toInt(); if (ch<0||ch>16){ log(LOG_WARN,"Invalid MIDI channel (0-16)"); return;} serverMidiChannel = (uint8_t)ch; logf(LOG_INFO,"Server MIDI channel set to %u", serverMidiChannel);
        } else if (sub == "map") {
            if (rest.isEmpty()) { log(LOG_INFO,"Current MIDI Map (relayIndex:program):"); for (int i=0;i<MAX_RELAY_CHANNELS;i++) logf(LOG_INFO,"  %d:%u", i, serverMidiChannelMap[i]); }
            else { int s2 = rest.indexOf(' '); if (s2==-1){ log(LOG_WARN,"Format: midi map <idx> <program>"); return;} int idx = rest.substring(0,s2).toInt(); int prog=rest.substring(s2+1).toInt(); if (idx<0||idx>=MAX_RELAY_CHANNELS){ log(LOG_WARN,"Index out of range"); return;} if (prog<0||prog>127){ log(LOG_WARN,"Program 0-127 only"); return;} serverMidiChannelMap[idx]=(uint8_t)prog; logf(LOG_INFO,"Map[%d]=%d", idx, prog); }
        } else if (sub == "reset") {
            for (int i=0;i<MAX_RELAY_CHANNELS;i++) serverMidiChannelMap[i]=0; saveServerMidiMapToNVS(); log(LOG_INFO,"MIDI map reset to defaults (all 0) and saved");
        } else if (sub == "info") {
            log(LOG_INFO,"=== MIDI INFO ==="); logf(LOG_INFO," Channel: %u (0=omni)", serverMidiChannel); log(LOG_INFO," Map (relayIndex -> Program):"); for (int i=0;i<MAX_RELAY_CHANNELS;i++) logf(LOG_INFO,"  %d -> %u", i, serverMidiChannelMap[i]); bool anyDup=false; for (int i=0;i<MAX_RELAY_CHANNELS;i++) for (int j=i+1;j<MAX_RELAY_CHANNELS;j++) if (serverMidiChannelMap[i]==serverMidiChannelMap[j] && serverMidiChannelMap[i]!=0){ logf(LOG_INFO,"  DUPLICATE: Program %u used by relays %d and %d", serverMidiChannelMap[i], i, j); anyDup=true; } if(!anyDup) log(LOG_INFO,"  No duplicate non-zero program assignments"); logf(LOG_INFO," Learn Armed: %s", serverMidiLearnArmed?"yes":"no"); if (serverMidiLearnArmed) logf(LOG_INFO," Learn Target Relay Index: %d", serverMidiLearnTarget); log(LOG_INFO,"=================");
        } else if (sub == "save") {
            saveServerMidiChannelToNVS(); saveServerMidiMapToNVS(); saveServerButtonPcMapToNVS();
        } else { log(LOG_WARN,"Unknown midi subcommand (use 'midi help')"); }
        return;
    }
    else if (command.startsWith("btn")) {
            String params = command.substring(3);
            params.trim();
            if (params.isEmpty() || params == "help") {
                log(LOG_INFO, "Button PC Map Commands:");
                log(LOG_INFO, "  btn list                - Show button->PC assignments");
                log(LOG_INFO, "  btn set <idx> <pc>      - Assign Program Change to button index (>=1 for extra buttons)");
                log(LOG_INFO, "  btn reset               - Reset button PC map to defaults (sequential) & save");
                log(LOG_INFO, "  btn save                - Persist button map (manual save; 'set' auto-saves)");
                return;
            }
            int sp = params.indexOf(' ');
            String sub = (sp==-1)?params:params.substring(0,sp);
            String rest = (sp==-1)?"":params.substring(sp+1);
            rest.trim();
            if (sub == "list") {
                logf(LOG_INFO, "Button Count: %u", serverButtonCount);
                for (int i=0;i<serverButtonCount;i++) {
                    logf(LOG_INFO, "  Button %d -> PC %u", i, serverButtonProgramMap[i]);
                }
            } else if (sub == "set") {
                int sp2 = rest.indexOf(' ');
                if (sp2 == -1) { log(LOG_WARN, "Format: btn set <idx> <pc>"); return; }
                int idx = rest.substring(0,sp2).toInt();
                int pc = rest.substring(sp2+1).toInt();
                if (idx < 0 || idx >= serverButtonCount) { log(LOG_WARN, "Index out of range"); return; }
                if (pc < 0 || pc > 127) { log(LOG_WARN, "PC 0-127 only"); return; }
                serverButtonProgramMap[idx] = (uint8_t)pc;
                logf(LOG_INFO, "Set button %d -> PC %d", idx, pc);
                saveServerButtonPcMapToNVS();
                log(LOG_INFO, "(Auto-saved button PC map)");
            } else if (sub == "reset") {
                for (int i=0;i<serverButtonCount;i++) serverButtonProgramMap[i] = (uint8_t)i; // sequential default
                saveServerButtonPcMapToNVS();
                log(LOG_INFO, "Button PC map reset to sequential defaults and saved");
            } else if (sub == "save") {
                saveServerButtonPcMapToNVS();
            } else {
                log(LOG_WARN, "Unknown btn subcommand");
            }
    } else if (command == "maps" || command == "showmaps") {
        log(LOG_INFO, "=== COMBINED MAP SUMMARY ===");
        logf(LOG_INFO, "MIDI Channel: %u (0=omni)", serverMidiChannel);
        log(LOG_INFO, "Relay MIDI Map (index -> Program):");
        for (int i=0;i<MAX_RELAY_CHANNELS;i++) {
            logf(LOG_INFO, "  %d -> %u", i, serverMidiChannelMap[i]);
        }
        log(LOG_INFO, "Button PC Map (button -> Program):");
        for (int i=0;i<serverButtonCount;i++) {
            logf(LOG_INFO, "  %d -> %u", i, serverButtonProgramMap[i]);
        }
        logf(LOG_INFO, "Learn Armed: %s  Target: %d", serverMidiLearnArmed?"yes":"no", serverMidiLearnTarget);
        log(LOG_INFO, "============================");
    } else {
        log(LOG_WARN, "Invalid command format. Use 'send <command>' or 'sendhelp'");
        printSendCommandHelp();
    }
}

void printSendCommandHelp() {
    log(LOG_INFO, "=== SEND COMMAND HELP ===");
    log(LOG_INFO, "Send commands to paired clients:");
    log(LOG_INFO, "");
    log(LOG_INFO, "Basic Commands:");
    log(LOG_INFO, "  send channel|pc <0-4>        - Select amp channel (mapped via PROGRAM_CHANGE)");
    log(LOG_INFO, "  send channel|pc <0-4> <client> - Select channel on specific client");
    log(LOG_INFO, "  send off                     - Turn off all channels on all clients");
    log(LOG_INFO, "  send off <client>            - Turn off all channels on specific client");
    log(LOG_INFO, "  send statusreq               - Request status from all clients");
    log(LOG_INFO, "  send statusreq <client>      - Request status from specific client");
    log(LOG_INFO, "  send pcraw <0-127>           - Forward raw MIDI Program Change to all clients");
    log(LOG_INFO, "  midi ch <0|1-16>             - Set server MIDI channel (0=omni)");
    log(LOG_INFO, "  midi map [idx prog]          - Show or set mapping entry");
    log(LOG_INFO, "  midi reset                   - Reset MIDI map to defaults (all 0) & save");
    log(LOG_INFO, "  midi save                    - Save MIDI channel/map to NVS");
    log(LOG_INFO, "  btn list|set|reset|save      - Manage button PC map (set auto-saves)");
    log(LOG_INFO, "  maps                         - Show combined MIDI & button maps");
    log(LOG_INFO, "");
    log(LOG_INFO, "Advanced Commands:");
    log(LOG_INFO, "  send raw <type> <value>         - Send raw command to all clients");
    log(LOG_INFO, "  send raw <type> <value> <client> - Send raw command to specific client");
    log(LOG_INFO, "    Command Types: 0=PROGRAM_CHANGE, 1=RESERVED, 2=ALL_CHANNELS_OFF, 3=STATUS_REQUEST");
    log(LOG_INFO, "");
    log(LOG_INFO, "Status Commands:");
    log(LOG_INFO, "  send status                  - Show paired clients");
    log(LOG_INFO, "  send help                    - Show this help");
    log(LOG_INFO, "  sendhelp                     - Show this help");
    log(LOG_INFO, "");
    log(LOG_INFO, "Examples:");
    log(LOG_INFO, "  send channel 1               - All clients -> channel 1");
    log(LOG_INFO, "  send channel 2 0             - Client 0 -> channel 2");
    log(LOG_INFO, "  send channel 0               - Turn off all clients (channel 0)");
    log(LOG_INFO, "  send pcraw 5                 - Forward MIDI Program 5 to all clients");
    log(LOG_INFO, "  send off                     - Turn off all clients");
    log(LOG_INFO, "  send statusreq               - Request current channel status from all clients");
    log(LOG_INFO, "  send statusreq 0             - Request status from client 0");
    log(LOG_INFO, "  send raw 0 1                 - Send PROGRAM_CHANGE 1 to all clients");
    log(LOG_INFO, "  send raw 3 0                 - Send STATUS_REQUEST to all clients");
    log(LOG_INFO, "  send status                  - List all paired clients with indices");
    log(LOG_INFO, "");
    log(LOG_INFO, "Notes:");
    log(LOG_INFO, "  - Channel 0 = All channels off");
    log(LOG_INFO, "  - Channels 1-4 = Specific amp channels");
    log(LOG_INFO, "  - Client index starts from 0 (use 'send status' to see indices)");
    log(LOG_INFO, "  - 'channel'/'pc' use PROGRAM_CHANGE (type 0) only; type 1 reserved");
    log(LOG_INFO, "========================");
}
