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
#include "config.h"
#include "commandHandler.h"
#include "pairing.h"
#include "globals.h"
#include "espnow-pairing.h"
#include "utils.h"
#include "nvsManager.h"
// Include for fast GPIO register access
#ifdef FAST_SWITCHING
#include <soc/gpio_reg.h>
#include <soc/io_mux_reg.h>
#endif

unsigned long midiLearnStartTime = 0;
static bool midiLearnJustTimedOut = false; // Flag to prevent pairing mode after MIDI Learn timeout
static unsigned long midiLearnCompleteTime = 0; // Time when MIDI Learn completed
static const unsigned long MIDI_LEARN_COOLDOWN = 2000; // 2 second cooldown after MIDI Learn

// Shared variables for both modes
static unsigned long lastLedFlash = 0;
static bool ledFlashed = false;
static bool milestone5s = false;
static bool milestone10s = false;
static bool milestone15s = false;
static bool milestone20s = false;
static bool milestone25s = false;
static bool milestone30s = false;

// Channel select variables for both modes
static bool channelSelectMode = false;
static uint8_t buttonPressCount = 0;
static uint8_t tempMidiChannel = 1;
static unsigned long channelSelectStart = 0;
static unsigned long lastChannelButtonPress = 0;

// Channel confirmation variables for non-blocking LED feedback
static bool showingChannelConfirmation = false;
static uint8_t confirmationChannel = 0;
static uint8_t confirmationFlashCount = 0;
static unsigned long lastConfirmationFlash = 0;
static bool confirmationLedState = false;

// Function to start non-blocking channel confirmation
void showChannelConfirmation(uint8_t channel) {
    showingChannelConfirmation = true;
    confirmationChannel = channel;
    confirmationFlashCount = 0;
    lastConfirmationFlash = millis();
    confirmationLedState = false;
    setStatusLedPattern(LED_OFF); // Start with LED off
}

// Function to handle non-blocking channel confirmation LED flashing
void updateChannelConfirmation() {
    if (!showingChannelConfirmation) return;
    
    unsigned long now = millis();
    
    if (confirmationFlashCount < confirmationChannel * 2) { // *2 because we flash on and off
        if (now - lastConfirmationFlash >= 200) { // 200ms intervals
            confirmationLedState = !confirmationLedState;
            if (confirmationLedState) {
                setStatusLedPattern(LED_SINGLE_FLASH);
            } else {
                setStatusLedPattern(LED_OFF);
            }
            confirmationFlashCount++;
            lastConfirmationFlash = now;
        }
    } else {
        // Finished flashing, return to normal operation
        showingChannelConfirmation = false;
        setStatusLedPattern(LED_OFF); // Return to normal operation (LED off)
    }
}

// Shared function to reset milestone flags
void resetMilestoneFlags() {
    milestone5s = false;
    milestone10s = false;
    milestone15s = false;
    milestone20s = false;
    milestone25s = false;
    milestone30s = false;
}

// Shared function to handle LED feedback
void handleLedFeedback(unsigned long held, const char* buttonName) {
    if (held >= 5000 && !milestone5s) {
        setStatusLedPattern(LED_SINGLE_FLASH);
        milestone5s = true;
        logf(LOG_INFO, "%s - 5s held - LED feedback (1 flash)", buttonName);
    } else if (held >= 10000 && !milestone10s) {
        setStatusLedPattern(LED_DOUBLE_FLASH);
        milestone10s = true;
        logf(LOG_INFO, "%s - 10s held - MIDI Learn ready (2 flashes)", buttonName);
    } else if (held >= 15000 && !milestone15s) {
        setStatusLedPattern(LED_TRIPLE_FLASH);
        milestone15s = true;
        logf(LOG_INFO, "%s - 15s held - Channel Select ready (3 flashes)", buttonName);
    } else if (held >= 20000 && !milestone20s) {
        setStatusLedPattern(LED_QUAD_FLASH);
        milestone20s = true;
        logf(LOG_INFO, "%s - 20s held - LED feedback (4 flashes)", buttonName);
    } else if (held >= 25000 && !milestone25s) {
        setStatusLedPattern(LED_PENTA_FLASH);
        milestone25s = true;
        logf(LOG_INFO, "%s - 25s held - LED feedback (5 flashes)", buttonName);
    } else if (held >= 30000 && !milestone30s) {
        setStatusLedPattern(LED_HEXA_FLASH);
        milestone30s = true;
        logf(LOG_INFO, "%s - 30s held - Pairing ready (6 flashes)", buttonName);
    }
}

