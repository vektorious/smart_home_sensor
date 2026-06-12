// ============================================================================
//  display.ino — ST7789 240x240 UI
//  Lifted from the original test_wv_display sketch. Renders a static layout
//  once (displayInit) and updates the value rows each time a new SensorPacket
//  arrives (displayUpdate). All functions are no-ops when USE_DISPLAY is 0.
// ============================================================================
#include "config.h"

#if USE_DISPLAY
#include <Arduino_GFX_Library.h>

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_LCD_DC, PIN_LCD_CS, PIN_SCLK, PIN_MOSI, PIN_MISO);

// rotation: 0=native, 1=90° CW, 2=180°, 3=90° CCW
// The ST7789 has a 240x320 internal framebuffer; for rotations 2/3 on a
// 240x240 panel we shift the visible window by 80px via row_offset2.
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, PIN_LCD_RST, 3 /* rotation: 90° CCW */, true /* IPS */,
    240, 240,
    0  /* col_offset1 */, 0  /* row_offset1 */,
    0  /* col_offset2 */, 80 /* row_offset2 */);

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

static void drawStaticUI() {
  gfx->fillScreen(BG_COLOR);
  gfx->setTextWrap(false);

  gfx->setTextColor(TITLE_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(LABEL_X, 6);
  gfx->print("ESP32-C6 BME680");

  // status / accuracy drawn dynamically in the footer by displayStatus()

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

// ---- Public API (called from setup, bme680.ino, utils.ino) ----------------

void displayInit() {
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  if (!gfx->begin()) Serial.println("gfx->begin() failed");
  drawStaticUI();
}

// Footer status line (e.g. "Stabilizing", "Calibrated", error messages).
void displayStatus(const char *msg, uint16_t color) {
  gfx->fillRect(0, FOOTER_Y - 2, 240, 24, BG_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  gfx->setCursor(LABEL_X, FOOTER_Y);
  gfx->print(msg);
}

// Map BSEC IAQ accuracy (0..3) to a short status string + color.
void displayAccuracy(uint8_t accuracy) {
  switch (accuracy) {
    case 0:  displayStatus("Stabilizing", COLOR_INFO); break;
    case 1:  displayStatus("Calibrating (1)", COLOR_WARN); break;
    case 2:  displayStatus("Calibrating (2)", COLOR_WARN); break;
    default: displayStatus("Calibrated", COLOR_OK); break;
  }
}

// Redraw all value rows from the latest packet.
void displayUpdate(const SensorPacket &p) {
  char buf[24];
  displayAccuracy(p.iaqAccuracy);

  if (!isnan(p.iaq))         { snprintf(buf, sizeof(buf), "%.0f", p.iaq);            drawValue(ROW_IAQ,  buf, iaqColor(p.iaq)); }
  if (!isnan(p.co2))         { snprintf(buf, sizeof(buf), "%.0f ppm", p.co2);        drawValue(ROW_CO2,  buf, co2Color(p.co2)); }
  if (!isnan(p.voc))         { snprintf(buf, sizeof(buf), "%.1f ppm", p.voc);        drawValue(ROW_VOC,  buf, VALUE_COLOR); }
  if (!isnan(p.temperature)) { snprintf(buf, sizeof(buf), "%.1f C", p.temperature);  drawValue(ROW_TEMP, buf, VALUE_COLOR); }
  if (!isnan(p.humidity))    { snprintf(buf, sizeof(buf), "%.1f %%", p.humidity);    drawValue(ROW_HUM,  buf, VALUE_COLOR); }
  if (!isnan(p.pressure))    { snprintf(buf, sizeof(buf), "%.1f hPa", p.pressure);   drawValue(ROW_PRES, buf, VALUE_COLOR); }
}

// Show placeholder dashes when the sensor failed to initialise.
void displayNoSensor() {
  drawValue(ROW_IAQ,  "---",      LABEL_COLOR);
  drawValue(ROW_CO2,  "--- ppm",  LABEL_COLOR);
  drawValue(ROW_VOC,  "--- ppm",  LABEL_COLOR);
  drawValue(ROW_TEMP, "--.- C",   LABEL_COLOR);
  drawValue(ROW_HUM,  "--.- %",   LABEL_COLOR);
  drawValue(ROW_PRES, "---- hPa", LABEL_COLOR);
}

#else  // ---- USE_DISPLAY == 0 : stub out the display API -------------------

void displayInit() {}
void displayStatus(const char *, uint16_t) {}
void displayAccuracy(uint8_t) {}
void displayUpdate(const SensorPacket &) {}
void displayNoSensor() {}

#endif
