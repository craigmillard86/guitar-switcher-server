#pragma once
#include <Arduino.h>

// Initialize MIDI UART (if enabled in config)
void initMidiInput();
// Poll / parse incoming MIDI and forward Program Change messages
void processMidiInput();
