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
#include <utils.h>
#include <espnow-pairing.h>
#include <commandHandler.h>
#include <math.h>
#include <commandSender.h>

#define BUTTON_DEBOUNCE_MS 100    // Button debounce duration in ms
#define BUTTON_LONGPRESS_MS 5000  // Base long-press threshold (first milestone)
#define MAX_BUTTONS 8             // Maximum number of buttons supported now

// Button system variables
static bool enableButtonChecking = true;
// pairing / mode button is serverButtonPins[0]; additional buttons in globals
static uint8_t pairingButtonPins[MAX_BUTTONS] = {PAIRING_BUTTON_PIN};

// --- Server MIDI learn / channel select state (mirrors simplified client UX) ---
static bool milestone5s = false;
static bool milestone10s = false;
static bool milestone15s = false;
static bool milestone30s = false;

static bool channelSelectMode = false;        // Currently selecting inbound MIDI channel
static uint8_t tempMidiChannel = 1;            // Temp channel while selecting
static unsigned long lastChannelButtonPress = 0; // For inactivity auto-save

static unsigned long serverMidiLearnStartTime = 0; // When learn actually started (after selecting target)
static int pendingLearnTarget = -1; // interim target selection while armed (multi-relay)

static bool midiLearnJustTimedOut = false; // Prevent pairing trigger right after timeout

// Forward declarations (local)
static void resetMilestones();
static void enterChannelSelectMode();
static void handleChannelSelectShortPress();
static void cycleLearnTarget();
static void handleChannelSelectAutoSave();
static bool handleServerMidiLearnTimeout();

// Update LED patterns - call this regularly from main loop
void updateLedPatterns() {
    static unsigned long lastUpdate = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastUpdate < 50) return; // Update every 50ms
    lastUpdate = currentTime;
    
    unsigned long elapsed = currentTime - ledPatternStart;
    
    switch (currentLedPattern) {
        case LED_SINGLE_FLASH:
            if (elapsed < 200) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 400) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else {
                currentLedPattern = LED_OFF;
            }
            break;
            
        case LED_DOUBLE_FLASH:
            if (elapsed < 200) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 300) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else if (elapsed < 500) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 600) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else {
                currentLedPattern = LED_OFF;
            }
            break;
            
        case LED_TRIPLE_FLASH:
            if (elapsed < 200) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 300) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else if (elapsed < 500) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 600) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else if (elapsed < 800) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else if (elapsed < 900) {
                ledcWrite(LEDC_CHANNEL_0, 0);
            } else {
                currentLedPattern = LED_OFF;
            }
            break;
            
        case LED_FAST_BLINK:
            if ((elapsed / 200) % 2 == 0) {
                ledcWrite(LEDC_CHANNEL_0, 255);
            } else {
                ledcWrite(LEDC_CHANNEL_0, 0);
            }
            break;
            
        case LED_SOLID_ON:
            ledcWrite(LEDC_CHANNEL_0, 255);
            break;
            
        case LED_FADE:
            {
                int brightness = (sin((elapsed / 1000.0) * 2 * PI) + 1) * 127.5;
                ledcWrite(LEDC_CHANNEL_0, brightness);
            }
            break;
            
        case LED_OFF:
        default:
            ledcWrite(LEDC_CHANNEL_0, 0);
            break;
    }
}

// Milestone feedback replicating client semantics:
//  5s  : feedback only
// 10s  : MIDI Learn ready
// 15s  : Channel Select ready
// 30s  : Pairing ready
void handleLedFeedback(unsigned long held, const char* buttonName) {
    if (held >= 5000 && !milestone5s) {
        currentLedPattern = LED_SINGLE_FLASH;
        ledPatternStart = millis();
        ledPatternStep = 0;
        milestone5s = true;
        logf(LOG_INFO, "%s - 5s held - LED feedback", buttonName);
    } else if (held >= 10000 && !milestone10s) {
        currentLedPattern = LED_DOUBLE_FLASH;
        ledPatternStart = millis();
        ledPatternStep = 0;
        milestone10s = true;
        logf(LOG_INFO, "%s - 10s held - MIDI Learn ready", buttonName);
    } else if (held >= 15000 && !milestone15s) {
        currentLedPattern = LED_TRIPLE_FLASH;
        ledPatternStart = millis();
        ledPatternStep = 0;
        milestone15s = true;
        logf(LOG_INFO, "%s - 15s held - Channel Select ready", buttonName);
    } else if (held >= 30000 && !milestone30s) {
        currentLedPattern = LED_FAST_BLINK;
        ledPatternStart = millis();
        ledPatternStep = 0;
        milestone30s = true;
        logf(LOG_INFO, "%s - 30s held - Pairing ready", buttonName);
    }
}

