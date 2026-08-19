// ============================================================================
//  mqtt.ino — publish readings to Home Assistant over MQTT
//
//  Three modes, selectable at runtime (settings.mqttMode, edited in the portal):
//
//   DISCOVERY  One retained config message per metric under
//              "<haDiscPrefix>/sensor/<deviceId>/<key>/config", so Home
//              Assistant creates the entities itself and groups them under one
//              device card. Readings go to a single JSON state topic.
//   FLAT       One plain topic per metric, "<mqttPrefix>/<deviceId>/<key>".
//              For non-HA consumers, or a hand-written HA sensor config.
//   BOTH       Flat topics, plus discovery configs whose state topics point at
//              those flat topics instead of the JSON state topic.
//
//  In every mode an LWT marks the device offline on
//  "<mqttPrefix>/<deviceId>/status" if it drops off the network.
// ============================================================================
#include "config.h"

#if USE_MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

static WiFiClient       plainClient;
static WiFiClientSecure tlsClient;
static PubSubClient     mqtt;
static uint32_t         lastPublish = 0;

// Topic roots, built once from settings at connect time (they used to be
// compile-time string concatenations; they are runtime values now).
static char topicBase[96];    // <prefix>/<deviceId>
static char topicState[112];  // <base>/state
static char topicAvail[112];  // <base>/status

static bool modeUsesDiscovery() {
  return settings.mqttMode == MQTT_MODE_DISCOVERY || settings.mqttMode == MQTT_MODE_BOTH;
}
static bool modeUsesFlat() {
  return settings.mqttMode == MQTT_MODE_FLAT || settings.mqttMode == MQTT_MODE_BOTH;
}

// One Home Assistant sensor per metric. devclass may be nullptr (no class).
struct Metric {
  const char *key;       // JSON field in the state payload + topic segment
  const char *name;      // entity name shown in HA
  const char *unit;      // unit_of_measurement ("" = unitless)
  const char *devclass;  // HA device_class, or nullptr
  int         decimals;
};

static const Metric METRICS[] = {
  { "iaq",          "IAQ",                   "",    "aqi",                              0 },
  { "iaq_accuracy", "IAQ Accuracy",          "",    nullptr,                            0 },
  { "co2",          "CO2 equivalent",        "ppm", "carbon_dioxide",                   0 },
  { "voc",          "Breath VOC equivalent", "ppm", "volatile_organic_compounds_parts", 2 },
  { "temperature",  "Temperature",           "°C",  "temperature",                      1 },
  { "humidity",     "Humidity",              "%",   "humidity",                         1 },
  { "pressure",     "Pressure",              "hPa", "atmospheric_pressure",             1 },
};
static const size_t N_METRICS = sizeof(METRICS) / sizeof(METRICS[0]);

static float metricValue(const SensorPacket &p, const char *key) {
  if (!strcmp(key, "iaq"))          return p.iaq;
  if (!strcmp(key, "iaq_accuracy")) return (float)p.iaqAccuracy;
  if (!strcmp(key, "co2"))          return p.co2;
  if (!strcmp(key, "voc"))          return p.voc;
  if (!strcmp(key, "temperature"))  return p.temperature;
  if (!strcmp(key, "humidity"))     return p.humidity;
  if (!strcmp(key, "pressure"))     return p.pressure;
  return NAN;
}

// Shared HA "device" object so all entities group under one device card.
static void deviceJson(char *buf, size_t len) {
  snprintf(buf, len,
           "\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\","
           "\"mf\":\"Waveshare/Bosch\",\"mdl\":\"ESP32-C6 + BME680\"}",
           settings.deviceId, settings.deviceName);
}

static void publishDiscovery() {
  char topic[192];
  char payload[640];
  char dev[192];
  deviceJson(dev, sizeof(dev));

  for (size_t i = 0; i < N_METRICS; i++) {
    const Metric &m = METRICS[i];
    snprintf(topic, sizeof(topic), "%s/sensor/%s/%s/config",
             settings.haDiscPrefix, settings.deviceId, m.key);

    int n = snprintf(payload, sizeof(payload),
      "{\"name\":\"%s\",\"uniq_id\":\"%s_%s\",\"avty_t\":\"%s\","
      "\"stat_cla\":\"measurement\"",
      m.name, settings.deviceId, m.key, topicAvail);

    // In BOTH mode the entity reads the flat per-metric topic directly; in
    // DISCOVERY mode it reads the shared JSON state topic through a template.
    if (settings.mqttMode == MQTT_MODE_BOTH) {
      n += snprintf(payload + n, sizeof(payload) - n,
                    ",\"stat_t\":\"%s/%s\"", topicBase, m.key);
    } else {
      n += snprintf(payload + n, sizeof(payload) - n,
                    ",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.%s }}\"",
                    topicState, m.key);
    }
    if (m.unit[0])  n += snprintf(payload + n, sizeof(payload) - n,
                                  ",\"unit_of_meas\":\"%s\"", m.unit);
    if (m.devclass) n += snprintf(payload + n, sizeof(payload) - n,
                                  ",\"dev_cla\":\"%s\"", m.devclass);
    snprintf(payload + n, sizeof(payload) - n, ",%s}", dev);

    mqtt.publish(topic, payload, /* retained */ true);
  }
  Serial.println("MQTT discovery configs published");
}

