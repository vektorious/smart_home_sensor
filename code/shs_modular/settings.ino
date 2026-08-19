// ============================================================================
//  settings.ino — runtime settings persisted in NVS/Preferences.
//
//  On first boot (or after the Settings layout changes) the DEFAULT_* values
//  from config.h are written into flash. Thereafter the setup portal edits
//  these and calls saveSettings(). config.h no longer needs editing per device,
//  which is what lets a single web-flashed binary be reconfigured on the bench.
//
//  DEVICE IDENTITY IS NOT A SETTING. The device ID and the write key are
//  derived from the factory-programmed MAC, not stored, so no reset can lose
//  them — see deriveIdentity() below for why that matters.
// ============================================================================
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <mbedtls/md.h>

// Global settings instance (declared extern in config.h).
Settings settings;

static Preferences settingsPrefs;

static const char *NVS_NAMESPACE = "shs";
// Bump when the Settings layout changes so stale flash is re-initialised.
static const uint32_t SETTINGS_VERSION = 1;

// ---------------------------------------------------------------------------
//  Device identity — derived, never stored
//
//  The write key is what proves ownership of a device ID to diy-sensor.org, and
//  it is unrecoverable by design: there is no reset endpoint. A device that
//  holds an API key is also persistent, so it is never swept — meaning a lost
//  write key orphans that device ID *permanently*, and only the operator can
//  free it again with `admin delete-device`.
//
//  A random key in NVS would be lost on any full flash erase — which is exactly
//  what the web flasher does on a first install. So instead we recompute the
//  key from the immutable efuse MAC at every boot:
//
//      writeKey = HMAC-SHA256(WORKSHOP_KEY_SALT, mac)[:32 hex chars]
//
//  Same board + same salt = same key, across factory resets, flash erases,
//  reflashes and variant switches. Nothing to lose, nothing to write down.
//
//  The salt ships inside a publicly downloadable workshop image, so anyone with
//  that image can derive any workshop device's key. That is an accepted
//  trade-off, not an oversight: the shared workshop API key is in the same
//  binary, so the write key was never the secret holding the door shut. It
//  prevents students colliding on an ID, and it is scoped to one workshop by
//  rotating the salt and withdrawing the image afterwards.
//
//  Without a salt (any build that has no workshop_secrets.h) there is nothing
//  to derive from, so we fall back to a random key generated once and kept in
//  NVS. That key IS secret; the portal displays it so it can be written down,
//  and it can be re-entered by hand to re-adopt an existing device ID.
// ---------------------------------------------------------------------------

static void macToBytes(uint8_t out[6]) {
  uint64_t mac = ESP.getEfuseMac();   // 48-bit, factory-programmed, immutable
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)(mac >> (8 * i));
}

// 32 hex chars (128 bits) of HMAC-SHA256 over the MAC. Truncation is fine here:
// this is an ownership token, not a signature.
static bool deriveWriteKey(const char *salt, char *out, size_t outLen) {
  if (outLen < 33 || salt == nullptr || salt[0] == '\0') return false;

  uint8_t mac[6];
  macToBytes(mac);

  uint8_t hmac[32];
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  bool ok = mbedtls_md_setup(&ctx, md, /* hmac */ 1) == 0 &&
            mbedtls_md_hmac_starts(&ctx, (const uint8_t *)salt, strlen(salt)) == 0 &&
            mbedtls_md_hmac_update(&ctx, mac, sizeof(mac)) == 0 &&
            mbedtls_md_hmac_finish(&ctx, hmac) == 0;
  mbedtls_md_free(&ctx);
  if (!ok) return false;

  for (int i = 0; i < 16; i++) snprintf(out + i * 2, 3, "%02x", hmac[i]);
  return true;
}

static void randomWriteKey(char *out, size_t outLen) {
  if (outLen < 33) return;
  for (int i = 0; i < 16; i++) {
    snprintf(out + i * 2, 3, "%02x", (uint8_t)(esp_random() & 0xFF));
  }
}

