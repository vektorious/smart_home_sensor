// ============================================================================
//  Smart Home Sensor — central configuration
//
//  This file holds only what genuinely belongs at compile time: the pin map,
//  the build variant, and the FACTORY DEFAULTS applied on first boot.
//
//  Everything a user may want to change per device — device name, project,
//  broker address and credentials, API key, publish interval, temperature
//  offset — lives in NVS and is edited from the setup portal (see portal.ino).
//  That is what lets one web-flashed binary serve every student without a
//  recompile. The DEFAULT_* values below are only the starting point.
//
//  There are no credentials in this file, so it is committed: a fresh clone
//  compiles as-is. Workshop credentials go in the gitignored
//  workshop_secrets.h (see workshop_secrets.example.h).
// ============================================================================
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Build variant — which backend this image talks to.
//
//  Pick one here for an Arduino IDE build, or override it from the command
//  line so all three images come out of the same sources:
//      arduino-cli compile --build-property \
//        "compiler.cpp.extra_flags=-DSHS_VARIANT=SHS_VARIANT_SENSORBOARD" ...
//  See web-flasher/build.sh, which builds all three.
// ---------------------------------------------------------------------------
#define SHS_VARIANT_MQTT_HA      1   // Home Assistant via MQTT (broker required)
#define SHS_VARIANT_SENSORBOARD  2   // diy-sensor.org over HTTPS (short workshop)
#define SHS_VARIANT_DISPLAY      3   // Display + serial only, no networking

#ifndef SHS_VARIANT
#define SHS_VARIANT  SHS_VARIANT_MQTT_HA
#endif

// Feature flags derived from the variant — modules compile themselves out when
// their flag is 0, so an unused backend costs nothing in the image.
#if   SHS_VARIANT == SHS_VARIANT_MQTT_HA
  #define USE_MQTT         1
  #define USE_SENSORBOARD  0
#elif SHS_VARIANT == SHS_VARIANT_SENSORBOARD
  #define USE_MQTT         0
  #define USE_SENSORBOARD  1
#elif SHS_VARIANT == SHS_VARIANT_DISPLAY
  #define USE_MQTT         0
  #define USE_SENSORBOARD  0
#else
  #error "Unknown SHS_VARIANT — see the list above"
#endif

// True when this build talks to a network at all (Wi-Fi + setup portal).
#define USE_NETWORK  (USE_MQTT || USE_SENSORBOARD)

#define USE_DISPLAY  1   // ST7789 240x240 LCD on the Waveshare board

// ---------------------------------------------------------------------------
//  Device identity defaults
//  The device ID is derived from the factory-programmed MAC in efuse
//  ("<prefix>-<mac8>"): unique per board, stable across reboots, reflashes and
//  a factory reset, so a student's dashboard entry never moves. It is shown in
//  the portal but is deliberately not editable — the device NAME is what
//  students personalise.
// ---------------------------------------------------------------------------
#define DEFAULT_DEVICE_NAME       "SHS"    // keep short — the title area fits ~14 chars
#define DEFAULT_DEVICE_ID_PREFIX  "shs"    // [a-z0-9-] only; the API rejects anything else

// ---------------------------------------------------------------------------
//  diy-sensor.org defaults (SENSORBOARD build)
//  The API key and project come from workshop_secrets.h when it is compiled in.
//  Without it the device publishes anonymously: it claims its ID with a random
//  write key kept in NVS, and the server deletes it 48 h after its last write.
//  That expiry is what makes the keyless build safe to hand out indefinitely —
//  an abandoned device cleans itself up, and a flash erase that loses the random
//  key only costs 48 h before the ID can be claimed again.
// ---------------------------------------------------------------------------
#define DEFAULT_API_URL       "https://diy-sensor.org/sensor/measurement"
#define DASHBOARD_URL_PREFIX  "diy-sensor.org/dashboard/device/"
// With the scheme, for the QR code shown after setup: a phone camera needs a
// resolvable URL, not a bare host path.
#define DASHBOARD_URL_FULL    "https://diy-sensor.org/dashboard/device/"

// The standard and workshop images are the same variant; they differ only in
// whether these credentials are compiled in. -DSHS_NO_WORKSHOP_SECRETS forces
// the standard build even on a machine that has the header, so both come out of
// one source tree in one run of build.sh.
// Gated on USE_SENSORBOARD as well: without that, the credentials would be
// linked into the Home Assistant and display-only images too — they still copy
// DEFAULT_API_KEY into settings, so the literal survives into a binary that has
// no use for it and is served publicly.
#if USE_SENSORBOARD && __has_include("workshop_secrets.h") && !defined(SHS_NO_WORKSHOP_SECRETS)
  #include "workshop_secrets.h"