// Shared function to handle channel select mode entry
void enterChannelSelectMode() {
    channelSelectMode = true;
    buttonPressCount = 0;
    tempMidiChannel = currentMidiChannel;
    channelSelectStart = millis();
    lastChannelButtonPress = millis(); // Initialize the last button press time
    log(LOG_INFO, "15s long press: Channel Select Mode Active!");
    setStatusLedPattern(LED_FADE);
}

// Shared function to handle channel selection
void handleChannelSelection() {
    buttonPressCount++;
    tempMidiChannel = ((buttonPressCount - 1) % 16) + 1;
    lastChannelButtonPress = millis(); // Update the last button press time
    logf(LOG_INFO, "Button press %d -> Channel %u", buttonPressCount, tempMidiChannel);
    
    // Immediate visual feedback - single flash to acknowledge button press
    setStatusLedPattern(LED_SINGLE_FLASH);
}

// Shared function to handle auto-save
void handleChannelSelectAutoSave() {
    if (channelSelectMode && (millis() - lastChannelButtonPress > 5000)) {
        currentMidiChannel = tempMidiChannel;
        saveMidiChannelToNVS();
        channelSelectMode = false;
        logf(LOG_INFO, "Channel %u selected and saved", currentMidiChannel);
        
        // Non-blocking LED feedback showing the selected channel number
        // This will be handled by a separate function to avoid blocking
        showChannelConfirmation(currentMidiChannel);
    }
}

// Forward declarations for button handler helper functions
bool handleMidiLearnTimeout();
void processButtonState(int buttonIndex, uint8_t reading, 
                       unsigned long* lastDebounceTime, uint8_t* lastButtonState,
                       bool* buttonPressed, unsigned long* buttonPressStart,
                       bool* buttonLongPressHandled);
void handleButtonPress(int buttonIndex, bool* buttonPressed, unsigned long* buttonPressStart,
                      bool* buttonLongPressHandled);
void handleButtonHeld(int buttonIndex, unsigned long held);
void handleButtonRelease(int buttonIndex, unsigned long held, bool* buttonPressed,
                        bool* buttonLongPressHandled);

void checkAmpChannelButtons() {
    // Skip button checking if disabled (when buttons aren't connected)
    if (!enableButtonChecking) {
        return;
    }
    
    static unsigned long lastDebounceTime[MAX_AMPSWITCHS] = {0};
    static uint8_t lastButtonState[MAX_AMPSWITCHS] = {HIGH};
    static bool buttonPressed[MAX_AMPSWITCHS] = {false};
    static unsigned long buttonPressStart[MAX_AMPSWITCHS] = {0};
    static bool buttonLongPressHandled[MAX_AMPSWITCHS] = {false};

    // Block all button actions during MIDI Learn lockout
    if (handleMidiLearnTimeout()) {
        // Handle timeout and reset button states
        for (int i = 0; i < MAX_AMPSWITCHS; i++) {
            buttonLongPressHandled[i] = true; // Mark as handled to prevent further actions
        }
        return;
    }

    // Unified button processing for all buttons with bounds checking
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        // Validate array index bounds
        if (i < 0 || i >= MAX_AMPSWITCHS) {
            logf(LOG_ERROR, "Button index out of bounds: %d", i);
            break;
        }
        
        // Validate pin configuration
        if (ampButtonPins[i] < 0 || ampButtonPins[i] > 255) {
            logf(LOG_ERROR, "Invalid button pin %d at index %d", ampButtonPins[i], i);
            continue;
        }
        
        uint8_t reading = digitalRead(ampButtonPins[i]);
        
        processButtonState(i, reading, lastDebounceTime, lastButtonState,
                          buttonPressed, buttonPressStart, buttonLongPressHandled);
    }
    
    // Update non-blocking channel confirmation LED feedback
    updateChannelConfirmation();
    
    // Shared auto-save logic
    handleChannelSelectAutoSave();
}

