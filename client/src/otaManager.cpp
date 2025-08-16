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
#include <WiFiManager.h>
#include <ElegantOTA.h>
#include <globals.h>
#include "utils.h"

WebServer server(80);

// === Start OTA and WiFiManager ===
void startOTA() {
  log(LOG_INFO, "=== Starting OTA Setup Mode ===");
  WiFiManager wm;

  if (!wm.autoConnect("OTA_Config_Portal")) {
    log(LOG_ERROR, "Failed to connect to WiFi during OTA setup");
    ESP.restart();
  }

  log(LOG_INFO, "WiFi connected during OTA setup");
  char ipStr[16];
  WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));
  logf(LOG_INFO, "IP Address: %s", ipStr);
  
  server.on("/", HTTP_GET, []() {
    char ipStr[16];
    WiFi.localIP().toString().toCharArray(ipStr, sizeof(ipStr));
    
    String html = "<html><head><style>body{font-family:sans-serif;text-align:center;padding:2em;}h1{color:#333;}p{margin:1em 0;}a,input[type=submit]{padding:0.5em 1em;background:#007bff;color:#fff;border:none;border-radius:5px;}a:hover,input[type=submit]:hover{background:#0056b3;}</style></head><body>";
    html += "<h1>ESP32 OTA Ready</h1>";
    html += "<p><b>Firmware Version:</b> " FIRMWARE_VERSION "</p>";
    html += "<p><b>IP:</b> ";
    html += ipStr;
    html += "</p>";
    html += "<p><a href='/update'>Go to OTA Update</a></p>";
    html += "<form action='/reboot' method='POST'><input type='submit' value='Reboot ESP32'></form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/reboot", HTTP_POST, []() {
    server.send(200, "text/plain", "Rebooting...");
    log(LOG_INFO, "Reboot requested via web interface");
    delay(1000);
    ESP.restart();
  });

  ElegantOTA.begin(&server);
  server.begin();
  log(LOG_INFO, "Web server started for OTA updates");

  unsigned long start = millis();
  const unsigned long TIMEOUT = 5 * 60 * 1000; // 5 min
  log(LOG_INFO, "OTA mode active for 5 minutes");
  
  while (millis() - start < TIMEOUT) {
    server.handleClient();
    delay(10);
  }

  log(LOG_WARN, "OTA timeout reached, rebooting...");
  ESP.restart();
}

void startOTA_AP() {
    // 1. Set AP IP address (optional, default is 192.168.4.1)
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);

    // 2. Start Access Point
    const char* ssid = "ESP32_OTA";
    const char* password = "12345678"; // Optional, at least 8 chars for WPA2
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP()); // Should print 192.168.4.1

    // 3. Start ElegantOTA
    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("ElegantOTA server started. Connect to the AP and go to http://192.168.4.1/update");
    ElegantOTA.setAutoReboot(true);
    // 4. Main OTA loop (blocks until timeout or reboot)
    unsigned long start = millis();
    const unsigned long TIMEOUT = 5 * 60 * 1000; // 5 minutes
    while (millis() - start < TIMEOUT) {
        server.handleClient();
        ElegantOTA.loop();   // <-- Add this line!
        updateStatusLED();   // If you want LED feedback
        delay(10);
    }

    Serial.println("OTA timeout reached, rebooting...");
    ESP.restart();
}