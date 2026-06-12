# Smart Home Sensor — Quick Build Reference

A one-page summary. Full guide: [`../build_instructions.md`](../build_instructions.md).

## 1. Wire the BME680 (I2C)

| BME680 | Board | |
|--------|-------|--|
| VCC    | 3V3   | 3.3 V only |
| GND    | GND   | |
| SDA    | GPIO3 | data |
| SCL    | GPIO2 | clock |

> Avoid GPIO16/17 (UART0). I2C address is `0x76` (some modules `0x77`).

---

## 2. Arduino Setup (main build)

- **Board:** ESP32C6 Dev Module (esp32 by Espressif).
- **Boards Manager URL:** `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- **Libraries:** GFX Library for Arduino · BSEC2 Software Library · BME68x Sensor library ·
  WiFiManager (tzapu) · PubSubClient (Nick O'Leary).
- **BSEC C6 blob (required):**
  ```bash
  cd ~/Arduino/libraries/BSEC2_Software_Library/src
  mkdir -p esp32c6 && cp esp32c3/libalgobsec.a esp32c6/libalgobsec.a
  ```
  Re-run after any BSEC2 library update (fixes `libalgobsec` linker errors).

---

## 3. Configure `code/shs_modular/config.h`

```c
#define DEVICE_NAME  "Smart Home Sensor"
#define DEVICE_ID    "shs-livingroom"   // unique: a-z 0-9 '-'
#define MQTT_HOST    "192.168.1.10"
#define MQTT_USER    "mqtt-user"
#define MQTT_PASS    "mqtt-password"
#define USE_DISPLAY  1
#define USE_MQTT     1                  // 0 = display only, no network
```

Upload. First boot opens a Wi-Fi AP **`Smart Home Sensor-Setup`** — connect and enter Wi-Fi
credentials. (Upload stuck? Hold BOOT while plugging in USB, release after 2 s.)

---

## 4. Home Assistant — MQTT

1. Install & start the **Mosquitto broker** add-on; add the **MQTT** integration.
2. Make an MQTT user; put it in `config.h`; re-upload.
3. Device **Smart Home Sensor** auto-appears under MQTT with entities: IAQ, IAQ Accuracy,
   CO₂ equivalent, Breath VOC equivalent, Temperature, Humidity, Pressure.

---

## 5. Reading the air quality

IAQ accuracy: **0** stabilizing · **1–2** calibrating (hours) · **3** trusted. IAQ bands:
0–50 good · 51–100 moderate · 101–150 light · 151–200 moderate · 201–300 heavy · 300+ severe.
Set `TEMP_OFFSET_C` to (reported − real thermometer) after a 20–30 min warm-up.

---

## Alternative: ESPHome

Install the **ESPHome** add-on → add `code/esphome/smart_home_sensor.yaml` → create
`secrets.yaml` (wifi_ssid, wifi_password, ap_password) → Install → accept the auto-discovered
device. No broker needed; minimal display only.
