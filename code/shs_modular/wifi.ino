// ============================================================================
//  wifi.ino — station connection using the credentials saved by the portal.
//
//  This is an always-on device (live display + multi-day BSEC calibration), so
//  there is no deep sleep: Wi-Fi stays associated and is re-established in the
//  background if it drops. Commissioning itself lives in portal.ino.
// ============================================================================
#include "config.h"

#if USE_NETWORK
#include <WiFi.h>
#include <esp_wifi.h>

// A single association attempt gets this long. 10 s is too tight: a busy
// 2.4 GHz AP plus a DHCP lease can easily exceed it.
static const uint32_t WIFI_ATTEMPT_MS = 15000;
static const uint8_t  WIFI_ATTEMPTS   = 2;

// Backoff between background reconnect attempts, so a router that is off
// overnight does not mean a reconnect storm.
static const uint32_t RECONNECT_MIN_MS = 5000;
static const uint32_t RECONNECT_MAX_MS = 60000;

static uint32_t nextReconnectAt = 0;
static uint32_t reconnectDelay  = RECONNECT_MIN_MS;

// True when Wi-Fi credentials have been saved before (by the portal or a
// previous session). Without them there is nothing to try and the caller
// should go straight to commissioning.
bool wifiHasCredentials() {
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return false;
  return conf.sta.ssid[0] != '\0';
}

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
  // Modem sleep ON, which is the ESP32 default. It was disabled here for
  // latency, which was the wrong trade for this device: the radio is a few
  // centimetres from the temperature sensor inside a closed enclosure, and
  // keeping its front-end powered between beacons dissipates a few hundred
  // milliwatts continuously. That heat lands on the one measurement the device
  // exists to report. The 5 °C self-heating offset was characterised on the
  // pre-networking sketch (code/legacy/test_wv_display/), which had no radio at
  // all; with sleep disabled the real figure was around 7.5 °C.
  //
  // Nothing here needs the latency it bought: readings go out every few minutes
  // and the reconnect path in wifiLoop() covers a link that drops.
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);

  if (!wifiHasCredentials()) {
    Serial.println("Wi-Fi: no saved credentials");
    return false;
  }

  for (uint8_t attempt = 1; attempt <= WIFI_ATTEMPTS; attempt++) {
    if (attempt > 1) {
      WiFi.disconnect(true);      // a stuck association does not recover alone
      delay(200);
    }
    WiFi.begin();                 // use the stored credentials

    Serial.printf("Wi-Fi: connecting (attempt %u/%u)", attempt, WIFI_ATTEMPTS);
    displayStatus("WiFi...", COLOR_INFO);
    uint32_t started = millis();
    while (millis() - started < WIFI_ATTEMPT_MS) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWi-Fi OK: %s  RSSI %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        displaySetWifiStatus(true);
        return true;
      }
      delay(250);
      Serial.print(".");
    }
    Serial.printf("\nWi-Fi: attempt %u timed out\n", attempt);
  }

  Serial.println("Wi-Fi: not connected with saved credentials");
  displaySetWifiStatus(false);
  return false;
}

// Called from loop(). The ESP32 auto-reconnects on its own, but only while the
// AP is reachable at the moment it tries; this covers the longer outages, with
// an exponential backoff so we are not retrying every pass of the loop.
void wifiLoop() {
  bool connected = WiFi.status() == WL_CONNECTED;
  displaySetWifiStatus(connected);

  if (connected) {
    reconnectDelay = RECONNECT_MIN_MS;
    nextReconnectAt = 0;
    return;
  }
  if (!wifiHasCredentials()) return;

  uint32_t now = millis();
  if (nextReconnectAt == 0) {
    nextReconnectAt = now + reconnectDelay;
    return;
  }
  if ((int32_t)(now - nextReconnectAt) < 0) return;

  Serial.println("Wi-Fi: link lost — reconnecting");
  WiFi.disconnect();
  WiFi.begin();
  reconnectDelay = min(reconnectDelay * 2, RECONNECT_MAX_MS);
  nextReconnectAt = now + reconnectDelay;
}

#else  // ---- no networking in this variant ---------------------------------

bool wifiHasCredentials() { return false; }
bool wifiConnect()        { return false; }
void wifiLoop()           {}

#endif
