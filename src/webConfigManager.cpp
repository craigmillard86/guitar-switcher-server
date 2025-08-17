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
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <globals.h>
#include <utils.h>
#include <webConfigManager.h>
#include <webAssets.h>
#include <footswitchConfig.h>
#include <nvsManager.h>
#include <relayControl.h>
#include <espnow-pairing.h>

WebServer configServer(80);
ConfigMode currentConfigMode = CONFIG_MODE_DISABLED;

// Forward declarations for helper functions
void setupConfigServer();
bool sendMessageToPeer(uint8_t* macAddr, struct_message* msg);
bool broadcastMessage(struct_message* msg);

// Check if configuration mode should start (different pattern than OTA)
bool checkConfigTrigger() {
    // Check for serial trigger first
    if (serialConfigTrigger) {
        serialConfigTrigger = false;  // Reset the trigger
        return true;
    }
    
    pinMode(PAIRING_BUTTON_PIN, INPUT_PULLUP);
    
    // Look for double-tap pattern: press, release, press within 2 seconds
    unsigned long firstPressStart = 0;
    bool firstPressDetected = false;
    
    // Wait for first press
    unsigned long startTime = millis();
    while (millis() - startTime < 1000) {
        if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
            firstPressStart = millis();
            firstPressDetected = true;
            break;
        }
        delay(10);
    }
    
    if (!firstPressDetected) return false;
    
    // Wait for release
    while (digitalRead(PAIRING_BUTTON_PIN) == LOW && millis() - firstPressStart < 500) {
        delay(10);
    }
    
    // Wait for second press within 2 seconds
    unsigned long releaseTime = millis();
    while (millis() - releaseTime < 2000) {
        if (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
            // Second press detected - check if held for 1 second
            unsigned long secondPressStart = millis();
            while (digitalRead(PAIRING_BUTTON_PIN) == LOW) {
                if (millis() - secondPressStart >= 1000) {
                    return true;  // Configuration mode triggered
                }
                delay(10);
            }
        }
        delay(10);
    }
    
    return false;
}

// Start configuration mode with WiFiManager
void startConfigurationMode() {
    log(LOG_INFO, "=== Starting Configuration Mode ===");
    currentConfigMode = CONFIG_MODE_ACTIVE;
    
    WiFiManager wm;
    
    if (!wm.autoConnect("Guitar_Switcher_Config")) {
        log(LOG_ERROR, "Failed to connect to WiFi during configuration");
        ESP.restart();
    }
    
    log(LOG_INFO, "WiFi connected for configuration");
    char ipStr[16];
    WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));
    logf(LOG_INFO, "Configuration interface available at: http://%s", ipStr);
    
    setupConfigServer();
    
    unsigned long start = millis();
    const unsigned long TIMEOUT = 10 * 60 * 1000; // 10 minutes
    log(LOG_INFO, "Configuration mode active for 10 minutes");
    
    while (millis() - start < TIMEOUT && currentConfigMode != CONFIG_MODE_DISABLED) {
        configServer.handleClient();
        delay(10);
    }
    
    log(LOG_INFO, "Configuration mode timeout or exit requested");
    currentConfigMode = CONFIG_MODE_DISABLED;
    ESP.restart();
}

// Start configuration mode as AP
void startConfigurationAP() {
    log(LOG_INFO, "=== Starting Configuration AP Mode ===");
    currentConfigMode = CONFIG_MODE_ACTIVE;
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    
    const char* ssid = "Guitar_Switcher_Config";
    const char* password = "configure123";
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    
    logf(LOG_INFO, "Configuration AP started: %s", ssid);
    logf(LOG_INFO, "AP IP: %s", WiFi.softAPIP().toString().c_str());
    
    setupConfigServer();
    
    unsigned long start = millis();
    const unsigned long TIMEOUT = 10 * 60 * 1000; // 10 minutes
    
    while (millis() - start < TIMEOUT && currentConfigMode != CONFIG_MODE_DISABLED) {
        configServer.handleClient();
        delay(10);
    }
    
    log(LOG_INFO, "Configuration AP mode timeout");
    currentConfigMode = CONFIG_MODE_DISABLED;
    ESP.restart();
}

