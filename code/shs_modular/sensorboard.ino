// ============================================================================
//  sensorboard.ino — publish readings to a diy-sensor.org instance over HTTPS.
//
//  The ingestion contract is one POST per reading:
//
//    POST /sensor/measurement
//    X-API-Key: <apiKey>            (optional; makes the device persistent)
//    {"device_id": "...", "write_key": "...", "project": "...", "name": "...",
//     "sensors": {"temperature": {"value": 21.4, "unit": "C"}, ...}}
//
//  The server assigns the timestamp on receipt — clients cannot supply one, and
//  sending a "timestamp" field is an error rather than an ignored extra.
//
//  Unlike the battery-powered sister project, this device is always on and
//  keeps its reading in RAM, so a failed POST is not a measurement lost
//  forever — the next one is only a minute away. The retry logic is still
//  worth having for a flaky AP, but it is deliberately short: blocking the
//  loop for a minute would stall BSEC's 3 s sampling cadence.
// ============================================================================
#include "config.h"

#if USE_SENSORBOARD
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static const uint8_t  SEND_ATTEMPTS   = 2;
static const uint32_t SEND_BACKOFF_MS = 1500;
static const uint32_t HTTP_CONNECT_MS = 8000;   // HTTPClient's 5 s default is
static const uint32_t HTTP_TIMEOUT_MS = 10000;  // short for a TLS handshake

static uint32_t lastSend = 0;

// Append "key":{"value":v,"unit":"u"} to the sensors object, skipping NaN.
// *first tracks whether a separating comma is needed.
static void appendSensor(String &json, const char *key, float value,
                         int decimals, const char *unit, bool *first) {
  if (!isValidFloat(value)) return;
  if (!*first) json += ",";
  json += "\"";
  json += key;
  json += "\":{\"value\":";
  json += String(value, (unsigned int)decimals);
  if (unit && unit[0]) {
    json += ",\"unit\":\"";
    json += unit;
    json += "\"";
  }
  json += "}";
  *first = false;
}

static String buildPayload(const SensorPacket &p) {
  String json = "{";
  json += "\"device_id\":\"" + String(settings.deviceId) + "\",";
  json += "\"name\":\"" + String(settings.deviceName) + "\",";
  if (settings.writeKey[0]) {
    json += "\"write_key\":\"" + String(settings.writeKey) + "\",";
  }
  // Omit the key entirely when unset — an ungrouped device is valid, an empty
  // project string is not.
  if (settings.project[0]) {
    json += "\"project\":\"" + String(settings.project) + "\",";
  }
  json += "\"sensors\":{";

  bool first = true;
  // IAQ is only meaningful once BSEC says so, but the accuracy level is sent
  // regardless — it is what explains a missing or implausible IAQ on the
  // dashboard during the first hours of calibration.
  appendSensor(json, "iaq",             p.iaq,                0, "",    &first);
  appendSensor(json, "iaq_accuracy",    (float)p.iaqAccuracy, 0, "",    &first);
  appendSensor(json, "co2_equivalent",  p.co2,                0, "ppm", &first);
  appendSensor(json, "voc_equivalent",  p.voc,                2, "ppm", &first);
  appendSensor(json, "temperature",     p.temperature,        2, "C",   &first);
  appendSensor(json, "humidity",        p.humidity,           1, "%",   &first);
  appendSensor(json, "pressure",        p.pressure,           1, "hPa", &first);
  appendSensor(json, "wifi_rssi",       (float)WiFi.RSSI(),   0, "dBm", &first);

  json += "}}";
  return json;
}

// One delivery attempt. Returns the HTTP status, a negative HTTPClient error
// code, or -1 if there is no link to send over.
static int postPayload(const String &json) {
  if (WiFi.status() != WL_CONNECTED) return -1;

  WiFiClientSecure tls;
  WiFiClient       plain;
  HTTPClient http;

  bool https = strncmp(settings.apiUrl, "https:", 6) == 0;
  if (https) {
    // No cert bundle is pinned: the workshop's threat model is a shared class
    // WiFi, and the payload is public sensor data. TLS still protects the
    // API and write keys in transit from casual observation.
    tls.setInsecure();
    if (!http.begin(tls, settings.apiUrl)) return -1;
  } else {
    if (!http.begin(plain, settings.apiUrl)) return -1;
  }

  http.setConnectTimeout(HTTP_CONNECT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  if (settings.apiKey[0]) http.addHeader("X-API-Key", settings.apiKey);

  int code = http.POST(json);
  http.end();
  return code;
}

// 201 = the device claimed its ID (first write), 200 = stored on an existing
// device. Anything else means the measurement did not land.
static bool isAccepted(int code) {
  return code == 200 || code == 201;
}

// Negative codes are transport failures (DNS, TLS, timeout, dropped link) and
// are worth another go. So are 429 and 5xx — the server is asking us to come
// back. Any other 4xx is our own malformed or unauthorised request: retrying
// produces the same rejection.
static bool shouldRetry(int code) {
  if (code < 0)    return true;
  if (code == 429) return true;
  return code >= 500;
}

// Explain the failures a student is actually likely to hit, rather than
// leaving them with a bare number.
static void explainCode(int code) {
  switch (code) {
    case 403:
      Serial.println("  403 invalid_write_key — this device ID is owned by a "
                     "different write key. If the board was flashed with a "
                     "different workshop image, ask the instructor to free the ID.");
      break;
    case 401:
      Serial.println("  401 — the API key was missing or rejected.");
      break;
    case 429:
      Serial.println("  429 — rate limited. The publish interval may be too short.");
      break;
    case 400:
      Serial.println("  400 — the server rejected the payload.");
      break;
    default: break;
  }
}

// ---- Public API -----------------------------------------------------------

void sensorboardConnect() {
  Serial.printf("diy-sensor: publishing to %s as '%s' every %u min\n",
                settings.apiUrl, settings.deviceId, settings.publishIntervalMin);
  if (settings.project[0]) Serial.printf("diy-sensor: project '%s'\n", settings.project);
  Serial.printf("diy-sensor: dashboard at %s%s\n", DASHBOARD_URL_PREFIX, settings.deviceId);
}

// Returns the HTTP code of the last attempt, or -1 if there was no link.
int sensorboardSend(const SensorPacket &p) {
  String json = buildPayload(p);
  int code = -1;

  for (uint8_t attempt = 1; attempt <= SEND_ATTEMPTS; attempt++) {
    code = postPayload(json);
    Serial.printf("POST attempt %u/%u -> %d\n", attempt, SEND_ATTEMPTS, code);

    if (isAccepted(code)) {
      displaySetMqttStatus(true);
      return code;
    }
    explainCode(code);
    if (!shouldRetry(code) || attempt == SEND_ATTEMPTS) break;
    delay(SEND_BACKOFF_MS);
  }

  displaySetMqttStatus(false);
  return code;
}

// Called from the BME680 callback on every new sample (~3 s); throttled to the
// configured interval. The per-device limit is 12 writes/min, which the 5 min
// default clears easily; the constraint that actually binds in a workshop is
// the per-credential write budget, since every device shares one API key.
void sensorboardPublish(const SensorPacket &p) {
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  if (lastSend != 0 && now - lastSend < publishIntervalMs()) return;
  lastSend = now;
  sensorboardSend(p);
}

#else  // ---- USE_SENSORBOARD == 0 : stub out the API -----------------------

void sensorboardConnect() {}
void sensorboardPublish(const SensorPacket &) {}
int  sensorboardSend(const SensorPacket &) { return -1; }

#endif
