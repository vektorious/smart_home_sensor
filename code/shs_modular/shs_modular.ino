// ============================================================================
//  Smart Home Sensor — modular firmware (entry point)
//
//  Waveshare ESP32-C6-LCD-1.3 + BME680 (Bosch BSEC2), reporting to Home
//  Assistant over MQTT. This is an always-on, USB-powered device: the live
//  display and BSEC's multi-day IAQ self-calibration mean it never deep-sleeps.
//
//  Board:    "ESP32C6 Dev Module"  (Arduino-ESP32 core)
//  Libraries: GFX Library for Arduino, bsec2, BME68x Sensor library,
//             WiFiManager (tzapu), PubSubClient
//
//  Everything you configure lives in config.h. Each feature is a separate
//  module (display.ino / bme680.ino / wifi.ino / mqtt.ino) toggled by the
//  USE_* flags there.
//
//  NOTE: bsec2 ships no esp32c6 precompiled blob — copy src/esp32c3/libalgobsec.a
//  to src/esp32c6/ before building. See instructions/build_instructions.md §4.
// ============================================================================
#include "config.h"
#include <bsec2.h>   // types must be visible before Arduino injects auto-generated prototypes

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Smart Home Sensor starting (ESP32-C6 + BME680 / BSEC2)");

  displayInit();
  displayStatus("Detecting...", COLOR_INFO);

  if (bme680Init()) {
    Serial.println("BME680 + BSEC initialized");
    displayStatus("Stabilizing", COLOR_INFO);
  } else {
    Serial.println("BME680 / BSEC init failed");
    // Leave the specific error from checkBsecStatus() on the footer line.
    displayNoSensor();
  }

  // Networking last: bring up Wi-Fi + MQTT only when USE_MQTT is enabled.
  wifiConnect();
  mqttConnect();
}

void loop() {
  bme680Run();    // samples when due (~3 s) and fires the data callback
  mqttLoop();     // keeps the MQTT connection alive / reconnects
  displayTick();  // drives the WiFi blink animation
}
