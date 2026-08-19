// ============================================================================
//  display.ino — ST7789 240x240 UI
//  Lifted from the original test_wv_display sketch. Renders a static layout
//  once (displayInit) and updates the value rows each time a new SensorPacket
//  arrives (displayUpdate). All functions are no-ops when USE_DISPLAY is 0.
// ============================================================================
#include "config.h"
#include <qrcode.h>   // Espressif QR encoder, bundled with the ESP32 core

#if USE_DISPLAY
#include <Arduino_GFX_Library.h>

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_LCD_DC, PIN_LCD_CS, PIN_SCLK, PIN_MOSI, PIN_MISO);

// The ST7789 has a 240x320 internal framebuffer; rotations 2/3 need an 80px
// row_offset2 to shift the visible window into the right region of that buffer.
static Arduino_GFX *gfx = new Arduino_ST7789(
    bus, PIN_LCD_RST, LCD_ROTATION, true /* IPS */,
    240, 240,
    0, 0,
    0, (LCD_ROTATION == 2 || LCD_ROTATION == 3) ? 80 : 0);

#define BG_COLOR    RGB565(0x10, 0x18, 0x20)
#define TITLE_COLOR RGB565(0xFF, 0xD1, 0x66)
#define LABEL_COLOR RGB565(0x9F, 0xB6, 0xCD)
#define VALUE_COLOR 0xFFFF
#define OK_COLOR    RGB565(0x06, 0xD6, 0xA0)
#define WARN_COLOR  RGB565(0xFF, 0xD1, 0x66)
#define ERR_COLOR   RGB565(0xEF, 0x47, 0x6F)
// QR codes need a light background and dark modules; scanners cope badly with
// the inverse, so this screen deliberately breaks the dark theme.
#define WHITE_BG    0xFFFF
#define QR_DARK     0x0000
#define QR_TEXT     RGB565(0x33, 0x33, 0x33)

// ---------- Display layout (240x240) ----------
// Six value rows + a title/status header and a footer.
// Labels are left-aligned at LABEL_X; values are right-aligned to the screen
// edge. The top-right corner (x >= CONN_X) is reserved for WiFi + MQ icons.
#define LABEL_X     10
#define VAL_CLEAR_X 84      // value area (cleared + right-aligned) starts here
#define ROW_IAQ     38
#define ROW_CO2     68
#define ROW_VOC     98
#define ROW_TEMP    128
#define ROW_HUM     158
#define ROW_PRES    188
#define FOOTER_Y    216

// Connection status area (top-right corner, x >= CONN_X)
// Icon radius 14 matches textSize(2) height (16 px); cy=22 puts the top of the
// icon at y=8, level with the device name.
#define CONN_X      181     // left edge of WiFi + MQ area
#define WIFI_CX     196     // WiFi icon centre x
#define WIFI_CY      22     // WiFi icon centre y (dot); arcs open upward
#define MQ_X        213     // "MQ" label x (textSize 2 → 24 px wide, ends at ~237)
#define MQ_Y          8     // "MQ" label y — top-aligned with device name

static bool     wifiOk    = false;
static int8_t   linkState = -1;   // -1 not tried, 0 failed, 1 ok
static bool     blinkOn   = true;
static uint32_t lastBlink = 0;

