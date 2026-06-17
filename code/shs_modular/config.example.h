// ============================================================================
//  Smart Home Sensor — central configuration
//  Copy this file to config.h and fill in your own values.
//  config.h is gitignored so your credentials stay local.
// ============================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
//  Feature flags — set to 0 to compile a module out entirely.
// ---------------------------------------------------------------------------
#define USE_DISPLAY  1   // ST7789 240x240 LCD on the Waveshare board
#define USE_MQTT     1   // Wi-Fi + MQTT publishing to Home Assistant
                         // Set USE_MQTT 0 for a standalone display/serial device.

// ---------------------------------------------------------------------------
//  Device identity
//  DEVICE_NAME is shown in Home Assistant and used for the Wi-Fi setup AP
//  ("<DEVICE_NAME>-Setup"). DEVICE_ID must be unique per device — it keys the
//  MQTT topics and the Home Assistant entity unique_ids.
// ---------------------------------------------------------------------------
#define DEVICE_NAME  "SHS"              // keep short — title area is ~14 chars at textSize(2)
#define DEVICE_ID    "shs-livingroom"      // a-z, 0-9, '-' only; unique per device

// ---------------------------------------------------------------------------
//  MQTT broker (e.g. the Mosquitto add-on in Home Assistant)
//  Leave USER/PASS empty ("") for an anonymous broker.
// ---------------------------------------------------------------------------
#define MQTT_TLS             0                // 1 = TLS/MQTTS, 0 = plain (local Mosquitto)
#define MQTT_HOST            "172.22.1.149"   // broker IP or hostname
#define MQTT_PORT            1883
#define MQTT_USER            "mqtt-user"
#define MQTT_PASS            "mqtt-password"
#define HA_DISCOVERY_PREFIX  "homeassistant"  // HA default; change only if you did
#define MQTT_PUBLISH_MS      30000UL          // publish to HA at most every 30 s

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
//  Calibration
//  TEMP_OFFSET_C: self-heating from the ESP32 + backlight warms the BME680
//  above true ambient. BSEC subtracts this fixed offset (°C). To calibrate:
//  run the board ~20-30 min until the reported temperature plateaus, then set
//  this to (reported - real thermometer).
//  STATE_SAVE_PERIOD_MS: how often the calibrated BSEC state is re-saved to NVS.
// ---------------------------------------------------------------------------
#define TEMP_OFFSET_C         5.0f
#define STATE_SAVE_PERIOD_MS  (6UL * 60UL * 60UL * 1000UL)  // 6 hours

// ---------------------------------------------------------------------------
//  Status colours (RGB565) for the footer line. Defined here — not in
//  display.ino — so the sensor/MQTT modules can pass them even when the
//  display is compiled out (USE_DISPLAY 0 stubs simply ignore them).
// ---------------------------------------------------------------------------
#define SHS_RGB565(r, g, b) \
  ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_INFO  SHS_RGB565(0x9F, 0xB6, 0xCD)   // gray-blue
#define COLOR_OK    SHS_RGB565(0x06, 0xD6, 0xA0)   // green
#define COLOR_WARN  SHS_RGB565(0xFF, 0xD1, 0x66)   // amber
#define COLOR_ERR   SHS_RGB565(0xEF, 0x47, 0x6F)   // red

// ---------------------------------------------------------------------------
//  Shared sensor reading — filled by bme680.ino, consumed by display.ino and
//  mqtt.ino. NAN marks a value that BSEC has not produced yet.
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