// Fill deviceId (and deviceName's suffix) from the MAC. Called on every boot,
// not just on reset, so identity is correct even if NVS holds something stale.
static void deriveIdentity(char *idOut, size_t idLen, char *uidOut, size_t uidLen) {
  uint32_t uid = (uint32_t)ESP.getEfuseMac();   // lower 32 bits
  snprintf(uidOut, uidLen, "%08x", uid);
  snprintf(idOut, idLen, "%s-%s", DEFAULT_DEVICE_ID_PREFIX, uidOut);
}

// ---------------------------------------------------------------------------

void resetSettingsToDefaults() {
  char uid[9];
  deriveIdentity(settings.deviceId, sizeof(settings.deviceId), uid, sizeof(uid));
  snprintf(settings.deviceName, sizeof(settings.deviceName), "%s-%s",
           DEFAULT_DEVICE_NAME, uid);

  strlcpy(settings.project,      DEFAULT_PROJECT,        sizeof(settings.project));
  strlcpy(settings.apiUrl,       DEFAULT_API_URL,        sizeof(settings.apiUrl));
  strlcpy(settings.apiKey,       DEFAULT_API_KEY,        sizeof(settings.apiKey));
  strlcpy(settings.mqttHost,     DEFAULT_MQTT_HOST,      sizeof(settings.mqttHost));
  strlcpy(settings.mqttUser,     DEFAULT_MQTT_USER,      sizeof(settings.mqttUser));
  strlcpy(settings.mqttPass,     DEFAULT_MQTT_PASS,      sizeof(settings.mqttPass));
  strlcpy(settings.mqttPrefix,   DEFAULT_MQTT_PREFIX,    sizeof(settings.mqttPrefix));
  strlcpy(settings.haDiscPrefix, DEFAULT_HA_DISC_PREFIX, sizeof(settings.haDiscPrefix));
  settings.mqttPort           = DEFAULT_MQTT_PORT;
  settings.mqttMode           = DEFAULT_MQTT_MODE;
  settings.mqttTls            = DEFAULT_MQTT_TLS;
  settings.publishIntervalSec = DEFAULT_PUBLISH_INTERVAL_SEC;
  settings.tempOffsetC        = DEFAULT_TEMP_OFFSET_C;

  // Derived keys survive this reset unchanged; a random key is only minted if
  // there is no salt to derive from and none has been stored yet.
  if (!deriveWriteKey(WORKSHOP_KEY_SALT, settings.writeKey, sizeof(settings.writeKey))) {
    if (settings.writeKey[0] == '\0') {
      randomWriteKey(settings.writeKey, sizeof(settings.writeKey));
    }
  }
}