void checkPairingButtons() {
    // Skip button checking if disabled
    if (!enableButtonChecking) {
        return;
    }

    static unsigned long lastDebounceTime[MAX_BUTTONS] = {0};
    static uint8_t lastButtonState[MAX_BUTTONS] = {HIGH, HIGH, HIGH, HIGH};
    static bool buttonPressed[MAX_BUTTONS] = {false, false, false, false};
    static unsigned long buttonPressStart[MAX_BUTTONS] = {0};
    static bool buttonLongPressHandled[MAX_BUTTONS] = {false, false, false, false};

    for (int i = 0; i < serverButtonCount && i < MAX_BUTTONS; i++) {
        if (serverButtonPins[i] == 255) continue;
        uint8_t reading = digitalRead(serverButtonPins[i]);
        processButtonState(i, reading, lastDebounceTime, lastButtonState,
                          buttonPressed, buttonPressStart, buttonLongPressHandled);
    }

    // Background tasks
    handleChannelSelectAutoSave();
    handleServerMidiLearnTimeout();
}

void processButtonState(int buttonIndex, uint8_t reading,
                       unsigned long* lastDebounceTime, uint8_t* lastButtonState,
                       bool* buttonPressed, unsigned long* buttonPressStart,
                       bool* buttonLongPressHandled) {
    const unsigned long debounceDelay = BUTTON_DEBOUNCE_MS;

    if (reading != lastButtonState[buttonIndex]) {
        lastDebounceTime[buttonIndex] = millis();
    }

    if ((millis() - lastDebounceTime[buttonIndex]) > debounceDelay) {
        // Button pressed
        if (reading == LOW && !buttonPressed[buttonIndex]) {
            handleButtonPress(buttonIndex, buttonPressed, buttonPressStart, buttonLongPressHandled);
        }
        // Button held
        else if (reading == LOW && buttonPressed[buttonIndex]) {
            unsigned long held = millis() - buttonPressStart[buttonIndex];
            handleButtonHeld(buttonIndex, held);
        }
        // Button released
        else if (reading == HIGH && buttonPressed[buttonIndex]) {
            unsigned long held = millis() - buttonPressStart[buttonIndex];
            handleButtonRelease(buttonIndex, held, buttonPressed, buttonLongPressHandled);
        }
    }

    lastButtonState[buttonIndex] = reading;
}

void handleButtonPress(int buttonIndex, bool* buttonPressed, unsigned long* buttonPressStart,
                      bool* buttonLongPressHandled) {
    buttonPressStart[buttonIndex] = millis();
    buttonPressed[buttonIndex] = true;
    buttonLongPressHandled[buttonIndex] = false;

    if (buttonIndex == 0) { // Mode button
        currentLedPattern = LED_SINGLE_FLASH;
        ledPatternStart = millis();
        ledPatternStep = 0;
        resetMilestones();
        midiLearnJustTimedOut = false; // clear flag on new press

        if (channelSelectMode) {
            handleChannelSelectShortPress();
        } else if (serverMidiLearnArmed && MAX_RELAY_CHANNELS > 1) {
            // While armed (before receiving PC) cycle through relay targets
            cycleLearnTarget();
        }
    }
}

void handleButtonHeld(int buttonIndex, unsigned long held) {
    // Only button 1 supports long press functions
    if (buttonIndex == 0) {
        // LED feedback for button 1
        handleLedFeedback(held, "Pairing Button");
    }
}

void handleButtonRelease(int buttonIndex, unsigned long held, bool* buttonPressed,
                        bool* buttonLongPressHandled) {
    const unsigned long firstLongPress = BUTTON_LONGPRESS_MS; // 5s milestone

    if (!buttonLongPressHandled[buttonIndex]) {
        if (buttonIndex != 0) {
            // Non-mode button short press semantics (only if NOT in any special mode and not armed learn cycling)
            if (held < BUTTON_LONGPRESS_MS && !channelSelectMode && !serverMidiLearnArmed) {
                // Send its mapped Program Change to clients
                uint8_t pc = serverButtonProgramMap[buttonIndex];
                forwardMidiProgramToAll(pc);
                logf(LOG_INFO, "Button %d short press -> send PC %u", buttonIndex, pc);
                currentLedPattern = LED_SINGLE_FLASH;
                ledPatternStart = millis();
            } else if (serverMidiLearnArmed && serverMidiLearnTarget >= 0 && held < BUTTON_LONGPRESS_MS) {
                // While armed pre-PC: use other buttons to pick relay target directly
                pendingLearnTarget = buttonIndex - 1; // map button1 -> relay0, button2 -> relay1, etc.
                if (pendingLearnTarget < 0) pendingLearnTarget = 0;
                if (pendingLearnTarget >= MAX_RELAY_CHANNELS) pendingLearnTarget = MAX_RELAY_CHANNELS-1;
                serverMidiLearnTarget = pendingLearnTarget;
                logf(LOG_INFO, "MIDI Learn: direct select relay %d via button %d", serverMidiLearnTarget+1, buttonIndex);
                currentLedPattern = LED_SINGLE_FLASH;
                ledPatternStart = millis();
            }
        }
        if (buttonIndex == 0) {
        if (channelSelectMode) {
            // In channel select mode every release counts as increment
            handleChannelSelectShortPress();
        } else if (held >= 30000 && !midiLearnJustTimedOut) {
            // Pairing mode (30s)
            log(LOG_INFO, "30s+ hold released: Pairing mode activated");
            pairingforceStart();
            currentLedPattern = LED_FADE;
            ledPatternStart = millis();
        } else if (held >= 15000) {
            // Enter channel select mode (15s)
            enterChannelSelectMode();
        } else if (held >= 10000) {
            // Arm MIDI learn (10s)
            serverMidiLearnArmed = true;
            if (MAX_RELAY_CHANNELS == 1) {
                serverMidiLearnTarget = 0;
                pendingLearnTarget = 0;
                log(LOG_INFO, "MIDI Learn armed for relay 1. Send Program Change...");
            } else {
                // Begin with first relay; subsequent presses cycle before PC arrives
                pendingLearnTarget = (pendingLearnTarget < 0) ? 0 : pendingLearnTarget;
                serverMidiLearnTarget = pendingLearnTarget; // active display target
                logf(LOG_INFO, "MIDI Learn armed. Initial target relay %d. Press button to cycle before sending PC...", pendingLearnTarget + 1);
            }
            serverMidiLearnStartTime = millis();
            currentLedPattern = LED_FAST_BLINK;
            ledPatternStart = millis();
        } else if (held >= firstLongPress) {
            // 5s hold release: just feedback (no OTA here to mirror client). OTA remains via serial command.
            log(LOG_INFO, "5s+ hold released: Feedback only");
            currentLedPattern = LED_SINGLE_FLASH;
            ledPatternStart = millis();
        } else {
            // Short press normal action: if not in any special mode just flash
            currentLedPattern = LED_SINGLE_FLASH;
            ledPatternStart = millis();
            ledPatternStep = 0;
    }
    }
    }

    buttonPressed[buttonIndex] = false;
    buttonLongPressHandled[buttonIndex] = false;

    if (buttonIndex == 0) {
        currentLedPattern = LED_OFF;
        resetMilestones();
    }
}

