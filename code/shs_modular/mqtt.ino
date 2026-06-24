// ============================================================================
//  mqtt.ino — publish sensor readings as individual MQTT topics
//  Each metric is published to <MQTT_TOPIC_PREFIX>/<DEVICE_ID>/<key>
//  e.g. diy-sensors/shs-livingroom/temperature
//  An LWT marks the device offline on <MQTT_TOPIC_PREFIX>/<DEVICE_ID>/status.
// ============================================================================
#include "config.h"

#if USE_MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

#define TOPIC_BASE    MQTT_TOPIC_PREFIX "/" DEVICE_ID
#define TOPIC_STATUS  TOPIC_BASE "/status"

#if MQTT_TLS
static WiFiClientSecure wifiClient;
#else
static WiFiClient       wifiClient;
#endif
static PubSubClient mqtt(wifiClient);
static uint32_t     lastPublish = 0;

static void publishStr(const char *key, const char *value, bool retained = false) {
  char topic[128];
  snprintf(topic, sizeof(topic), TOPIC_BASE "/%s", key);
  mqtt.publish(topic, value, retained);
}

static void publishFloat(const char *key, float val, int decimals) {
  if (isnan(val)) return;
  char buf[32];
  snprintf(buf, sizeof(buf), "%.*f", decimals, val);
  publishStr(key, buf);
}

static void publishUint(const char *key, unsigned int val) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u", val);
  publishStr(key, buf);
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

static void mqttReconnect() {
  if (mqtt.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("MQTT connecting... ");
  bool ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                         TOPIC_STATUS, 0, /* retained */ true, "offline");
  if (ok) {
    Serial.println("connected");
    mqtt.publish(TOPIC_STATUS, "online", /* retained */ true);
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
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  mqttReconnect();
}

void mqttLoop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();
}

// Called from the BME680 callback on every new sample; throttled to
// MQTT_PUBLISH_MS so the broker isn't flooded every ~3 s.
void mqttPublish(const SensorPacket &p) {
  if (!mqtt.connected()) return;
  uint32_t now = millis();
  if (lastPublish != 0 && now - lastPublish < MQTT_PUBLISH_MS) return;
  lastPublish = now;

  char ts[32];
  getISOTimestamp(ts, sizeof(ts));
  publishStr("timestamp",    ts);
  publishStr("status",       "ok");
  publishUint("iaq_accuracy", p.iaqAccuracy);
  publishFloat("iaq",         p.iaq,         0);
  publishFloat("co2",         p.co2,         0);
  publishFloat("voc",         p.voc,         2);
  publishFloat("temperature", p.temperature, 2);
  publishFloat("humidity",    p.humidity,    1);
  publishFloat("pressure",    p.pressure,    1);
}

#else  // ---- USE_MQTT == 0 : stub out the MQTT API -------------------------

void mqttConnect() {}
void mqttLoop() {}
void mqttPublish(const SensorPacket &) {}

#endif