// Retained discovery configs outlive the device that published them: without
// this, switching to FLAT mode would leave orphaned entities in Home Assistant
// that never update again.
static void clearDiscovery() {
  char topic[192];
  for (size_t i = 0; i < N_METRICS; i++) {
    snprintf(topic, sizeof(topic), "%s/sensor/%s/%s/config",
             settings.haDiscPrefix, settings.deviceId, METRICS[i].key);
    mqtt.publish(topic, "", /* retained */ true);   // empty payload removes the entity
  }
}

static void getISOTimestamp(char *buf, size_t len) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  if (now < 1000000000L) {
    // NTP not synced yet — fall back to device uptime
    snprintf(buf, len, "uptime-%lus", millis() / 1000UL);
    return;
  }
  gmtime_r(&now, &timeinfo);
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

static void publishFlat(const SensorPacket &p) {
  char topic[128];
  char buf[32];

  for (size_t i = 0; i < N_METRICS; i++) {
    const Metric &m = METRICS[i];
    float v = metricValue(p, m.key);
    if (!isValidFloat(v)) continue;
    snprintf(topic, sizeof(topic), "%s/%s", topicBase, m.key);
    snprintf(buf, sizeof(buf), "%.*f", m.decimals, v);
    mqtt.publish(topic, buf);
  }

  char ts[32];
  getISOTimestamp(ts, sizeof(ts));
  snprintf(topic, sizeof(topic), "%s/timestamp", topicBase);
  mqtt.publish(topic, ts);
}

static void publishStateJson(const SensorPacket &p) {
  char buf[320];
  // iaq_accuracy (0..3) is always valid, so emit it first; every later field is
  // then preceded by a comma.
  int n = snprintf(buf, sizeof(buf), "{\"iaq_accuracy\":%u", p.iaqAccuracy);
  for (size_t i = 0; i < N_METRICS; i++) {
    const Metric &m = METRICS[i];
    if (!strcmp(m.key, "iaq_accuracy")) continue;
    float v = metricValue(p, m.key);
    if (!isValidFloat(v)) continue;      // omit rather than send null
    n += snprintf(buf + n, sizeof(buf) - n, ",\"%s\":%.*f", m.key, m.decimals, v);
  }
  char ts[32];
  getISOTimestamp(ts, sizeof(ts));
  snprintf(buf + n, sizeof(buf) - n, ",\"timestamp\":\"%s\"}", ts);

  mqtt.publish(topicState, buf);
}

static void buildTopics() {
  snprintf(topicBase,  sizeof(topicBase),  "%s/%s", settings.mqttPrefix, settings.deviceId);
  snprintf(topicState, sizeof(topicState), "%s/state",  topicBase);
  snprintf(topicAvail, sizeof(topicAvail), "%s/status", topicBase);
}

static bool mqttReconnect() {
  if (mqtt.connected()) return true;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (settings.mqttHost[0] == '\0') return false;   // not configured yet

  Serial.print("MQTT connecting... ");
  // LWT: the broker publishes "offline" to the availability topic if we vanish.
  const char *user = settings.mqttUser[0] ? settings.mqttUser : nullptr;
  const char *pass = settings.mqttPass[0] ? settings.mqttPass : nullptr;
  bool ok = mqtt.connect(settings.deviceId, user, pass,
                         topicAvail, 0, /* retained */ true, "offline");
  if (ok) {
    Serial.println("connected");
    mqtt.publish(topicAvail, "online", /* retained */ true);
    if (modeUsesDiscovery()) publishDiscovery();
    else                     clearDiscovery();
    displaySetMqttStatus(true);
  } else {
    Serial.printf("failed (rc=%d)\n", mqtt.state());
    displaySetMqttStatus(false);
  }
  return ok;
}

// ---- Public API -----------------------------------------------------------

void mqttConnect() {
  if (settings.mqttTls) {
    tlsClient.setInsecure();   // skip cert verification — the broker is trusted
    mqtt.setClient(tlsClient);
  } else {
    mqtt.setClient(plainClient);
  }
  mqtt.setServer(settings.mqttHost, settings.mqttPort);
  mqtt.setBufferSize(768);     // discovery payloads exceed the 256-byte default
  buildTopics();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  mqttReconnect();
}

void mqttLoop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();
}

// Called from the BME680 callback on every new sample; throttled to the
// configured interval so Home Assistant isn't flooded every ~3 s.
void mqttPublish(const SensorPacket &p) {
  if (!mqtt.connected()) return;
  uint32_t now = millis();
  if (lastPublish != 0 && now - lastPublish < settings.publishIntervalSec * 1000UL) return;
  lastPublish = now;

  if (modeUsesFlat()) publishFlat(p);
  if (settings.mqttMode != MQTT_MODE_FLAT) publishStateJson(p);
}

// Publish once, ignoring the throttle — used by the portal's "send test"
// button, where waiting out the interval would defeat the point. Returns 0 on
// success, -1 if there is no broker connection to publish over.
int mqttSendTest(const SensorPacket &p) {
  buildTopics();
  if (!mqttReconnect()) return -1;
  lastPublish = millis();
  if (modeUsesFlat()) publishFlat(p);
  if (settings.mqttMode != MQTT_MODE_FLAT) publishStateJson(p);
  return 0;
}

#else  // ---- USE_MQTT == 0 : stub out the MQTT API -------------------------

void mqttConnect() {}
void mqttLoop() {}
void mqttPublish(const SensorPacket &) {}
int  mqttSendTest(const SensorPacket &) { return -1; }

#endif
