#include <config.h>
#include <utils.h>
#include <commandSender.h>
#include <MIDI.h>
#include <globals.h>
#include <relayControl.h>

// Ensure this translation unit only compiled once; if included via another source accidentally, guard with unique macro.
#ifdef SERVER_MIDI_INPUT_SOURCE
#error "SERVER_MIDI_INPUT_SOURCE re-included"
#endif
#define SERVER_MIDI_INPUT_SOURCE

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, SERVER_MIDI);
static uint8_t lastProgram = 0xFF;
static bool lastProgramOn = false;

void serverHandleProgramChange(byte channel, byte program) {
    // Channel filter (0 = omni)
    if (serverMidiChannel != 0 && channel != serverMidiChannel) return;

    // Ignore during post-learn cooldown
    if (!serverMidiLearnArmed && serverMidiLearnCompleteTime > 0 && (millis() - serverMidiLearnCompleteTime) < SERVER_MIDI_LEARN_COOLDOWN) {
        return; // suppress spurious immediate repeats like client cooldown
    }

    // MIDI Learn handling (single mapping write per armed session)
    if (serverMidiLearnArmed && serverMidiLearnTarget >= 0) {
        if (serverMidiLearnTarget < MAX_RELAY_CHANNELS) {
            serverMidiChannelMap[serverMidiLearnTarget] = program;
            saveServerMidiMapToNVS();
            logf(LOG_INFO, "Server MIDI Learn: mapped Program %u to relay %d", program, serverMidiLearnTarget + 1);
        } else {
            logf(LOG_ERROR, "Server MIDI Learn target %d out of bounds", serverMidiLearnTarget);
        }
        serverMidiLearnArmed = false;
        serverMidiLearnTarget = -1;
        currentLedPattern = LED_SINGLE_FLASH;
        ledPatternStart = millis();
        lastProgram = program;
        serverMidiLearnCompleteTime = millis();
        return;
    }

    // Forward to clients always
    forwardMidiProgramToAll(program);

    // Mapping-driven behavior
#if MAX_RELAY_CHANNELS == 1
    // Single relay: toggle only when incoming PC matches mapping[0]
    if (serverMidiChannelMap[0] == program) {
        if (lastProgramOn) {
            setRelayChannel(0); // off
            lastProgramOn = false;
            logf(LOG_INFO, "Server MIDI: PC %u -> Relay OFF (toggle)", program);
        } else {
            setRelayChannel(1);
            lastProgramOn = true;
            logf(LOG_INFO, "Server MIDI: PC %u -> Relay ON (toggle)", program);
        }
        currentLedPattern = LED_TRIPLE_FLASH;
        ledPatternStart = millis();
    } else {
        // Not mapped -> ignore (debug only)
        logf(LOG_DEBUG, "Server MIDI: PC %u no mapping", program);
    }
#else
    bool matched = false;
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        if (serverMidiChannelMap[i] == program) {
            setRelayChannel(i + 1);
            logf(LOG_INFO, "Server MIDI: PC %u -> Relay %d", program, i + 1);
            currentLedPattern = LED_TRIPLE_FLASH;
            ledPatternStart = millis();
            matched = true;
            break;
        }
    }
    if (!matched) {
        logf(LOG_DEBUG, "Server MIDI: PC %u no mapping", program);
    }
#endif
    lastProgram = program;
}

// Initialize server MIDI input (single definition)
void initMidiInput() {
    Serial1.begin(MIDI_BAUD_RATE, SERIAL_8N1, MIDI_UART_RX_PIN, -1); // RX only
    SERVER_MIDI.setHandleProgramChange(serverHandleProgramChange);
    SERVER_MIDI.begin(MIDI_CHANNEL_OMNI);
    logf(LOG_INFO, "Server MIDI initialized RX pin %d", MIDI_UART_RX_PIN);
}

// Poll MIDI input - call in main loop
void processMidiInput() {
    SERVER_MIDI.read();
}
 