#endif
#ifndef WORKSHOP_API_KEY
  #define WORKSHOP_API_KEY  ""
  #define SHS_HAS_WORKSHOP_KEY  0
#else
  #define SHS_HAS_WORKSHOP_KEY  1
#endif
#ifndef WORKSHOP_PROJECT
  #define WORKSHOP_PROJECT  ""
#endif
#ifndef WORKSHOP_KEY_SALT
  #define WORKSHOP_KEY_SALT ""       // no salt: random write key on first boot
  #define SHS_DERIVED_WRITE_KEY  0
#else
  #define SHS_DERIVED_WRITE_KEY  1   // key reproduced from the MAC at every boot
#endif

#define DEFAULT_API_KEY  WORKSHOP_API_KEY
#define DEFAULT_PROJECT  WORKSHOP_PROJECT

// ---------------------------------------------------------------------------
//  MQTT / Home Assistant defaults (MQTT_HA build)
//  Host, credentials and mode are all editable in the portal.
// ---------------------------------------------------------------------------
#define DEFAULT_MQTT_HOST      ""              // set in the portal
#define DEFAULT_MQTT_PORT      1883
#define DEFAULT_MQTT_USER      ""
#define DEFAULT_MQTT_PASS      ""
#define DEFAULT_MQTT_TLS       false
#define DEFAULT_MQTT_PREFIX    "smart_home_sensor"   // <prefix>/<id>/state, .../status
#define DEFAULT_HA_DISC_PREFIX "homeassistant"       // HA's discovery topic root

// How readings reach Home Assistant:
//   MQTT_MODE_DISCOVERY — retained discovery configs + one JSON state topic.
//                         HA creates the entities itself; nothing to configure.
//   MQTT_MODE_FLAT      — one plain topic per metric (<prefix>/<id>/temperature).
//                         For non-HA consumers, or manual HA sensor config.
//   MQTT_MODE_BOTH      — flat topics, plus discovery configs pointing at them.
#define MQTT_MODE_DISCOVERY  0
#define MQTT_MODE_FLAT       1
#define MQTT_MODE_BOTH       2
#define DEFAULT_MQTT_MODE    MQTT_MODE_DISCOVERY

// ---------------------------------------------------------------------------
//  Reporting cadence, in MINUTES.
//  BSEC produces a sample every ~3 s; publishing every one of them floods both
//  backends. Indoor air quality does not move fast enough to need more than a
//  reading every few minutes, and the cost of a short interval is paid by the
//  whole workshop at once: diy-sensor.org charges its write budget per
//  credential and per stored value, so N devices sharing one API key spend
//  N x (sensors per reading) / interval against the same bucket.
//  The display still updates every ~3 s regardless — this is only the
//  reporting rate.
// ---------------------------------------------------------------------------
#define DEFAULT_PUBLISH_INTERVAL_MIN  5
#define MIN_PUBLISH_INTERVAL_MIN      1
#define MAX_PUBLISH_INTERVAL_MIN      1440   // a day; beyond this it is a typo

// ---------------------------------------------------------------------------
//  Calibration defaults
//  TEMP_OFFSET_C: self-heating from the ESP32 + backlight warms the BME680
//  above true ambient. BSEC subtracts this fixed offset (°C). To calibrate:
//  run the board ~20-30 min until the reported temperature plateaus, then set
//  this to (reported - real thermometer). Editable in the portal, because the
//  right value depends on the enclosure.
//  STATE_SAVE_PERIOD_MS: how often the calibrated BSEC state is re-saved to NVS.
// ---------------------------------------------------------------------------
#define DEFAULT_TEMP_OFFSET_C  5.0f
#define STATE_SAVE_PERIOD_MS   (6UL * 60UL * 60UL * 1000UL)  // 6 hours

// ---------------------------------------------------------------------------
//  Display rotation — 0=0°, 1=90° CW, 2=180°, 3=90° CCW
//  Only the factory default: it is editable in the setup portal, because which
//  way up the board ends up depends on the enclosure and the shelf it sits on.
//  Builds without networking have no portal and so keep this value.
// ---------------------------------------------------------------------------
#define DEFAULT_LCD_ROTATION  0

// ---------------------------------------------------------------------------
//  Display pins (Waveshare ESP32-C6-LCD-1.3, ST7789 over SPI)
// ---------------------------------------------------------------------------
#define PIN_MISO     5
#define PIN_MOSI     6
#define PIN_SCLK     7
#define PIN_LCD_CS   14
#define PIN_LCD_DC   15
#define PIN_LCD_RST  21
#define PIN_BL       22

// ---------------------------------------------------------------------------
//  BME680 pins (I2C)
//  Note: avoid GPIO16/17 — those are UART0 TX/RX on the ESP32-C6 and get
//  bootloader chatter at startup, which can disturb I2C devices.
// ---------------------------------------------------------------------------
#define PIN_BME_SCL  2
#define PIN_BME_SDA  3

