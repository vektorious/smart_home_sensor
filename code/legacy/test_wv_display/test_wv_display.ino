// Waveshare ESP32-C6-LCD-1.3 + BME680, processed with Bosch BSEC2
// Display: ST7789 240x240 via SPI (Arduino_GFX_Library)
// Sensor:  BME680 via I2C — VCC to 3V3, GND to GND, SCL=GPIO2, SDA=GPIO3
//
// Required libraries: GFX Library for Arduino, bsec2, BME68x Sensor library
// Board: "ESP32C6 Dev Module"
//
// NOTE: bsec2 1.10.2610 ships no esp32c6 precompiled blob. The C6 is
// soft-float RISC-V (rv32imac), ABI-compatible with the C3 blob, so
// src/esp32c6/libalgobsec.a was created as a copy of src/esp32c3/. A
// library update will remove that folder — recreate it if linking fails.

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <bsec2.h>
#include "bsec_config_33v_3s_4d.h"  // BSEC tuning for 3.3 V supply, LP (3 s)

// ---------- Display pins ----------
#define PIN_MISO   5
#define PIN_MOSI   6
#define PIN_SCLK   7
#define PIN_LCD_CS 14
#define PIN_LCD_DC 15
#define PIN_LCD_RST 21
#define PIN_BL     22

// ---------- BME680 pins ----------
// Note: avoid GPIO16/17 — those are UART0 TX/RX on ESP32-C6 and get
// bootloader chatter at startup, which can disturb I2C devices.
#define PIN_BME_SCL 2
#define PIN_BME_SDA 3

// ---------- Temperature offset ----------
// External self-heating from the ESP32 + LCD backlight warms the BME680
// above true ambient. BSEC subtracts this fixed offset (°C) from the
// reading. To calibrate: let the board run ~20-30 min until the reported
// temperature plateaus, then set this to (reported - real thermometer).
#define TEMP_OFFSET_C 5.0f

// ---------- BSEC state persistence ----------
// Re-save the calibration state to NVS at most this often, and only once
// the IAQ accuracy has reached 3 (fully calibrated). Restoring this on boot
// lets auto-calibration pick up where it left off instead of starting cold.
#define STATE_SAVE_PERIOD_MS  (6UL * 60UL * 60UL * 1000UL)  // 6 hours

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_LCD_DC, PIN_LCD_CS, PIN_SCLK, PIN_MOSI, PIN_MISO);

// rotation: 0=native, 1=90° CW, 2=180°, 3=90° CCW
// The ST7789 has a 240x320 internal framebuffer; for rotations 2/3 on a
// 240x240 panel we shift the visible window by 80px via row_offset2.
Arduino_GFX *gfx = new Arduino_ST7789(
    bus, PIN_LCD_RST, 3 /* rotation: 90° CCW */, true /* IPS */,
    240, 240,
    0  /* col_offset1 */, 0  /* row_offset1 */,
    0  /* col_offset2 */, 80 /* row_offset2 */);

Bsec2 envSensor;
Preferences prefs;
bool bmeReady = false;

uint8_t  bsecState[BSEC_MAX_STATE_BLOB_SIZE];
uint32_t lastStateSave = 0;
int      lastSavedAccuracy = -1;

#define BG_COLOR    RGB565(0x10, 0x18, 0x20)
#define TITLE_COLOR RGB565(0xFF, 0xD1, 0x66)
#define LABEL_COLOR RGB565(0x9F, 0xB6, 0xCD)
#define VALUE_COLOR 0xFFFF
#define OK_COLOR    RGB565(0x06, 0xD6, 0xA0)
#define WARN_COLOR  RGB565(0xFF, 0xD1, 0x66)
#define ERR_COLOR   RGB565(0xEF, 0x47, 0x6F)

// ---------- Display layout (240x240) ----------
// Six value rows + a title and a status/accuracy footer. Labels are left-
// aligned at LABEL_X; values are right-aligned to the screen edge.
#define LABEL_X     10
#define VAL_CLEAR_X 84      // value area (cleared + right-aligned) starts here
#define ROW_IAQ     38
#define ROW_CO2     68
#define ROW_VOC     98
#define ROW_TEMP    128
#define ROW_HUM     158
#define ROW_PRES    188
#define FOOTER_Y    216