void setupConfigServer() {
    // Main configuration page
    configServer.on("/", HTTP_GET, handleConfigRoot);
    
    // API endpoints
    configServer.on("/api/config", HTTP_GET, handleConfigLoad);
    configServer.on("/api/config", HTTP_POST, handleConfigSave);
    configServer.on("/api/test", HTTP_POST, handleConfigTest);
    configServer.on("/api/status", HTTP_GET, handleSystemStatus);
    configServer.on("/api/export", HTTP_GET, handleConfigExport);
    configServer.on("/api/import", HTTP_POST, handleConfigImport);
    configServer.on("/api/reboot", HTTP_POST, handleReboot);
    
    // Serve static assets
    configServer.on("/style.css", HTTP_GET, []() {
        configServer.send(200, "text/css", getConfigCSS());
    });
    
    configServer.on("/script.js", HTTP_GET, []() {
        configServer.send(200, "application/javascript", getConfigJS());
    });
    
    configServer.begin();
    log(LOG_INFO, "Configuration web server started");
}

void handleConfigRoot() {
    String html = getConfigHTML();
    configServer.send(200, "text/html", html);
}

void handleConfigLoad() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    String json = footswitchConfigToJson();
    configServer.send(200, "application/json", json);
}

void handleConfigSave() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!configServer.hasArg("plain")) {
        configServer.send(400, "application/json", "{\"error\":\"No configuration data received\"}");
        return;
    }
    
    String jsonData = configServer.arg("plain");
    
    if (footswitchConfigFromJson(jsonData)) {
        saveFootswitchConfigToNVS();
        configServer.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved successfully\"}");
        log(LOG_INFO, "Footswitch configuration saved via web interface");
    } else {
        configServer.send(400, "application/json", "{\"error\":\"Invalid configuration data\"}");
        log(LOG_ERROR, "Failed to parse configuration data from web interface");
    }
}

void handleConfigTest() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!configServer.hasArg("plain")) {
        configServer.send(400, "application/json", "{\"error\":\"No test data received\"}");
        return;
    }
    
    JsonDocument doc;
    deserializeJson(doc, configServer.arg("plain"));
    
    uint8_t footswitchIndex = doc["footswitch"];
    bool isLongPress = doc["longPress"] | false;
    
    currentConfigMode = CONFIG_MODE_TESTING;
    bool result = executeFootswitchAction(footswitchIndex, isLongPress);
    currentConfigMode = CONFIG_MODE_ACTIVE;
    
    if (result) {
        configServer.send(200, "application/json", "{\"success\":true,\"message\":\"Test action executed\"}");
    } else {
        configServer.send(400, "application/json", "{\"error\":\"Test action failed\"}");
    }
}

void handleSystemStatus() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    JsonDocument doc;
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["deviceName"] = DEVICE_NAME;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis();
    doc["espnowChannel"] = chan;
    doc["connectedPeers"] = numClients;
    
    // Relay status
    JsonArray relays = doc["relayStates"].to<JsonArray>();
    // You'll need to implement getRelayState() function
    for (int i = 0; i < MAX_RELAY_CHANNELS; i++) {
        // relays.add(getRelayState(i));
        relays.add(false); // Placeholder
    }
    
    // Peer information
    JsonArray peers = doc["peers"].to<JsonArray>();
    for (int i = 0; i < numLabeledPeers; i++) {
        JsonObject peer = peers.add<JsonObject>();
        peer["name"] = labeledPeers[i].name;
        
        String macStr = "";
        for (int j = 0; j < 6; j++) {
            if (j > 0) macStr += ":";
            macStr += String(labeledPeers[i].mac[j], HEX);
        }
        peer["mac"] = macStr;
    }
    
    String response;
    serializeJson(doc, response);
    configServer.send(200, "application/json", response);
}

void handleConfigExport() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    configServer.sendHeader("Content-Disposition", "attachment; filename=\"guitar_switcher_config.json\"");
    
    String json = footswitchConfigToJson();
    configServer.send(200, "application/json", json);
}

void handleConfigImport() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    // Handle file upload
    if (configServer.hasArg("plain")) {
        String jsonData = configServer.arg("plain");
        
        if (footswitchConfigFromJson(jsonData)) {
            saveFootswitchConfigToNVS();
            configServer.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration imported successfully\"}");
        } else {
            configServer.send(400, "application/json", "{\"error\":\"Invalid configuration file\"}");
        }
    } else {
        configServer.send(400, "application/json", "{\"error\":\"No file data received\"}");
    }
}

void handleReboot() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    configServer.send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting device...\"}");
    
    log(LOG_INFO, "Reboot requested via configuration interface");
    delay(1000);
    ESP.restart();
}