// WiFi icon: three concentric rings (2 px wide, 2 px gap) + centre dot,
// clipped to a 90° wedge (45° each side of vertical) opening upward.
// Radius 14 matches textSize(2) height so the icon sits at the same scale as
// the device name and the "MQ" label beside it.
static void drawWifiIcon(uint16_t color) {
  // Build rings outside-in: fillCircle(BG) carves each gap, fillCircle(color)
  // restores the next ring. Result: rings at r 3-5, 7-9, 11-14; gaps at 1-3, 5-7, 9-11.
  gfx->fillCircle(WIFI_CX, WIFI_CY, 14, color);
  gfx->fillCircle(WIFI_CX, WIFI_CY, 11, BG_COLOR);
  gfx->fillCircle(WIFI_CX, WIFI_CY,  9, color);
  gfx->fillCircle(WIFI_CX, WIFI_CY,  7, BG_COLOR);
  gfx->fillCircle(WIFI_CX, WIFI_CY,  5, color);
  gfx->fillCircle(WIFI_CX, WIFI_CY,  3, BG_COLOR);
  gfx->fillCircle(WIFI_CX, WIFI_CY,  1, color);    // centre dot

  // Clip outside the 90° wedge. tan(45°) = 1, so the wedge edge lands exactly
  // r px horizontally from cx — the clipping triangles are perfect right triangles.
  const int16_t cx = WIFI_CX, cy = WIFI_CY;
  gfx->fillTriangle(cx, cy, cx-15, cy-15, cx-15, cy, BG_COLOR); // left
  gfx->fillTriangle(cx, cy, cx+15, cy-15, cx+15, cy, BG_COLOR); // right
  gfx->fillRect(cx-15, cy+1, 31, 15, BG_COLOR);                  // below dot
}

// Label for the backend indicator beside the WiFi icon. The display-only build
// has no backend, so it gets no label at all rather than a permanently red one.
#if USE_MQTT
  #define LINK_LABEL "MQ"
#elif USE_SENSORBOARD
  #define LINK_LABEL "API"
#else
  #define LINK_LABEL nullptr
#endif

