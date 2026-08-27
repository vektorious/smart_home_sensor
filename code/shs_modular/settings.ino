// ============================================================================
//  settings.ino — runtime settings persisted in NVS/Preferences.
//
//  On first boot (or after the Settings layout changes) the DEFAULT_* values
//  from config.h are written into flash. Thereafter the setup portal edits
//  these and calls saveSettings(). config.h no longer needs editing per device,
//  which is what lets a single web-flashed binary be reconfigured on the bench.
//
//  DEVICE IDENTITY IS DERIVED, not stored: the device ID and the write key come
//  from the factory-programmed MAC, so no reset can lose them — see
//  deriveIdentity() below for why that matters. The one exception is an explicit
//  device-ID override entered in the portal, which is a setting like any other;
//  see setDeviceIdOverride().
// ============================================================================
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <mbedtls/md.h>

// Global settings instance (declared extern in config.h).
Settings settings;

static Preferences settingsPrefs;

static bool lastIdentityHashOk = false;

static const char *NVS_NAMESPACE = "shs";
// Bump when the Settings layout changes so stale flash is re-initialised.
// Deliberately NOT bumped for deviceIdOverride: a bump throws every configured
// device back to defaults on the next boot, and the new field needs no
// migration — an absent NVS key leaves the buffer untouched, and loadSettings()
// clears it first.
static const uint32_t SETTINGS_VERSION = 4;

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
//  The salt ships inside a publicly downloadable workshop image, so it is not a
//  secret. It is not quite a free pass either: the HMAC covers the full 48-bit
//  MAC, and the public device ID is a one-way hash of it, so the ID alone does
//  not hand over the derivation input. What defeats that entirely is proximity —
//  the Wi-Fi station MAC is this same efuse MAC, broadcast in every frame, so
//  anyone in radio range reads the derivation input directly.
//
//  That is an accepted trade-off, not an oversight. The shared workshop API key
//  sits in the same binary, so the write key was never the secret holding the
//  door shut; it stops students colliding on an ID, and it is scoped to one
//  workshop by rotating the salt and withdrawing the image afterwards.
//
//  Without a salt (any build that has no workshop_secrets.h) there is nothing
//  to derive from, so we fall back to a random key generated once and kept in
//  NVS. That key IS secret; the portal displays it so it can be written down,
//  and offers a field to paste one in — which is how a board re-adopts a device
//  ID it claimed under a different build. A salted build re-derives on every
//  boot and would overwrite anything entered, so the field is only offered
//  where it can actually take effect.
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
//
// The suffix is the first 32 bits of SHA-256 over the *whole* 48-bit MAC, not
// the low 32 bits of the MAC itself. Truncating the MAC keeps the Espressif OUI
// and one NIC byte, so a workshop's worth of boards — same chip, often the same
// production batch — share a long common prefix and differ only in the last two
// hex digits, which is neither recognisable on a badge nor evenly spread. The
// hash spreads all 48 bits across all 8 characters; every board still gets the
// same ID on every boot, since the MAC is immutable.
static void deriveIdentity(char *idOut, size_t idLen, char *uidOut, size_t uidLen) {
  uint8_t mac[6];
  macToBytes(mac);

  uint8_t digest[32];
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  bool ok = md != nullptr &&
            mbedtls_md_setup(&ctx, md, /* hmac */ 0) == 0 &&
            mbedtls_md_starts(&ctx) == 0 &&
            // Domain-separate from the write key, which HMACs the same bytes.
            mbedtls_md_update(&ctx, (const uint8_t *)"shs-device-id", 13) == 0 &&
            mbedtls_md_update(&ctx, mac, sizeof(mac)) == 0 &&
            mbedtls_md_finish(&ctx, digest) == 0;
  mbedtls_md_free(&ctx);

  lastIdentityHashOk = ok;

  // Fall back to the raw MAC bytes rather than a constant: a hash failure must
  // not give every board the same ID.
  if (!ok) for (int i = 0; i < 4; i++) digest[i] = mac[i];

  snprintf(uidOut, uidLen, "%02x%02x%02x%02x",
           digest[0], digest[1], digest[2], digest[3]);
  snprintf(idOut, idLen, "%s-%s", DEFAULT_DEVICE_ID_PREFIX, uidOut);
}

