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

#include "relayControl.h"
#include "globals.h"
#include "utils.h"

#if HAS_RELAY_OUTPUTS

void setRelayChannel(uint8_t channel) {
    // Turn off all relays first
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (relayOutputPins[i] != 255) {
            digitalWrite(relayOutputPins[i], LOW);
        }
    }
    
    // Turn on the specified channel (1-based)
    if (channel > 0 && channel <= MAX_RELAY_CHANNELS) {
        if (relayOutputPins[channel - 1] != 255) {
            digitalWrite(relayOutputPins[channel - 1], HIGH);
            currentRelayChannel = channel;
            logf(LOG_INFO, "Relay channel %d activated", channel);
        } else {
            logf(LOG_ERROR, "Invalid relay pin for channel %d", channel);
        }
    } else if (channel == 0) {
        currentRelayChannel = 0;
        log(LOG_INFO, "All relays turned off");
    } else {
        logf(LOG_ERROR, "Invalid relay channel: %d (valid: 0-%d)", channel, MAX_RELAY_CHANNELS);
    }
}

void turnOffAllRelays() {
    setRelayChannel(0);
}

uint8_t getCurrentRelayChannel() {
    return currentRelayChannel;
}

void testRelaySpeed() {
    log(LOG_INFO, "=== RELAY SPEED TEST ===");
    
    if (relayOutputPins[0] == 255) {
        log(LOG_ERROR, "No relay pins configured for speed test");
        return;
    }
    
    unsigned long startTime = micros();
    setRelayChannel(1);
    unsigned long time1 = micros();
    setRelayChannel(0);
    unsigned long time2 = micros();
    setRelayChannel(1);
    unsigned long endTime = micros();
    
    logf(LOG_INFO, "Relay ON time: %lu us", time1 - startTime);
    logf(LOG_INFO, "Relay OFF time: %lu us", time2 - time1);
    logf(LOG_INFO, "Total cycle time: %lu us", endTime - startTime);
    logf(LOG_INFO, "Average per switch: %lu us", (endTime - startTime) / 3);
    
    setRelayChannel(0); // Ensure off after test
}

void cycleRelays() {
    log(LOG_INFO, "Cycling through all relay channels...");
    for (int i = 1; i <= MAX_RELAY_CHANNELS; i++) {
        if (relayOutputPins[i - 1] != 255) {
            setRelayChannel(i);
            delay(500);
        }
    }
    setRelayChannel(0);
    log(LOG_INFO, "Relay cycle complete");
}

void printRelayStatus() {
    log(LOG_INFO, "=== RELAY STATUS ===");
    logf(LOG_INFO, "Current Relay Channel: %u", currentRelayChannel);
    logf(LOG_INFO, "Max Relay Channels: %d", MAX_RELAY_CHANNELS);
    
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (relayOutputPins[i] != 255) {
            bool state = digitalRead(relayOutputPins[i]);
            logf(LOG_INFO, "Relay %d (Pin %d): %s", i+1, relayOutputPins[i], 
                 state ? "ON" : "OFF");
        }
    }
    log(LOG_INFO, "=== END RELAY STATUS ===");
}

#endif

// Footswitch functions (always available)
void updateFootswitchState() {
#if HAS_FOOTSWITCH
    static bool lastFootswitchStates[4] = {false, false, false, false};
    
    for (int i = 0; i < 4; i++) {
        if (footswitchPins[i] != 255) {
            bool currentState = digitalRead(footswitchPins[i]) == LOW; // Assuming active low
            
            // Update global footswitchPressed for compatibility with existing code
            if (i == 0) {
                footswitchPressed = currentState;
            }
            
            // Log state changes
            if (currentState != lastFootswitchStates[i]) {
                logf(LOG_DEBUG, "Footswitch %d: %s", i+1, currentState ? "PRESSED" : "RELEASED");
                lastFootswitchStates[i] = currentState;
            }
        }
    }
#else
    // For non-footswitch configurations, keep the original single pin behavior
    footswitchPressed = digitalRead(FOOTSWITCH_PIN) == LOW;
#endif
}

bool isFootswitchPressed(uint8_t footswitchIndex) {
#if HAS_FOOTSWITCH
    if (footswitchIndex < 4 && footswitchPins[footswitchIndex] != 255) {
        return digitalRead(footswitchPins[footswitchIndex]) == LOW;
    }
    return false;
#else
    if (footswitchIndex == 0) {
        return digitalRead(FOOTSWITCH_PIN) == LOW;
    }
    return false;
#endif
}