bool handleMidiLearnTimeout() {
    if (midiLearnChannel >= 0) {
        // Timeout check
        if (millis() - midiLearnStartTime > MIDI_LEARN_TIMEOUT) {
            log(LOG_WARN, "MIDI Learn timed out, exiting learn mode.");
            midiLearnArmed = false;
            midiLearnChannel = -1;
            midiLearnJustTimedOut = true; // Set flag to prevent pairing mode trigger
            setStatusLedPattern(LED_OFF);
            return true;
        }
        return true; // Still in MIDI learn mode, block other button actions
    }
    return false;
}

void processButtonState(int buttonIndex, uint8_t reading, 
                       unsigned long* lastDebounceTime, uint8_t* lastButtonState,
                       bool* buttonPressed, unsigned long* buttonPressStart,
                       bool* buttonLongPressHandled) {
    const unsigned long debounceDelay = BUTTON_DEBOUNCE_MS;
    
    // Debounce check
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
    
    if (buttonIndex == 0) { // Only button 1 gets LED feedback
        ledFlashed = false;
        lastLedFlash = 0;
        resetMilestoneFlags();
    }
    
    // Multi-button MIDI Learn channel selection
    #if MAX_AMPSWITCHS > 1
    if (midiLearnArmed) {
        midiLearnChannel = buttonIndex;
        midiLearnArmed = false;
        midiLearnStartTime = millis();
        logf(LOG_INFO, "MIDI Learn: Waiting for MIDI PC for channel %d", buttonIndex+1);
        setStatusLedPattern(LED_TRIPLE_FLASH);
    }
    #endif
}

void handleButtonHeld(int buttonIndex, unsigned long held) {
    // Only button 1 supports long press functions
    if (buttonIndex == 0) {
        // LED feedback for button 1
        handleLedFeedback(held, "Button 1");
    }
}

void handleButtonRelease(int buttonIndex, unsigned long held, bool* buttonPressed,
                        bool* buttonLongPressHandled) {
    const unsigned long longPressTime = BUTTON_LONGPRESS_MS;
    const unsigned long midiLearnActivationTime = 10000; // 10 seconds for MIDI Learn
    
    if (!buttonLongPressHandled[buttonIndex]) {
        logf(LOG_DEBUG, "Button %d released after %lu ms, channelSelectMode=%d, midiLearnJustTimedOut=%d", 
             buttonIndex, held, channelSelectMode ? 1 : 0, midiLearnJustTimedOut ? 1 : 0);
        
        if (channelSelectMode) {
            // In channel select mode, any button can be used for selection
            handleChannelSelection();
        } else if (held >= 30000 && !midiLearnJustTimedOut && buttonIndex == 0) {
            // Pairing mode (30s+ hold released) - only button 1, not if MIDI Learn just timed out
            clearPairingNVS();
            resetPairingToDefaults();
            pairingStatus = PAIR_REQUEST; // Override to start pairing process
            log(LOG_INFO, "30s+ hold released: Pairing mode triggered!");
            channelSelectMode = false;
        } else if (held >= 15000 && buttonIndex == 0) {
            // Channel Select Mode (15s+ hold released) - only button 1
            enterChannelSelectMode();
        } else if (held >= midiLearnActivationTime && buttonIndex == 0) {
            // MIDI Learn mode (10s+ hold released) - only button 1, unified for both single and multi-button
            midiLearnArmed = true;
            #if MAX_AMPSWITCHS == 1
            midiLearnChannel = 0; // Single channel device - start learning immediately
            midiLearnStartTime = millis();
            log(LOG_INFO, "10s+ hold released: MIDI Learn mode armed for single channel.");
            #else
            log(LOG_INFO, "10s+ hold released: MIDI Learn mode armed. Press a channel button to select.");
            #endif
            setStatusLedPattern(LED_FAST_BLINK);
        } else if (held < longPressTime) {
            // Short press handling
            // Check cooldown period after MIDI Learn completion
            if (midiLearnCompleteTime > 0 && (millis() - midiLearnCompleteTime < MIDI_LEARN_COOLDOWN)) {
                log(LOG_DEBUG, "Button press ignored during post-learn cooldown period");
            } else if (!midiLearnArmed) {
                #if MAX_AMPSWITCHS == 1
                // Single button: toggle relay - FAST PATH
                if (currentAmpChannel == 1) {
                    setAmpChannel(0);
                    // Defer logging to avoid button response delay
                    #if LOG_LEVEL >= LOG_INFO
                    log(LOG_INFO, "Toggled relay OFF");
                    #endif
                } else {
                    setAmpChannel(1);
                    #if LOG_LEVEL >= LOG_INFO
                    log(LOG_INFO, "Toggled relay ON");
                    #endif
                }
                #else
                // Multi-button: switch to specific channel
                setAmpChannel(buttonIndex + 1);
                #if LOG_LEVEL >= LOG_INFO
                logf(LOG_INFO, "Button %d: channel %d", buttonIndex + 1, buttonIndex + 1);
                #endif
                #endif
            }
        }
    }
    
    buttonPressed[buttonIndex] = false;
    buttonLongPressHandled[buttonIndex] = false;
    midiLearnJustTimedOut = false; // Reset flag when any button is released
    if (buttonIndex == 0) {
        resetMilestoneFlags();
    }
}