// The derived ID assumes a unique MAC. Boards from some batches ship with a
// duplicated efuse MAC, and two of those in one room derive the same ID, publish
// to the same dashboard entry and overwrite each other's readings. The override
// is the way out on the workshop floor: it is stored, so unlike the derived ID
// it does not survive a flash erase, and it deliberately does not touch the
// write key — that stays derived from the MAC, which is what lets the board
// claim the new ID at all.
//
// Normalises into `out`: trimmed, lower-cased, [a-z0-9-] only, and prefixed with
// DEFAULT_DEVICE_ID_PREFIX unless the input already carries it, so a student who
// types "bench2" still ends up inside the project's namespace. Returns false if
// nothing usable is left — the caller keeps the previous ID rather than
// publishing under a mangled one.
static bool normaliseDeviceId(const char *raw, char *out, size_t outLen) {
  if (raw == nullptr) return false;

  char cleaned[32];
  size_t n = 0;
  for (const char *p = raw; *p && n < sizeof(cleaned) - 1; p++) {
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      // No leading or doubled separators: the API takes them, dashboards read
      // badly with them.
      if (c == '-' && (n == 0 || cleaned[n - 1] == '-')) continue;
      cleaned[n++] = c;
    }
  }
  while (n > 0 && cleaned[n - 1] == '-') n--;   // nor a trailing one
  cleaned[n] = '\0';
  if (n < SHS_DEVICE_ID_MIN_LEN) return false;

  const char *prefix = DEFAULT_DEVICE_ID_PREFIX;
  size_t plen = strlen(prefix);
  bool hasPrefix = strncmp(cleaned, prefix, plen) == 0 && cleaned[plen] == '-';
  int written = hasPrefix ? snprintf(out, outLen, "%s", cleaned)
                          : snprintf(out, outLen, "%s-%s", prefix, cleaned);
  // Truncation would silently claim a different ID than the one on screen.
  return written > 0 && (size_t)written < outLen;
}

// Copy the override over the derived ID, if one is set and still valid. Called
// after every deriveIdentity() so a single path decides what settings.deviceId
// finally holds.
static void applyDeviceIdOverride() {
  if (settings.deviceIdOverride[0] == '\0') return;
  char normalised[sizeof(settings.deviceId)];
  if (normaliseDeviceId(settings.deviceIdOverride, normalised, sizeof(normalised))) {
    strlcpy(settings.deviceId, normalised, sizeof(settings.deviceId));
  } else {
    // Stored value no longer passes validation (a rule change, or corrupt
    // flash): fall back to the derived ID rather than to something invalid.
    settings.deviceIdOverride[0] = '\0';
  }
}

// Portal entry point. "" clears the override and returns to the derived ID;
// anything else is normalised first and rejected if nothing usable is left.
// Does not save — the caller batches this with the rest of the form.
bool setDeviceIdOverride(const char *raw) {
  if (raw == nullptr || raw[0] == '\0') {
    settings.deviceIdOverride[0] = '\0';
    char uid[9];
    deriveIdentity(settings.deviceId, sizeof(settings.deviceId), uid, sizeof(uid));
    return true;
  }

  char normalised[sizeof(settings.deviceId)];
  if (!normaliseDeviceId(raw, normalised, sizeof(normalised))) return false;

  strlcpy(settings.deviceIdOverride, normalised, sizeof(settings.deviceIdOverride));
  strlcpy(settings.deviceId,         normalised, sizeof(settings.deviceId));
  return true;
}

