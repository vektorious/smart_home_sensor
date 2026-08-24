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
  cd ~/Arduino/libraries/bsec2/src
  mkdir -p esp32c6 && cp esp32c3/libalgobsec.a esp32c6/libalgobsec.a
  ```
  Folder named differently? Find the one holding `esp32c3/libalgobsec.a`.
  Re-run after any BSEC2 library update (fixes `libalgobsec` linker errors).

---

## 3. Pick a build, then set up on the device

The only compile-time choice — in `code/shs_modular/config.h`:

```c
#define SHS_VARIANT  SHS_VARIANT_MQTT_HA      // Home Assistant over MQTT
// SHS_VARIANT_SENSORBOARD  → diy-sensor.org      SHS_VARIANT_DISPLAY → no network
```

Everything else is entered **on the device**: name, interval, temp offset, display
rotation, plus MQTT broker and credentials (HA build) or API URL and key
(diy-sensor build).

Upload — or flash a prebuilt image from the [web flasher](../../web-flasher/). First boot
opens the AP **`SHS-xxxxxxxx-Setup`** (no password) → browse to **`192.168.4.1`** → Configure
WiFi → Setup → *Live readings & connection test* → **Send a test reading** → Finish.

**Back into setup later: press RESET twice quickly.**
(Upload stuck? Hold BOOT while plugging in USB, release after 2 s.)

---

## 4. Home Assistant — MQTT

1. Install & start the **Mosquitto broker** add-on; add the **MQTT** integration.
2. Make an MQTT user; enter it in the device's setup portal (no re-upload).
3. The device auto-appears under MQTT with entities: IAQ, IAQ Accuracy,
   CO₂ equivalent, Breath VOC equivalent, Temperature, Humidity, Pressure.

---

## 5. Reading the air quality

IAQ accuracy: **0** stabilizing · **1–2** calibrating (hours) · **3** trusted. IAQ bands:
0–50 good · 51–100 moderate · 101–150 light · 151–200 moderate · 201–300 heavy · 300+ severe.
Temp offset: after a 20–30 min warm-up, **add** the leftover error to the offset already set
(`new = current + (reported − real)`); the default is 5 °C, not 0.
Accuracy dropping 3→2→1 is BSEC rebuilding its baseline after a change, a restart, or
unchanging air. Normal; it climbs back.

---

## Alternative: ESPHome

Install the **ESPHome** add-on → add `code/esphome/smart_home_sensor.yaml` → create
`secrets.yaml` (wifi_ssid, wifi_password, ap_password) → Install → accept the auto-discovered
device. No broker needed; minimal display only.