void handleCommand(uint8_t commandType, uint8_t value) {
    logf(LOG_DEBUG, "Received command - Type: %u, Value: %u", commandType, value);
    
    switch (commandType) {
        case PROGRAM_CHANGE: {
            // Unified handling: interpret 'value' primarily as a MIDI Program Number.
            // If it matches a learned mapping (midiChannelMap[channelIndex] == value), switch to that channel.
            // Fallbacks:
            //   value == 0            -> all channels off
            //   1..MAX_AMPSWITCHS     -> treat as direct channel selection (legacy direct mode)
            //   otherwise             -> ignore (no mapping)
            uint8_t program = value;
            bool handled = false;

#if MAX_AMPSWITCHS == 1
            // Single channel device logic
            if (program == 0) {
                log(LOG_INFO, "Remote: Program 0 -> all off");
                setAmpChannel(0);
                setStatusLedPattern(LED_DOUBLE_FLASH); // distinct off feedback
                handled = true;
            } else if (program == midiChannelMap[0] || program == 1) {
                // Learned program or legacy '1' acts as TOGGLE
                if (currentAmpChannel == 1) {
                    logf(LOG_INFO, "Remote: Program %u -> toggle OFF", program);
                    setAmpChannel(0);
                    setStatusLedPattern(LED_DOUBLE_FLASH);
                } else {
                    logf(LOG_INFO, "Remote: Program %u -> toggle ON", program);
                    setAmpChannel(1);
                    setStatusLedPattern(LED_TRIPLE_FLASH);
                }
                handled = true;
            }
#else
            // Multi-channel device: search mapping first
            int mappedChannel = -1;
            for (int i = 0; i < MAX_AMPSWITCHS; i++) {
                if (midiChannelMap[i] == program) {
                    mappedChannel = i + 1;
                    break;
                }
            }
            if (program == 0) {
                log(LOG_INFO, "Remote: Program 0 -> all off");
                setAmpChannel(0);
                setStatusLedPattern(LED_DOUBLE_FLASH);
                handled = true;
            } else if (mappedChannel != -1) {
                logf(LOG_INFO, "Remote: Program %u mapped -> channel %d", program, mappedChannel);
                setAmpChannel(mappedChannel);
                setStatusLedPattern(LED_TRIPLE_FLASH);
                handled = true;
            } else if (program >= 1 && program <= MAX_AMPSWITCHS) {
                // Legacy direct mode (server sending raw channel number)
                logf(LOG_INFO, "Remote: Direct channel select %u", program);
                setAmpChannel(program);
                setStatusLedPattern(LED_SINGLE_FLASH);
                handled = true;
            }
#endif
            if (!handled) {
                logf(LOG_DEBUG, "Remote: Program %u has no mapping (ignored)", program);
            }
            break; }
        case RESERVED1:
            // Reserved / legacy - ignore silently
            break;
        case ALL_CHANNELS_OFF:
            log(LOG_INFO, "All channels off command received");
            setAmpChannel(0);
            setStatusLedPattern(LED_DOUBLE_FLASH);
            break;
        case STATUS_REQUEST:
            logf(LOG_INFO, "Status request received - current channel: %u", currentAmpChannel);
            // Could send back status if needed in the future
            setStatusLedPattern(LED_SINGLE_FLASH);
            break;
        default:
            logf(LOG_WARN, "Unknown command received - Type: %u, Value: %u", commandType, value);
            break;
    }
}