// Redraw the entire connection status corner (WiFi icon + backend label).
static void drawConnStatus() {
  gfx->fillRect(CONN_X, 0, 240 - CONN_X, 32, BG_COLOR);

  // WiFi icon — hidden during blink-off phase while disconnected
  if (wifiOk || blinkOn)
    drawWifiIcon(wifiOk ? OK_COLOR : ERR_COLOR);

  const char *label = LINK_LABEL;
  if (!label) return;

  // linkState is deliberately tri-state. The device only contacts the backend
  // every few minutes, so a boolean would have to start as "failed" and sit
  // there in red for the whole first interval, reporting a failure that has not
  // happened. Grey means "not tried yet".
  uint16_t color = linkState < 0 ? LABEL_COLOR : (linkState ? OK_COLOR : ERR_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  // "API" is 3 chars at textSize 2 (18 px); nudge left so it still fits.
  gfx->setCursor(strlen(label) > 2 ? MQ_X - 8 : MQ_X, MQ_Y);
  gfx->print(label);
}

static void drawStaticUI() {
  gfx->fillScreen(BG_COLOR);
  gfx->setTextWrap(false);

  // Device name — may be overwritten at right edge by drawConnStatus() below
  gfx->setTextColor(TITLE_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(LABEL_X, 8);
  gfx->print(settings.deviceName);

  // Connection status (clears x >= CONN_X and draws icons over any title overflow)
  drawConnStatus();

  // Row labels
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

// While the setup portal owns the screen, readings must not paint over it. The
// portal keeps BSEC running on purpose, so the callbacks continue to arrive;
// they are dropped here rather than at the source, which keeps the sensor's
// timing untouched and the suppression in one place.
// The portal and the QR screen both take the display over. The portal holds it
// until setup finishes; the QR releases itself after a deadline. Readings are
// dropped while either owns it, rather than pausing the sensor: the portal
// keeps BSEC sampling on purpose, since its calibration clock must not stall.
static bool     screenLocked  = false;
static uint32_t screenUnlockAt = 0;   // 0 = held until released explicitly

// Footer writer used by the portal view itself, which must bypass the latch.
static void drawStatus(const char *msg, uint16_t color) {
  gfx->fillRect(0, FOOTER_Y - 2, 240, 24, BG_COLOR);
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  gfx->setCursor(LABEL_X, FOOTER_Y);
  gfx->print(msg);
}

// ---- Public API (called from setup, bme680.ino, wifi.ino, mqtt.ino) --------

void displayInit() {
  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);
  if (!gfx->begin()) Serial.println("gfx->begin() failed");
  drawStaticUI();
}

// Footer status line (e.g. "Stabilizing", "Calibrated", error messages).
void displayStatus(const char *msg, uint16_t color) {
  if (screenLocked) return;
  drawStatus(msg, color);
}

// Map BSEC IAQ accuracy (0..3) to a short status string + color.
void displayAccuracy(uint8_t accuracy) {
  if (screenLocked) return;
  switch (accuracy) {
    case 0:  displayStatus("Stabilizing", COLOR_INFO); break;
    case 1:  displayStatus("Calibrating (1)", COLOR_WARN); break;
    case 2:  displayStatus("Calibrating (2)", COLOR_WARN); break;
    default: displayStatus("Calibrated", COLOR_OK); break;
  }
}

// Redraw all value rows from the latest packet.
void displayUpdate(const SensorPacket &p) {
  if (screenLocked) return;
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
  if (screenLocked) return;
  drawValue(ROW_IAQ,  "---",      LABEL_COLOR);
  drawValue(ROW_CO2,  "--- ppm",  LABEL_COLOR);
  drawValue(ROW_VOC,  "--- ppm",  LABEL_COLOR);
  drawValue(ROW_TEMP, "--.- C",   LABEL_COLOR);
  drawValue(ROW_HUM,  "--.- %",   LABEL_COLOR);
  drawValue(ROW_PRES, "---- hPa", LABEL_COLOR);
}

// Called after Wi-Fi connects or drops.
void displaySetWifiStatus(bool connected) {
  if (screenLocked) return;
  if (wifiOk == connected) return;
  wifiOk = connected;
  if (connected) blinkOn = true;
  drawConnStatus();
}

// Called after MQTT connects or disconnects.
void displaySetMqttStatus(bool connected) {
  int8_t next = connected ? 1 : 0;
  if (next == linkState) return;
  linkState = next;
  if (screenLocked) return;
  drawConnStatus();
}

// Drive the WiFi blink animation. Call from loop() — no-op when Wi-Fi is up.
void displayTick() {
  if (screenLocked) {
    if (screenUnlockAt && (int32_t)(millis() - screenUnlockAt) >= 0) displayResume();
    return;
  }
  if (wifiOk) return;
  uint32_t now = millis();
  if (now - lastBlink < 500) return;
  lastBlink = now;
  blinkOn = !blinkOn;
  drawConnStatus();
}

// Full-screen commissioning view. The AP name and the device ID are the two
// things a student needs while the portal is open, and they are needed exactly
// when the device has no other way to tell them — no network, no dashboard.
void displayPortal(const char *apName, const char *deviceId) {
  screenLocked = true;
  screenUnlockAt = 0;
  gfx->fillScreen(BG_COLOR);
  gfx->setTextWrap(false);

  gfx->setTextColor(TITLE_COLOR);
  gfx->setTextSize(2);
  gfx->setCursor(LABEL_X, 12);
  gfx->print("Setup mode");

  gfx->setTextSize(1);
  gfx->setTextColor(LABEL_COLOR);
  gfx->setCursor(LABEL_X, 52);
  gfx->print("Join this WiFi network:");
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_OK);
  gfx->setCursor(LABEL_X, 68);
  gfx->print(apName);

  gfx->setTextSize(1);
  gfx->setTextColor(LABEL_COLOR);
  gfx->setCursor(LABEL_X, 104);
  gfx->print("Then open:");
  gfx->setTextSize(2);
  gfx->setTextColor(VALUE_COLOR);
  gfx->setCursor(LABEL_X, 120);
  gfx->print("192.168.4.1");

  gfx->setTextSize(1);
  gfx->setTextColor(LABEL_COLOR);
  gfx->setCursor(LABEL_X, 150);
  gfx->print("Device ID:");
  gfx->setTextSize(2);
  gfx->setTextColor(VALUE_COLOR);
  gfx->setCursor(LABEL_X, 164);
  gfx->print(deviceId);

  displayPortalSensor("checking...", COLOR_INFO);
  drawStatus("Waiting for setup", COLOR_INFO);
}

// Sensor line on the setup screen. Bypasses the overlay latch by design: this
// is part of the setup view, not a reading painted over it.
void displayPortalSensor(const char *msg, uint16_t color) {
  if (!screenLocked) return;          // only meaningful while setup owns the screen
  gfx->fillRect(0, 190, 240, 20, BG_COLOR);
  gfx->setTextSize(1);
  gfx->setTextColor(LABEL_COLOR);
  gfx->setCursor(LABEL_X, 192);
  gfx->print("Sensor:");
  gfx->setTextSize(2);
  gfx->setTextColor(color);
  gfx->setCursor(LABEL_X + 52, 190);
  gfx->print(msg);
}

// Full-screen QR code, released automatically after showMs by displayTick().
// Scanning it is the shortest path from a board on the table to that board's
// dashboard on a phone; typing the device ID by hand is the alternative.
// esp_qrcode_generate() hands the finished code to a plain function pointer
// with no user context, so the rendering lives in this callback and needs
// nothing but the handle.
static void drawQrCallback(esp_qrcode_handle_t qr) {
  int size = esp_qrcode_get_size(qr);
  if (size <= 0) return;

  gfx->fillScreen(WHITE_BG);   // scanners need a light quiet zone

  const int16_t header = 22;
  int16_t avail = 240 - header - 8;
  int16_t scale = avail / size;          // whole pixels per module, or it blurs
  if (scale < 1) return;
  int16_t dim = size * scale;
  int16_t ox  = (240 - dim) / 2;
  int16_t oy  = header + (avail - dim) / 2;

  gfx->setTextSize(1);
  gfx->setTextColor(QR_TEXT);
  gfx->setCursor(LABEL_X, 8);
  gfx->print("Scan for your dashboard");

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (esp_qrcode_get_module(qr, x, y)) {
        gfx->fillRect(ox + x * scale, oy + y * scale, scale, scale, QR_DARK);
      }
    }
  }
}

