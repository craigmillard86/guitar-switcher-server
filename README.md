# Guitar Switcher Server - Project Documentation

## Overview

The Guitar Switcher Server is an ESP32-based firmware designed to control and automate guitar rig switching. It acts as a central controller, communicating wirelessly with client devices (such as remote footswitches or sensor modules) using ESP-NOW, and can also process MIDI input for advanced control scenarios. The server manages relays, footswitches, and supports over-the-air (OTA) firmware updates.

## Key Features

- **Wireless Communication:** Uses ESP-NOW for low-latency, peer-to-peer communication with multiple clients.
- **Relay Control:** Controls relay outputs for switching audio paths, effects, or other hardware.
- **Footswitch Input:** Reads footswitch states for manual channel/effect switching.
- **MIDI Input:** Supports MIDI IN via UART for integration with MIDI controllers.
- **Pairing Mode:** Securely pairs with new client devices using a dedicated button and LED feedback.
- **OTA Updates:** Supports WiFi-based firmware updates using ElegantOTA, with a dedicated OTA mode.
- **Configuration Storage:** Uses NVS (non-volatile storage) to save configuration, peer info, and settings.
- **Debug & Monitoring:** Provides serial debug output, performance metrics, and system status reporting.

## Main Components

- **main.cpp:** Entry point; initializes hardware, configuration, WiFi/ESP-NOW, and runs the main loop.
- **config.h/cpp:** Handles pin assignments, device settings, and configuration initialization.
- **espnow-pairing.h/cpp:** Manages pairing process with new clients, including button/LED logic.
- **relayControl.h/cpp:** Controls relay outputs for switching.
- **midiInput.h/cpp:** Handles MIDI input parsing and processing.
- **otaManager.h/cpp:** Manages OTA update mode and ElegantOTA server.
- **nvsManager.h/cpp:** Handles saving/loading settings and peer info to/from NVS.
- **utils.h/cpp:** Utility functions for logging, command handling, and help menus.
- **debug.h/cpp:** Provides debug output, performance, and memory monitoring.

## How It Works

1. **Startup:**
   - Initializes serial, LEDs, relays, footswitches, and configuration.
   - Waits for 'ota' command or button to enter OTA update mode.
   - Loads saved settings and peers from NVS.
   - Initializes ESP-NOW and WiFi channel.

2. **Main Loop:**
   - Monitors footswitch and pairing button states.
   - Handles LED feedback and advanced patterns.
   - Processes MIDI input and sends commands to clients as needed.
   - Handles serial commands for debugging, configuration, and control.
   - Updates performance and memory metrics.

3. **Pairing:**
   - Enter pairing mode via button or command.
   - New clients can be added securely; LED provides feedback.

4. **Relay & MIDI Control:**
   - Footswitch or MIDI events trigger relay changes and send commands to clients.

5. **OTA Updates:**
   - Enter OTA mode via serial command or button at boot.
   - Device starts WiFi AP and ElegantOTA server for firmware upload.

## User Interaction

- **Serial Console:**
  - Provides command-line interface for configuration, debugging, and control.
  - Type 'help' for a list of available commands.
- **Pairing Button/LED:**
  - Press to enter pairing mode; LED indicates status.
- **Footswitches:**
  - Trigger relay and channel changes.
- **OTA Mode:**
  - Enter 'ota' at boot or press button to update firmware via web browser.

## Extensibility

- Add new relay or footswitch channels by updating configuration macros.
- Extend command handling in `utils.cpp` for custom serial commands.
- Integrate additional sensors or controls via ESP-NOW or MIDI.

## File Structure

- `src/` - Main source files (core logic, hardware control, communication)
- `include/` - Header files (APIs, configuration, data structures)
- `platformio.ini` - PlatformIO project configuration

## Getting Started

1. **Build and Upload:** Use PlatformIO to build and upload the firmware to your ESP32 device.
2. **Connect Serial Monitor:** Open a serial terminal at 115200 baud for logs and commands.
3. **Pair Clients:** Use the pairing button or serial command to add new client devices.
4. **Control Relays:** Use footswitches, MIDI, or serial commands to control relays and channels.
5. **OTA Updates:** Enter OTA mode and update firmware via web browser when needed.

---

For more details, see comments in the source files and use the serial 'help' command for runtime documentation.