// Virtual sensors we want BSEC to compute and hand back via the callback.
bsecSensor sensorList[] = {
  BSEC_OUTPUT_IAQ,
  BSEC_OUTPUT_CO2_EQUIVALENT,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  BSEC_OUTPUT_RAW_PRESSURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
};

static void drawStaticUI() {
  gfx->fillScreen(BG_COLOR);
  gfx->setTextWrap(false);

  gfx->setTextColor(TITLE_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(LABEL_X, 6);
  gfx->print("ESP32-C6 BME680");

  // status / accuracy drawn dynamically in the footer by drawStatus()

  // row labels
  gfx->setTextSize(2);
  gfx->setTextColor(LABEL_COLOR);
  gfx->setCursor(LABEL_X, ROW_IAQ);  gfx->print("IAQ");
  gfx->setCursor(LABEL_X, ROW_CO2);  gfx->print("CO2");
  gfx->setCursor(LABEL_X, ROW_VOC);  gfx->print("VOC");
  gfx->setCursor(LABEL_X, ROW_TEMP); gfx->print("Temp");
  gfx->setCursor(LABEL_X, ROW_HUM);  gfx->print("Hum");
  gfx->setCursor(LABEL_X, ROW_PRES); gfx->print("Press");
}

static void drawStatus(const char *msg, uint16_t color) {
  gfx->fillRect(0, FOOTER_Y - 2, 240, 24, BG_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  gfx->setCursor(LABEL_X, FOOTER_Y);
  gfx->print(msg);
}

// Right-align a value string to the screen edge on its row.
static void drawValue(int16_t y, const char *value, uint16_t color) {
  gfx->fillRect(VAL_CLEAR_X, y, 240 - VAL_CLEAR_X, 18, BG_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(value, 0, y, &x1, &y1, &w, &h);
  gfx->setCursor(240 - w - 6, y);
  gfx->print(value);
}

// IAQ band colors (0-50 good ... >300 severe), per BSEC's IAQ scale.
static uint16_t iaqColor(float iaq) {
  if (iaq <= 50)  return RGB565(0x06, 0xD6, 0xA0);  // good        - green
  if (iaq <= 100) return RGB565(0xA8, 0xD8, 0x3A);  // moderate    - lime
  if (iaq <= 150) return RGB565(0xFF, 0xD1, 0x66);  // light poll. - amber
  if (iaq <= 200) return RGB565(0xFF, 0x9F, 0x40);  // moderate p. - orange
  if (iaq <= 300) return RGB565(0xEF, 0x47, 0x6F);  // heavy poll. - red
  return RGB565(0xB5, 0x17, 0x9E);                  // severe      - magenta
}

// CO2-equivalent band colors (ppm). ~400 = outdoor air, 1000 is the common
// indoor "ventilate" threshold, >2000 is clearly stuffy.
static uint16_t co2Color(float co2) {
  if (co2 <= 800)  return RGB565(0x06, 0xD6, 0xA0);  // fresh    - green
  if (co2 <= 1000) return RGB565(0xA8, 0xD8, 0x3A);  // good     - lime
  if (co2 <= 1500) return RGB565(0xFF, 0xD1, 0x66);  // moderate - amber
  if (co2 <= 2000) return RGB565(0xFF, 0x9F, 0x40);  // poor     - orange
  return RGB565(0xEF, 0x47, 0x6F);                   // bad      - red
}

// Map BSEC IAQ accuracy (0..3) to a short status string + color.
static void drawAccuracyStatus(uint8_t accuracy) {
  switch (accuracy) {
    case 0:  drawStatus("Stabilizing", LABEL_COLOR); break;
    case 1:  drawStatus("Calibrating (1)", WARN_COLOR); break;
    case 2:  drawStatus("Calibrating (2)", WARN_COLOR); break;
    default: drawStatus("Calibrated", OK_COLOR); break;
  }
}

static void checkBsecStatus() {
  char buf[24];

  if (envSensor.sensor.status < BME68X_OK) {
    // I2C/sensor-level failure (bad wiring, wrong address, no chip ID).
    Serial.printf("BME68x error %d\n", envSensor.sensor.status);
    snprintf(buf, sizeof(buf), "BME68x err %d", envSensor.sensor.status);
    drawStatus(buf, ERR_COLOR);
  } else if (envSensor.sensor.status > BME68X_OK) {
    Serial.printf("BME68x warning %d\n", envSensor.sensor.status);
  }

  if (envSensor.status < BSEC_OK) {
    // BSEC-library-level failure (init/version/subscription).
    Serial.printf("BSEC error %d\n", envSensor.status);
    snprintf(buf, sizeof(buf), "BSEC err %d", envSensor.status);
    drawStatus(buf, ERR_COLOR);
  } else if (envSensor.status > BSEC_OK) {
    Serial.printf("BSEC warning %d\n", envSensor.status);
  }
}

static void loadState() {
  prefs.begin("bsec", /* readOnly */ true);
  size_t len = prefs.getBytesLength("state");
  if (len == BSEC_MAX_STATE_BLOB_SIZE) {
    prefs.getBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
    prefs.end();
    if (envSensor.setState(bsecState)) {
      Serial.println("BSEC state restored from NVS");
    } else {
      checkBsecStatus();
    }
  } else {
    prefs.end();
    Serial.println("No saved BSEC state — starting fresh calibration");
  }
}

static void saveState() {
  if (!envSensor.getState(bsecState)) {
    checkBsecStatus();
    return;
  }
  prefs.begin("bsec", /* readOnly */ false);
  prefs.putBytes("state", bsecState, BSEC_MAX_STATE_BLOB_SIZE);
  prefs.end();
  lastStateSave = millis();
  Serial.println("BSEC state saved to NVS");
}

// Called by BSEC each time a new set of processed outputs is ready (~3 s in LP).
void newDataCallback(const bme68xData data, const bsecOutputs outputs,
                     const Bsec2 bsec) {
  if (!outputs.nOutputs) return;

  float iaq = NAN, co2 = NAN, voc = NAN, temp = NAN, hum = NAN, pres = NAN;
  uint8_t iaqAccuracy = 0;

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData out = outputs.output[i];
    switch (out.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        iaq = out.signal;
        iaqAccuracy = out.accuracy;
        break;
      case BSEC_OUTPUT_CO2_EQUIVALENT:                  co2  = out.signal; break;
      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:           voc  = out.signal; break;
      // BSEC reports raw pressure already in hPa-scale here (~1014), so no
      // Pa->hPa division is needed — dividing gave a 100x-too-small value.
      case BSEC_OUTPUT_RAW_PRESSURE:                    pres = out.signal; break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE: temp = out.signal; break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:    hum  = out.signal; break;
    }
  }

  char buf[24];
  drawAccuracyStatus(iaqAccuracy);

  if (!isnan(iaq))  { snprintf(buf, sizeof(buf), "%.0f", iaq);      drawValue(ROW_IAQ,  buf, iaqColor(iaq)); }
  if (!isnan(co2))  { snprintf(buf, sizeof(buf), "%.0f ppm", co2);  drawValue(ROW_CO2,  buf, co2Color(co2)); }
  if (!isnan(voc))  { snprintf(buf, sizeof(buf), "%.1f ppm", voc);  drawValue(ROW_VOC,  buf, VALUE_COLOR); }
  if (!isnan(temp)) { snprintf(buf, sizeof(buf), "%.1f C", temp);   drawValue(ROW_TEMP, buf, VALUE_COLOR); }
  if (!isnan(hum))  { snprintf(buf, sizeof(buf), "%.1f %%", hum);    drawValue(ROW_HUM,  buf, VALUE_COLOR); }
  if (!isnan(pres)) { snprintf(buf, sizeof(buf), "%.1f hPa", pres); drawValue(ROW_PRES, buf, VALUE_COLOR); }

  Serial.printf("IAQ=%.0f(a%u)  CO2=%.0fppm  VOC=%.2fppm  T=%.2fC  H=%.2f%%  P=%.2fhPa\n",
                iaq, iaqAccuracy, co2, voc, temp, hum, pres);

  // Persist calibration once it's trustworthy, then at most every few hours,
  // or whenever the accuracy level improves.
  if (iaqAccuracy >= 3 &&
      ((int)iaqAccuracy != lastSavedAccuracy ||
       millis() - lastStateSave >= STATE_SAVE_PERIOD_MS)) {
    saveState();
    lastSavedAccuracy = iaqAccuracy;
  }
}

static bool initBSEC() {
  Wire.begin(PIN_BME_SDA, PIN_BME_SCL);

  Serial.println("Probing BME680 at 0x76...");
  drawStatus("1: I2C probe", LABEL_COLOR);
  bool ok = envSensor.begin(BME68X_I2C_ADDR_LOW, Wire);
  if (!ok) {
    Serial.printf("  0x76 failed (sensor.status=%d, bsec.status=%d)\n",
                  envSensor.sensor.status, envSensor.status);
    Serial.println("Probing BME680 at 0x77...");
    ok = envSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  }
  if (!ok) {
    Serial.printf("  begin() failed (sensor.status=%d, bsec.status=%d)\n",
                  envSensor.sensor.status, envSensor.status);
    checkBsecStatus();
    return false;
  }
  Serial.printf("begin() OK — BSEC v%d.%d.%d.%d\n",
                envSensor.version.major, envSensor.version.minor,
                envSensor.version.major_bugfix, envSensor.version.minor_bugfix);

  // Apply the 3.3 V / LP tuning before restoring state and subscribing.
  // (Recommended order: init -> config -> state -> subscription.)
  envSensor.setConfig(bsec_config_iaq);
  if (envSensor.status < BSEC_OK) {
    Serial.printf("setConfig error %d\n", envSensor.status);
    checkBsecStatus();
    return false;
  }
  Serial.println("3.3 V LP config applied");

  // Compensate for external board/LCD self-heating (see TEMP_OFFSET_C).
  envSensor.setTemperatureOffset(TEMP_OFFSET_C);

  loadState();

  // Note: updateSubscription() returns false on any non-OK status, including
  // benign positive warnings (e.g. 14 = SAMPLERATEMISMATCH, raised when no
  // matching config blob is loaded). Only a negative status is a real error.
  envSensor.updateSubscription(sensorList,
                               sizeof(sensorList) / sizeof(sensorList[0]),
                               BSEC_SAMPLE_RATE_LP);
  if (envSensor.status < BSEC_OK) {
    Serial.printf("updateSubscription error %d\n", envSensor.status);
    checkBsecStatus();
    return false;
  }
  if (envSensor.status > BSEC_OK)
    Serial.printf("updateSubscription warning %d (continuing)\n", envSensor.status);

  envSensor.attachCallback(newDataCallback);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("ESP32-C6-LCD-1.3 + BME680 (BSEC2) starting");

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);

  if (!gfx->begin()) Serial.println("gfx->begin() failed");

  drawStaticUI();
  drawStatus("Detecting...", LABEL_COLOR);

  bmeReady = initBSEC();
  if (bmeReady) {
    Serial.println("BME680 + BSEC initialized");
    drawStatus("Stabilizing", LABEL_COLOR);
  } else {
    Serial.println("BME680 / BSEC init failed");
    // Leave the specific error from checkBsecStatus() on the footer line.
    drawValue(ROW_IAQ,  "---",      LABEL_COLOR);
    drawValue(ROW_CO2,  "--- ppm",  LABEL_COLOR);
    drawValue(ROW_VOC,  "--- ppm",  LABEL_COLOR);
    drawValue(ROW_TEMP, "--.- C",   LABEL_COLOR);
    drawValue(ROW_HUM,  "--.- %",   LABEL_COLOR);
    drawValue(ROW_PRES, "---- hPa", LABEL_COLOR);
  }
}

void loop() {
  if (!bmeReady) return;

  // When it's time for a new sample (every ~3 s in LP mode) run() reads the
  // sensor and fires the callback. It returns false on any non-OK status —
  // including benign positive warnings — so only react to a negative status.
  if (!envSensor.run() &&
      (envSensor.status < BSEC_OK || envSensor.sensor.status < BME68X_OK)) {
    checkBsecStatus();
    delay(1000);
  }
}
