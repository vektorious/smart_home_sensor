// ============================================================================
//  Smart Home Sensor — modular firmware (entry point)
//
//  Waveshare ESP32-C6-LCD-1.3 + BME680 (Bosch BSEC2). An always-on,
//  USB-powered device: the live display and BSEC's multi-day IAQ
//  self-calibration mean it never deep-sleeps.
//
//  One source tree, three builds — pick with SHS_VARIANT in config.h, or
//  override it from the command line (see web-flasher/build.sh):
//
//    SHS_VARIANT_MQTT_HA      Home Assistant over MQTT
//    SHS_VARIANT_SENSORBOARD  diy-sensor.org over HTTPS  (short workshop)
//    SHS_VARIANT_DISPLAY      display + serial only, no networking
//
//  Per-device configuration is NOT compiled in: it lives in NVS and is edited
//  from the setup portal, so one flashed image serves every device. See
//  settings.ino and portal.ino.
//
//  Board:     "ESP32C6 Dev Module"  (Arduino-ESP32 core)
//  Libraries: GFX Library for Arduino, bsec2, BME68x Sensor library,
//             WiFiManager (tzapu), PubSubClient
//
//  NOTE: bsec2 ships no esp32c6 precompiled blob — copy src/esp32c3/libalgobsec.a
//  to src/esp32c6/ before building. See instructions/build_instructions.md §4.
// ============================================================================
#include "config.h"
// These headers must be visible before Arduino injects its auto-generated
// prototypes: any function signature naming one of their types (bsecOutputs,
// WiFiManagerParameter) is otherwise prototyped against an unknown type.
#include <bsec2.h>
#if USE_NETWORK
#include <WiFiManager.h>
#endif

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nSmart Home Sensor starting (ESP32-C6 + BME680 / BSEC2)");

  // Settings first: the display title, the sensor's temperature offset and the
  // AP name all read from them.
  loadSettings();
  Serial.printf("Device: %s (%s)\n", settings.deviceName, settings.deviceId);

  displayInit();
  displayStatus("Detecting...", COLOR_INFO);

  // Must run before the sensor comes up: it blocks for the detection window,
  // and BSEC's sampling clock should not be started and then stalled.
  bool doubleReset = detectDoubleReset();

  if (bme680Init()) {
    Serial.println("BME680 + BSEC initialized");
    displayStatus("Stabilizing", COLOR_INFO);
  } else {
    Serial.println("BME680 / BSEC init failed");
    // Leave the specific error from checkBsecStatus() on the footer line.
    displayNoSensor();
  }

#if USE_NETWORK
  bool connected = wifiConnect();

  // Open the portal on a double reset, or whenever we cannot get online. Unlike
  // the battery-powered sister project there is no cost to being wrong here:
  // the device is on mains, and the portal keeps sampling BSEC while it waits.
  if (doubleReset || !connected) {
    runCommissioningPortal();
    displayResume();
    // Credentials may have just been entered — pick them up without a reboot.
    if (WiFi.status() != WL_CONNECTED) wifiConnect();
  }

  mqttConnect();
  sensorboardConnect();
#else
  (void)doubleReset;
  Serial.println("Display-only build — no networking");
#endif
}

void loop() {
  bme680Run();    // samples when due (~3 s) and fires the data callback,
                  // which publishes to whichever backend is compiled in
  wifiLoop();     // re-associates in the background if the link drops
  mqttLoop();     // keeps the broker connection alive / reconnects
  displayTick();  // drives the Wi-Fi blink animation
}