void handleProgramChange(byte midiChannel, byte program) {
    // Validate MIDI parameters
    if (midiChannel > 16) {
        logf(LOG_ERROR, "Invalid MIDI channel: %u (must be 1-16)", midiChannel);
        return;
    }
    
    if (program > 127) {
        logf(LOG_ERROR, "Invalid MIDI program: %u (must be 0-127)", program);
        return;
    }
    
    if (midiChannel != currentMidiChannel) return; // Only respond to selected channel

#if MAX_AMPSWITCHS == 1
    // MIDI Learn mode - standardized timeout handling
    if (midiLearnChannel >= 0) {
        // Validate learn channel bounds
        if (midiLearnChannel < 0 || midiLearnChannel >= MAX_AMPSWITCHS) {
            logf(LOG_ERROR, "Invalid MIDI learn channel: %d", midiLearnChannel);
            midiLearnChannel = -1;
            midiLearnArmed = false;
            return;
        }
        
        // Check timeout
        if (millis() - midiLearnStartTime > MIDI_LEARN_TIMEOUT) {
            log(LOG_WARN, "MIDI Learn timed out, exiting learn mode.");
            midiLearnArmed = false;
            midiLearnChannel = -1;
            setStatusLedPattern(LED_OFF);
            return;
        }
        
        // Learn the MIDI PC mapping with bounds checking
        if (midiLearnChannel >= 0 && midiLearnChannel < MAX_AMPSWITCHS) {
            midiChannelMap[midiLearnChannel] = program;
            saveMidiMapToNVS();
            logf(LOG_INFO, "MIDI PC#%u assigned to channel 1", program);
            setStatusLedPattern(LED_SINGLE_FLASH);
            midiLearnChannel = -1;
            midiLearnArmed = false;
            midiLearnCompleteTime = millis(); // Set cooldown time
        } else {
            logf(LOG_ERROR, "Learn channel %d out of bounds", midiLearnChannel);
        }
        return;
    }
    
    // Normal operation: toggle relay when mapped PC is received
    // Check cooldown period after MIDI Learn completion
    if (midiLearnCompleteTime > 0 && (millis() - midiLearnCompleteTime < MIDI_LEARN_COOLDOWN)) {
        log(LOG_DEBUG, "MIDI PC ignored during post-learn cooldown period");
        return;
    }
    
    // Validate array access before checking MIDI map
    if (0 < MAX_AMPSWITCHS && program == midiChannelMap[0]) {
        // FAST MIDI switching - minimal logging
        if (currentAmpChannel == 1) {
            setAmpChannel(0);
        } else {
            setAmpChannel(1);
        }
        setStatusLedPattern(LED_TRIPLE_FLASH);
        
        // Optional logging only if debug enabled
        #if LOG_LEVEL >= LOG_INFO
        log(LOG_INFO, "MIDI PC: Toggled relay");
        #endif
    }
    return;
#else
    // Multi-button mode - MIDI Learn with timeout handling
    if (midiLearnChannel >= 0) {
        // Check timeout
        if (millis() - midiLearnStartTime > MIDI_LEARN_TIMEOUT) {
            log(LOG_WARN, "MIDI Learn timed out, exiting learn mode.");
            midiLearnArmed = false;
            midiLearnChannel = -1;
            return;
        }
        
        // Learn the MIDI PC mapping
        midiChannelMap[midiLearnChannel] = program;
        saveMidiMapToNVS();
        logf(LOG_INFO, "MIDI PC#%u assigned to channel %d", program, midiLearnChannel + 1);
        setStatusLedPattern(LED_SINGLE_FLASH);
        midiLearnChannel = -1;
        midiLearnCompleteTime = millis(); // Set cooldown time
        return;
    }
    
    // Normal operation: use mapping
    // Check cooldown period after MIDI Learn completion
    if (midiLearnCompleteTime > 0 && (millis() - midiLearnCompleteTime < MIDI_LEARN_COOLDOWN)) {
        log(LOG_DEBUG, "MIDI PC ignored during post-learn cooldown period");
        return;
    }
    
    // FAST MIDI switching for multi-channel - optimized loop
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        if (midiChannelMap[i] == program) {
            setAmpChannel(i + 1);
            setStatusLedPattern(LED_TRIPLE_FLASH);
            
            // Optional logging only if debug enabled
            #if LOG_LEVEL >= LOG_INFO
            logf(LOG_INFO, "MIDI PC: Channel %d", i + 1);
            #endif
            return;
        }
    }
    
    // No mapping found - minimal logging
    #if LOG_LEVEL >= LOG_DEBUG
    logf(LOG_DEBUG, "MIDI PC#%u: No mapping", program);
    #endif