void displayQr(const char *url, uint32_t showMs) {
  screenLocked   = true;
  screenUnlockAt = millis() + showMs;
  if (screenUnlockAt == 0) screenUnlockAt = 1;   // 0 means "held indefinitely"

  esp_qrcode_config_t cfg = {
    .display_func       = drawQrCallback,
    // Version 5 is 37x37 modules: 6 whole pixels each on a 240 px screen, and
    // room for a dashboard URL. Higher versions scan worse at this size.
    .max_qrcode_version = 5,
    .qrcode_ecc_level   = ESP_QRCODE_ECC_LOW,
  };
  if (esp_qrcode_generate(&cfg, url) != ESP_OK) {
    // Nothing to scan: fall back to the readings screen rather than a blank one.
    Serial.println("QR generation failed");
    displayResume();
  }
}

// Hand the screen back to the readings layout once setup is finished.
void displayResume() {
  screenLocked = false;
  screenUnlockAt = 0;
  drawStaticUI();
  drawConnStatus();
}

#else  // ---- USE_DISPLAY == 0 : stub out the display API -------------------

void displayInit() {}
void displayStatus(const char *, uint16_t) {}
void displayAccuracy(uint8_t) {}
void displayUpdate(const SensorPacket &) {}
void displayNoSensor() {}
void displaySetWifiStatus(bool) {}
void displaySetMqttStatus(bool) {}
void displayTick() {}
void displayPortal(const char *, const char *) {}
void displayPortalSensor(const char *, uint16_t) {}
void displayQr(const char *, uint32_t) {}
void displayResume() {}

#endif