// ---------------------------------------------------------------------------
//  Status colours (RGB565) for the footer line. Defined here — not in
//  display.ino — so the sensor/backend modules can pass them even when the
//  display is compiled out (USE_DISPLAY 0 stubs simply ignore them).
// ---------------------------------------------------------------------------
#define SHS_RGB565(r, g, b) \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_INFO  SHS_RGB565(0x9F, 0xB6, 0xCD)   // gray-blue
#define COLOR_OK    SHS_RGB565(0x06, 0xD6, 0xA0)   // green
#define COLOR_WARN  SHS_RGB565(0xFF, 0xD1, 0x66)   // amber
#define COLOR_ERR   SHS_RGB565(0xEF, 0x47, 0x6F)   // red

// ---------------------------------------------------------------------------
//  Runtime settings — persisted in NVS, edited from the setup portal.
//  Defined in settings.ino; loaded once at boot via loadSettings().
//
//  The string fields are fixed-size and filled with strlcpy, which truncates
//  silently. A compiled-in credential that does not fit therefore produces a
//  device that looks configured and is rejected by the server — so the sizes
//  are named and checked below rather than left as literals in the struct.
// ---------------------------------------------------------------------------
#define SHS_PROJECT_LEN    48
#define SHS_WRITE_KEY_LEN  40   // 32 hex chars + NUL
#define SHS_API_URL_LEN   128
#define SHS_API_KEY_LEN    96   // generous: keys are operator-chosen strings

// Catch an oversized compiled-in default at build time instead of discovering
// it as a 401 on the workshop floor. sizeof() on a string literal counts NUL.
static_assert(sizeof(DEFAULT_API_KEY) <= SHS_API_KEY_LEN,
              "WORKSHOP_API_KEY does not fit in Settings::apiKey — raise SHS_API_KEY_LEN");
static_assert(sizeof(DEFAULT_PROJECT) <= SHS_PROJECT_LEN,
              "WORKSHOP_PROJECT does not fit in Settings::project — raise SHS_PROJECT_LEN");
static_assert(sizeof(DEFAULT_API_URL) <= SHS_API_URL_LEN,
              "DEFAULT_API_URL does not fit in Settings::apiUrl — raise SHS_API_URL_LEN");
struct Settings {
  char     deviceName[32];
  char     deviceId[32];          // derived from the MAC; not user-editable
  char     project[SHS_PROJECT_LEN];
  char     writeKey[SHS_WRITE_KEY_LEN];
  char     apiUrl[SHS_API_URL_LEN];
  char     apiKey[SHS_API_KEY_LEN];
  char     mqttHost[64];
  char     mqttUser[32];
  char     mqttPass[64];
  char     mqttPrefix[32];
  char     haDiscPrefix[32];
  uint16_t mqttPort;
  uint8_t  mqttMode;              // MQTT_MODE_*
  bool     mqttTls;
  uint32_t publishIntervalMin;
  uint8_t  lcdRotation;           // 0-3, quarter turns
  float    tempOffsetC;
};

extern Settings settings;

void loadSettings();               // populate `settings` from NVS (or defaults on first boot)
void saveSettings();               // persist `settings` to NVS
void resetSettingsToDefaults();    // DEFAULT_* into RAM; call saveSettings() to persist
void clearWiFiCredentials();       // forget the saved network only
void clearBsecState();             // discard the IAQ calibration only

// ---------------------------------------------------------------------------
//  Shared sensor reading — filled by bme680.ino, consumed by display.ino and
//  the backend modules. NAN marks a value BSEC has not produced yet.
// ---------------------------------------------------------------------------
struct SensorPacket {
  float   iaq          = NAN;
  uint8_t iaqAccuracy  = 0;     // 0=stabilizing .. 3=fully calibrated
  float   co2          = NAN;   // ppm  (CO2-equivalent)
  float   voc          = NAN;   // ppm  (breath-VOC-equivalent)
  float   temperature  = NAN;   // °C   (heat-compensated)
  float   humidity     = NAN;   // %    (heat-compensated)
  float   pressure     = NAN;   // hPa
};

// Functions taking SensorPacket by reference need explicit prototypes: the
// Arduino auto-prototype generator skips reference parameters.
SensorPacket sensorLatest();               // bme680.ino — most recent reading
bool         bme680Ok();                   // bme680.ino — sensor found and subscribed
void         displayUpdate(const SensorPacket &p);
void         displayPortalSensor(const char *msg, uint16_t color);
void         displayQr(const char *url, uint32_t showMs);
void         mqttPublish(const SensorPacket &p);
int          sensorboardSend(const SensorPacket &p);  // HTTP code, -1 if offline
bool         isValidFloat(float v);