// Button simulation functions for command interface
void simulateButton1Press() {
    log(LOG_INFO, "Simulating button 1 short press");
    currentLedPattern = LED_SINGLE_FLASH;
    ledPatternStart = millis();
    ledPatternStep = 0;
}

void simulateButton2Press() {
    log(LOG_INFO, "Simulating button 2 short press");
    currentLedPattern = LED_DOUBLE_FLASH;
    ledPatternStart = millis();
    ledPatternStep = 0;
}

// ---- Helper (local) implementations ----
static void resetMilestones() {
    milestone5s = milestone10s = milestone15s = milestone30s = false;
}

static void enterChannelSelectMode() {
    channelSelectMode = true;
    tempMidiChannel = (serverMidiChannel == 0) ? 1 : serverMidiChannel; // start from current (omni -> 1)
    lastChannelButtonPress = millis();
    logf(LOG_INFO, "Channel Select Mode: starting at channel %u", tempMidiChannel);
    currentLedPattern = LED_FADE;
    ledPatternStart = millis();
}

static void handleChannelSelectShortPress() {
    if (!channelSelectMode) return;
    // Increment channel 1..16 cycling
    tempMidiChannel++;
    if (tempMidiChannel > 16) tempMidiChannel = 1;
    lastChannelButtonPress = millis();
    logf(LOG_INFO, "Channel Select: temp channel -> %u", tempMidiChannel);
    currentLedPattern = LED_SINGLE_FLASH;
    ledPatternStart = millis();
}

static void handleChannelSelectAutoSave() {
    if (!channelSelectMode) return;
    if (millis() - lastChannelButtonPress > 5000) { // 5s inactivity
        serverMidiChannel = tempMidiChannel;
        saveServerMidiChannelToNVS();
        channelSelectMode = false;
        logf(LOG_INFO, "Channel Select: committed channel %u", serverMidiChannel);
        currentLedPattern = LED_TRIPLE_FLASH; // confirmation
        ledPatternStart = millis();
    }
}

static bool handleServerMidiLearnTimeout() {
    if (serverMidiLearnArmed && serverMidiLearnTarget >= 0) {
        if (millis() - serverMidiLearnStartTime > SERVER_MIDI_LEARN_TIMEOUT) {
            log(LOG_WARN, "Server MIDI Learn timed out");
            serverMidiLearnArmed = false;
            serverMidiLearnTarget = -1;
            pendingLearnTarget = -1;
            midiLearnJustTimedOut = true;
            currentLedPattern = LED_OFF;
            return true;
        }
        return true; // still in learn mode (block pairing trigger)
    }
    return false;
}

static void cycleLearnTarget() {
    if (!serverMidiLearnArmed) return;
    if (pendingLearnTarget < 0) pendingLearnTarget = 0;
    pendingLearnTarget++;
    if (pendingLearnTarget >= MAX_RELAY_CHANNELS) pendingLearnTarget = 0;
    serverMidiLearnTarget = pendingLearnTarget;
    logf(LOG_INFO, "MIDI Learn: target relay -> %d (press again to cycle)", serverMidiLearnTarget + 1);
    currentLedPattern = LED_SINGLE_FLASH;
    ledPatternStart = millis();
}