static void printIdentityDiagnostics(const char *stage) {
  uint64_t efuseMac = ESP.getEfuseMac();
  uint8_t mac[6];
  macToBytes(mac);

  char derivedId[sizeof(settings.deviceId)];
  char uid[9];
  deriveIdentity(derivedId, sizeof(derivedId), uid, sizeof(uid));

  // Keep these three lines verbatim with the issue report so logs can be
  // compared directly between colliding boards.
  Serial.printf("Efuse MAC raw: %012llx\n",
                (unsigned long long)(efuseMac & 0xFFFFFFFFFFFFULL));
  Serial.printf("MAC bytes: %02x:%02x:%02x:%02x:%02x:%02x\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("Device ID: %s\n", settings.deviceId);
  Serial.printf("Device name: %s\n", settings.deviceName);

  uint8_t staMac[6];
  bool staMacOk = esp_read_mac(staMac, ESP_MAC_WIFI_STA) == ESP_OK;

  Serial.printf("Identity[%s]: hash_ok=%d fallback=%s ",
                stage, lastIdentityHashOk ? 1 : 0,
                lastIdentityHashOk ? "no" : "raw-mac");
  Serial.printf("Identity[%s]: efuse_mac=%02x:%02x:%02x:%02x:%02x:%02x ",
                stage, mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  if (staMacOk) {
    Serial.printf("wifi_sta_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                  staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5]);
  } else {
    Serial.println("wifi_sta_mac=<unavailable>");
  }

  Serial.printf("Identity[%s]: efuse_u64=0x%012llx derived_id=%s override=%s final_id=%s\n",
                stage, (unsigned long long)(efuseMac & 0xFFFFFFFFFFFFULL),
                derivedId,
                settings.deviceIdOverride[0] ? settings.deviceIdOverride : "<none>",
                settings.deviceId);
#if SHS_DERIVED_WRITE_KEY
  Serial.printf("Identity[%s]: write_key_source=derived salt_present=yes\n", stage);
#else
  Serial.printf("Identity[%s]: write_key_source=random-nvs salt_present=no\n", stage);
#endif
}

void resetSettingsToDefaults() {
  char uid[9];
  // A factory reset drops the override: it is a setting, and the derived ID is
  // the factory state. The ID it named stays claimed by this board's write key,
  // so entering it again in the portal re-adopts it.
  settings.deviceIdOverride[0] = '\0';
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
  settings.publishIntervalMin = DEFAULT_PUBLISH_INTERVAL_MIN;
  settings.tempOffsetC        = DEFAULT_TEMP_OFFSET_C;
  settings.lcdRotation        = DEFAULT_LCD_ROTATION;

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
  // Cleared first: Preferences leaves the buffer alone when the key is absent,
  // which is exactly the case on a device flashed before this field existed.
  settings.deviceIdOverride[0] = '\0';
  settingsPrefs.getString("devIdOvr",    settings.deviceIdOverride, sizeof(settings.deviceIdOverride));
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
  settings.publishIntervalMin = settingsPrefs.getUInt("pubMin",     DEFAULT_PUBLISH_INTERVAL_MIN);
  settings.tempOffsetC        = settingsPrefs.getFloat("tempOff",   DEFAULT_TEMP_OFFSET_C);
  settings.lcdRotation        = settingsPrefs.getUChar("rot",       DEFAULT_LCD_ROTATION);
  settingsPrefs.end();

  // Identity is always recomputed, never trusted from flash: a board restored
  // from someone else's NVS dump would otherwise publish under their ID.
  char uid[9];
  deriveIdentity(settings.deviceId, sizeof(settings.deviceId), uid, sizeof(uid));
  // ...except an override the user typed in the portal, which is stored and
  // wins over the derived value. Only the ID moves; the write key stays derived
  // from the MAC, so the board still owns whatever ID it publishes under.
  applyDeviceIdOverride();

#if SHS_HAS_WORKSHOP_KEY
  // Credentials compiled into the image are a property of the image, not a user
  // setting, so they are reapplied rather than read from flash. Without this a
  // board that ever ran a different build keeps that build's stored key: NVS
  // survives re-flashing, and the settings version does not change when only
  // the credentials do. That is a 401 with nothing on the device to explain it.
  strlcpy(settings.apiKey,  DEFAULT_API_KEY, sizeof(settings.apiKey));
  strlcpy(settings.project, DEFAULT_PROJECT, sizeof(settings.project));
#endif

  // A salted build always re-derives; an unsalted one keeps whatever is stored,
  // and only mints a key if flash somehow came back empty.
  if (!deriveWriteKey(WORKSHOP_KEY_SALT, settings.writeKey, sizeof(settings.writeKey)) &&
      settings.writeKey[0] == '\0') {
    randomWriteKey(settings.writeKey, sizeof(settings.writeKey));
    saveSettings();
  }

  Serial.println("Settings: loaded from NVS");
  printIdentityDiagnostics("loaded");
}

void saveSettings() {
  settingsPrefs.begin(NVS_NAMESPACE, /* readOnly */ false);
  settingsPrefs.putString("deviceName", settings.deviceName);
  settingsPrefs.putString("devIdOvr",   settings.deviceIdOverride);
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
  settingsPrefs.putUInt("pubMin",       settings.publishIntervalMin);
  settingsPrefs.putFloat("tempOff",     settings.tempOffsetC);
  settingsPrefs.putUChar("rot",         settings.lcdRotation);
  settingsPrefs.putUInt("version",      SETTINGS_VERSION);
  settingsPrefs.end();

  Serial.println("Settings: saved to NVS");
  printIdentityDiagnostics("saved");
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