#endif
}

void setAmpChannel(uint8_t channel) {
    // Ultra-fast path for single channel mode
    #if MAX_AMPSWITCHS == 1 && defined(FAST_SWITCHING)
    
    if (channel > 1 || channel == currentAmpChannel) return;
    
    // Direct GPIO register access for maximum speed (ESP32-C3)
    // Use ESP32-C3 specific GPIO registers
    if (channel == 0) {
        // Clear bit (LOW) - direct register write
        REG_WRITE(GPIO_OUT_W1TC_REG, (1UL << ampSwitchPins[0]));
        currentAmpChannel = 0;
    } else {
        // Set bit (HIGH) - direct register write
        REG_WRITE(GPIO_OUT_W1TS_REG, (1UL << ampSwitchPins[0]));
        currentAmpChannel = 1;
    }
    
    // No logging in fast mode - adds ~500μs delay
    
    #elif MAX_AMPSWITCHS == 1
    // Standard fast path
    if (channel > 1) return;
    if (channel == currentAmpChannel) return;
    
    if (channel == 0) {
        digitalWrite(ampSwitchPins[0], LOW);
        currentAmpChannel = 0;
    } else {
        digitalWrite(ampSwitchPins[0], HIGH);
        currentAmpChannel = 1;
    }
    
    #else
    // Multi-channel mode - optimized for speed when FAST_SWITCHING enabled
    #ifdef FAST_SWITCHING
    // Ultra-fast multi-channel with direct register access
    if (channel > MAX_AMPSWITCHS) return;
    if (channel == currentAmpChannel) return;
    
    // Turn all channels OFF - direct register writes
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        REG_WRITE(GPIO_OUT_W1TC_REG, (1UL << ampSwitchPins[i]));
    }
    
    // Turn ON the requested channel (if valid)
    if (channel >= 1 && channel <= MAX_AMPSWITCHS) {
        int pinIndex = channel - 1;
        REG_WRITE(GPIO_OUT_W1TS_REG, (1UL << ampSwitchPins[pinIndex]));
        currentAmpChannel = channel;
    } else if (channel == 0) {
        currentAmpChannel = 0;
    }
    
    // No logging in fast mode
    
    #else
    // Standard multi-channel mode with full validation and logging
    if (channel > MAX_AMPSWITCHS) {
        logf(LOG_ERROR, "Invalid channel %u requested (max: %d)", channel, MAX_AMPSWITCHS);
        return;
    }
    
    if (channel == currentAmpChannel) {
        logf(LOG_DEBUG, "Channel %u already active, ignoring", channel);
        return;
    }

    logf(LOG_INFO, "Switching amp channel from %u to %u", currentAmpChannel, channel);

    // Turn all channels OFF
    for (int i = 0; i < MAX_AMPSWITCHS; i++) {
        // Validate array bounds and pin values
        if (i < 0 || i >= MAX_AMPSWITCHS) {
            logf(LOG_ERROR, "Switch index out of bounds: %d", i);
            break;
        }
        
        if (ampSwitchPins[i] < 0 || ampSwitchPins[i] > 255) {
            logf(LOG_ERROR, "Invalid switch pin %d at index %d", ampSwitchPins[i], i);
            continue;
        }
        
        digitalWrite(ampSwitchPins[i], LOW);
    }

    // Turn ON the requested channel
    if (channel >= 1 && channel <= MAX_AMPSWITCHS) {
        int pinIndex = channel - 1;
        digitalWrite(ampSwitchPins[pinIndex], HIGH);
        currentAmpChannel = channel;
        logf(LOG_INFO, "Amp channel %u activated", channel);
    } else if (channel == 0) {
        currentAmpChannel = 0;
        log(LOG_INFO, "All amp channels turned off");
    } else {
        currentAmpChannel = 0; // None selected
        logf(LOG_WARN, "Invalid channel number: %u (valid range: 0-%d)", channel, MAX_AMPSWITCHS);
    }
    #endif
    #endif
}