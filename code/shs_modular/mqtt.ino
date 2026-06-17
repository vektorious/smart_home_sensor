// ============================================================================
//  mqtt.ino — publish readings to Home Assistant over MQTT
//  On connect it publishes one retained MQTT-discovery config per metric under
//  "<HA_DISCOVERY_PREFIX>/sensor/<DEVICE_ID>/<key>/config", so Home Assistant
//  creates the entities automatically and groups them under one device.
//  Readings go to a single JSON state topic; an LWT marks the device offline
//  if it drops off the network.
// ============================================================================
#include "config.h"

#if USE_MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#define TOPIC_STATE   "smart_home_sensor/" DEVICE_ID "/state"
#define TOPIC_AVAIL   "smart_home_sensor/" DEVICE_ID "/status"

#if MQTT_TLS
static WiFiClientSecure wifiClient;
#else
static WiFiClient       wifiClient;
#endif
static PubSubClient mqtt(wifiClient);
static uint32_t     lastPublish = 0;
static bool         discoverySent = false;

// One Home Assistant sensor per metric. devclass may be nullptr (no class).
struct Metric {
  const char *key;       // JSON field in the state payload + topic segment
  const char *name;      // entity name shown in HA
  const char *unit;      // unit_of_measurement ("" = unitless)
  const char *devclass;  // HA device_class, or nullptr
};

static const Metric METRICS[] = {
  { "iaq",          "IAQ",                   "",    "aqi" },
  { "iaq_accuracy", "IAQ Accuracy",          "",    nullptr },
  { "co2",          "CO2 equivalent",        "ppm", "carbon_dioxide" },
  { "voc",          "Breath VOC equivalent", "ppm", "volatile_organic_compounds_parts" },
  { "temperature",  "Temperature",           "°C",  "temperature" },
  { "humidity",     "Humidity",              "%",   "humidity" },
  { "pressure",     "Pressure",              "hPa", "atmospheric_pressure" },
};
static const size_t N_METRICS = sizeof(METRICS) / sizeof(METRICS[0]);

// Shared HA "device" object so all entities group under one device card.
static const char *DEVICE_JSON =
  "\"dev\":{\"ids\":[\"" DEVICE_ID "\"],\"name\":\"" DEVICE_NAME "\","
  "\"mf\":\"Waveshare/Bosch\",\"mdl\":\"ESP32-C6 + BME680\"}";

static void publishDiscovery() {
  char topic[160];
  char payload[640];
  for (size_t i = 0; i < N_METRICS; i++) {
    const Metric &m = METRICS[i];
    snprintf(topic, sizeof(topic),
             HA_DISCOVERY_PREFIX "/sensor/" DEVICE_ID "/%s/config", m.key);

    int n = snprintf(payload, sizeof(payload),
      "{\"name\":\"%s\",\"uniq_id\":\"" DEVICE_ID "_%s\","
      "\"stat_t\":\"" TOPIC_STATE "\",\"avty_t\":\"" TOPIC_AVAIL "\","
      "\"val_tpl\":\"{{ value_json.%s }}\",\"stat_cla\":\"measurement\"",
      m.name, m.key, m.key);
    if (m.unit[0])  n += snprintf(payload + n, sizeof(payload) - n,
                                  ",\"unit_of_meas\":\"%s\"", m.unit);
    if (m.devclass) n += snprintf(payload + n, sizeof(payload) - n,
                                  ",\"dev_cla\":\"%s\"", m.devclass);
    n += snprintf(payload + n, sizeof(payload) - n, ",%s}", DEVICE_JSON);

    mqtt.publish(topic, payload, /* retained */ true);
  }
  Serial.println("MQTT discovery configs published");
}

// Append "key":value to a JSON buffer, skipping NaN. *first tracks the comma.
static int appendNum(char *buf, int n, int cap, const char *key,
                     float val, int decimals, bool *first) {
  if (isnan(val)) return n;
  n += snprintf(buf + n, cap - n, "%s\"%s\":%.*f",
                *first ? "" : ",", key, decimals, val);
  *first = false;
  return n;
}

static void mqttReconnect() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("MQTT connecting... ");
  // LWT: broker publishes "offline" to the availability topic if we vanish.
  bool ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                         TOPIC_AVAIL, 0, /* retained */ true, "offline");
  if (ok) {
    Serial.println("connected");
    mqtt.publish(TOPIC_AVAIL, "online", /* retained */ true);
    publishDiscovery();
    discoverySent = true;
    displaySetMqttStatus(true);
  } else {
    Serial.printf("failed (rc=%d)\n", mqtt.state());
    displaySetMqttStatus(false);
  }
}

// ---- Public API -----------------------------------------------------------

void mqttConnect() {
#if MQTT_TLS
  wifiClient.setInsecure();  // skip cert verification — broker is trusted
#endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(768);   // discovery payloads exceed the 256-byte default
  mqttReconnect();
}

void mqttLoop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();
}

// Called from the BME680 callback on every new sample; throttled to
// MQTT_PUBLISH_MS so Home Assistant isn't flooded every ~3 s.
void mqttPublish(const SensorPacket &p) {
  if (!mqtt.connected()) return;
  uint32_t now = millis();
  if (lastPublish != 0 && now - lastPublish < MQTT_PUBLISH_MS) return;
  lastPublish = now;

  char buf[256];
  // iaq_accuracy (0..3) is always valid, so emit it first; every later field
  // is then preceded by a comma (first = false).
  bool first = false;
  int n = snprintf(buf, sizeof(buf), "{\"iaq_accuracy\":%u", p.iaqAccuracy);
  n = appendNum(buf, n, sizeof(buf), "iaq",          p.iaq,         0, &first);
  n = appendNum(buf, n, sizeof(buf), "co2",          p.co2,         0, &first);
  n = appendNum(buf, n, sizeof(buf), "voc",          p.voc,         2, &first);
  n = appendNum(buf, n, sizeof(buf), "temperature",  p.temperature, 1, &first);
  n = appendNum(buf, n, sizeof(buf), "humidity",     p.humidity,    1, &first);
  n = appendNum(buf, n, sizeof(buf), "pressure",     p.pressure,    1, &first);
  snprintf(buf + n, sizeof(buf) - n, "}");

  mqtt.publish(TOPIC_STATE, buf);
}

#else  // ---- USE_MQTT == 0 : stub out the MQTT API -------------------------

void mqttConnect() {}
void mqttLoop() {}
void mqttPublish(const SensorPacket &) {}

#endif