void loadSettings() {
  settingsPrefs.begin(NVS_NAMESPACE, /* readOnly */ true);
  uint32_t version = settingsPrefs.getUInt("version", 0);

  if (version != SETTINGS_VERSION) {
    settingsPrefs.end();
    Serial.println("Settings: initialising NVS from defaults");
    settings.writeKey[0] = '\0';
    resetSettingsToDefaults();
    saveSettings();
    return;
  }

  settingsPrefs.getString("deviceName",  settings.deviceName,  sizeof(settings.deviceName));
  settingsPrefs.getString("project",     settings.project,     sizeof(settings.project));
  settingsPrefs.getString("writeKey",    settings.writeKey,    sizeof(settings.writeKey));
  settingsPrefs.getString("apiUrl",      settings.apiUrl,      sizeof(settings.apiUrl));
  settingsPrefs.getString("apiKey",      settings.apiKey,      sizeof(settings.apiKey));
  settingsPrefs.getString("mqttHost",    settings.mqttHost,    sizeof(settings.mqttHost));
  settingsPrefs.getString("mqttUser",    settings.mqttUser,    sizeof(settings.mqttUser));
  settingsPrefs.getString("mqttPass",    settings.mqttPass,    sizeof(settings.mqttPass));
  settingsPrefs.getString("mqttPrefix",  settings.mqttPrefix,  sizeof(settings.mqttPrefix));
  settingsPrefs.getString("haDisc",      settings.haDiscPrefix, sizeof(settings.haDiscPrefix));
  settings.mqttPort           = settingsPrefs.getUShort("mqttPort", DEFAULT_MQTT_PORT);
  settings.mqttMode           = settingsPrefs.getUChar("mqttMode",  DEFAULT_MQTT_MODE);
  settings.mqttTls            = settingsPrefs.getBool("mqttTls",    DEFAULT_MQTT_TLS);
  settings.publishIntervalSec = settingsPrefs.getUInt("pubSec",     DEFAULT_PUBLISH_INTERVAL_SEC);
  settings.tempOffsetC        = settingsPrefs.getFloat("tempOff",   DEFAULT_TEMP_OFFSET_C);
  settingsPrefs.end();

  // Identity is always recomputed, never trusted from flash: a board restored
  // from someone else's NVS dump would otherwise publish under their ID.
  char uid[9];
  deriveIdentity(settings.deviceId, sizeof(settings.deviceId), uid, sizeof(uid));

  // A salted build always re-derives; an unsalted one keeps whatever is stored,
  // and only mints a key if flash somehow came back empty.
  if (!deriveWriteKey(WORKSHOP_KEY_SALT, settings.writeKey, sizeof(settings.writeKey)) &&
      settings.writeKey[0] == '\0') {
    randomWriteKey(settings.writeKey, sizeof(settings.writeKey));
    saveSettings();
  }

  Serial.println("Settings: loaded from NVS");
}

void saveSettings() {
  settingsPrefs.begin(NVS_NAMESPACE, /* readOnly */ false);
  settingsPrefs.putString("deviceName", settings.deviceName);
  settingsPrefs.putString("project",    settings.project);
  settingsPrefs.putString("writeKey",   settings.writeKey);
  settingsPrefs.putString("apiUrl",     settings.apiUrl);
  settingsPrefs.putString("apiKey",     settings.apiKey);
  settingsPrefs.putString("mqttHost",   settings.mqttHost);
  settingsPrefs.putString("mqttUser",   settings.mqttUser);
  settingsPrefs.putString("mqttPass",   settings.mqttPass);
  settingsPrefs.putString("mqttPrefix", settings.mqttPrefix);
  settingsPrefs.putString("haDisc",     settings.haDiscPrefix);
  settingsPrefs.putUShort("mqttPort",   settings.mqttPort);
  settingsPrefs.putUChar("mqttMode",    settings.mqttMode);
  settingsPrefs.putBool("mqttTls",      settings.mqttTls);
  settingsPrefs.putUInt("pubSec",       settings.publishIntervalSec);
  settingsPrefs.putFloat("tempOff",     settings.tempOffsetC);
  settingsPrefs.putUInt("version",      SETTINGS_VERSION);
  settingsPrefs.end();

  Serial.println("Settings: saved to NVS");
}

// Forget the saved network only — settings, identity and the IAQ calibration
// all stay. This is the button for "I typed the wrong Wi-Fi password".
void clearWiFiCredentials() {
  WiFi.disconnect(/* wifioff */ false, /* eraseap */ true);
  Serial.println("Settings: Wi-Fi credentials cleared");
}

// Discard the BSEC calibration only. Kept separate from the factory reset
// because it is expensive to rebuild — hours to reach accuracy 3, up to four
// days to fully converge — and because clearing it is occasionally what you
// actually want: after moving the device to a different room, or when a bad
// calibration has locked itself in.
void clearBsecState() {
  Preferences p;
  p.begin("bsec", /* readOnly */ false);
  p.clear();
  p.end();
  Serial.println("Settings: BSEC calibration state cleared");
}
