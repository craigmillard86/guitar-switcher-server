# Guitar Switcher Web Configuration

This feature adds a comprehensive web-based configuration interface for mapping footswitches to various actions including relay control, MIDI commands, and scene management.

## Features

### 🎸 Footswitch Mapping
- Map up to 8 footswitches to different actions
- Support for short press and long press configurations
- Real-time testing of mappings from the web interface

### 🎛️ Action Types
1. **Relay Control**
   - Toggle relays on/off
   - Momentary relay activation with configurable duration
   - Control individual relays or all relays simultaneously

2. **MIDI Commands**
   - Send MIDI locally via UART
   - Send MIDI commands via ESP-NOW to connected devices
   - Support for Control Change, Program Change, Note On/Off messages
   - Configurable MIDI channels and data values

3. **Program Changes**
   - Send program change commands to all connected devices
   - Useful for switching amp channels or effects

4. **Scene Management**
   - Create and recall scenes (snapshots of relay states)
   - Up to 8 scenes with custom names
   - Capture current system state as a new scene

5. **System Control**
   - All channels off command
   - Custom actions for specialized use cases

### 🌐 Web Interface
- Modern, responsive design optimized for mobile and desktop
- Real-time system status monitoring
- Configuration import/export functionality
- Live testing of footswitch actions
- Device management and peer information

## How to Access Configuration Mode

### Method 1: Double-Tap Button Trigger
1. Power on the device
2. Within 10 seconds of startup, perform a double-tap and hold on the pairing button:
   - Quick press and release
   - Wait up to 2 seconds
   - Press and hold for 1 second
3. Device will enter configuration mode and create a WiFi access point

### Method 2: Via WiFiManager
If the device can connect to your existing WiFi network, it will use that connection and provide the configuration interface at the device's IP address.

## Using the Web Interface

### 🔗 Connection
- **Access Point Mode**: Connect to "Guitar_Switcher_Config" network (password: "configure123")
- **Station Mode**: Access via device IP address on your existing network
- **Interface URL**: http://192.168.4.1 (AP mode) or http://[device-ip] (station mode)

### 📝 Configuration Steps

1. **Open the Configuration Interface**
   - Navigate to the device's web interface
   - You'll see three main tabs: Footswitches, Scenes, and System

2. **Configure Footswitch Mappings**
   - Click "Add Mapping" to create a new footswitch assignment
   - Select the action type (relay, MIDI, scene, etc.)
   - Configure the specific parameters for your chosen action
   - Enable/disable mappings as needed
   - Use the "Test" button to verify functionality

3. **Set Up Scenes**
   - Create scenes by manually setting relay states
   - Use "Capture Current" to save the current system state
   - Name scenes for easy identification
   - Test scene recall functionality

4. **Save Configuration**
   - Click "Save All" to persist your configuration to device memory
   - Export configurations for backup or sharing between devices
   - Import previously saved configurations

## Configuration Examples

### Basic Relay Toggle
```
Footswitch 1: Toggle Relay 1
- Action Type: Toggle Relay
- Target Channel: 1
- Description: "Lead Channel"
```

### MIDI Program Change
```
Footswitch 2: Send Program Change
- Action Type: MIDI ESP-NOW
- MIDI Type: Program Change
- MIDI Channel: 1
- Program Number: 5
- Description: "Crunch Sound"
```

### Scene Recall
```
Footswitch 3: Recall Scene
- Action Type: Recall Scene
- Scene: "Clean Setup"
- Description: "Clean guitar tone"
```

### Momentary Relay
```
Footswitch 4: Momentary Boost
- Action Type: Momentary Relay
- Target Channel: 3
- Duration: 1000ms
- Description: "Solo Boost"
```

## Technical Details

### 🔧 Configuration Storage
- All configurations are stored in ESP32 NVS (Non-Volatile Storage)
- Automatic backup and restore functionality
- Configuration versioning for future compatibility

### 📡 Communication
- ESP-NOW protocol for wireless device communication
- UART MIDI for local MIDI output
- WebServer on port 80 for configuration interface

### 💾 Data Format
- JSON-based configuration format for easy export/import
- Human-readable structure for troubleshooting
- Validation to prevent invalid configurations

### ⚡ Performance
- Non-blocking configuration interface
- Minimal impact on real-time audio switching
- Efficient NVS usage for fast boot times

## Troubleshooting

### Configuration Mode Won't Start
- Ensure proper button timing (double-tap within 2 seconds, hold for 1 second)
- Check that device has power and is not already in pairing mode
- Verify button connections and functionality

### Can't Connect to Web Interface
- Verify WiFi connection to "Guitar_Switcher_Config" network
- Check device IP address if using station mode
- Ensure firewall is not blocking port 80

### Settings Not Saving
- Verify "Save All" button was clicked after making changes
- Check device memory usage in System tab
- Restart device if NVS becomes corrupted

### Actions Not Working
- Use "Test" button to verify individual mappings
- Check physical connections for relays and MIDI
- Verify ESP-NOW peer connections in System tab

## Future Enhancements

- MIDI learn functionality for easier setup
- Advanced scene sequencing
- Backup to external storage (SD card)
- Mobile app companion
- Cloud configuration sync
- Advanced MIDI routing and filtering

## Safety Notes

- Always test configurations thoroughly before live use
- Keep backup configurations for important setups
- Avoid configuring during live performances
- Monitor system status for any connectivity issues

---

For technical support or feature requests, please refer to the project documentation or submit an issue on the project repository.
