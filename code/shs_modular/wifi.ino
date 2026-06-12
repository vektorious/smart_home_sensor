// ============================================================================
//  wifi.ino — Wi-Fi connection via WiFiManager
//  On first boot (or when no saved network is found) the device starts a
//  temporary access point "<DEVICE_NAME>-Setup"; connect to it to enter your
//  Wi-Fi credentials. They are saved and reused on every later boot.
//
//  This is an always-on device (live display + multi-day BSEC calibration),
//  so there is no deep sleep — Wi-Fi stays connected.
// ============================================================================
#include "config.h"

#if USE_MQTT
#include <WiFiManager.h>   // tzapu

void wifiConnect() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);   // give up the portal after 3 min, then retry

  displayStatus("WiFi setup...", COLOR_INFO);
  Serial.println("Connecting to Wi-Fi (WiFiManager)...");

  // Opens the "<DEVICE_NAME>-Setup" AP if no saved network connects.
  if (!wm.autoConnect(DEVICE_NAME "-Setup")) {
    Serial.println("Wi-Fi failed / portal timed out — restarting");
    displayStatus("WiFi failed", COLOR_ERR);
    delay(2000);
    ESP.restart();
  }

  Serial.print("Wi-Fi connected: ");
  Serial.println(WiFi.localIP());
}

#else  // ---- USE_MQTT == 0 : no networking -------------------------------

void wifiConnect() {}

#endif
