# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Smart Home Sensor Starter Kit — an educational IoT project for student workshops. The device
reads a **Bosch BME680** over I2C, runs **Bosch BSEC2** to derive air-quality outputs (IAQ,
CO₂-equivalent, breath-VOC-equivalent) alongside temperature/humidity/pressure, shows them on a
240×240 ST7789 display, and reports them to **Home Assistant**.

Unlike the sister "Smart Plants" workshop, this device is **always on / USB-powered** — there is
**no deep sleep and no battery**. The live display and BSEC's multi-day gas-sensor
self-calibration require continuous operation. There is also **no custom PCB**: the BME680
connects to the board with four I2C jumper wires.

**Hardware:** Waveshare **ESP32-C6-LCD-1.3** (ESP32-C6 + ST7789 240×240 IPS display) + BME680.

## Two firmware variants (pick one)

- **Main build — `code/shs_modular/`** (Arduino): WiFiManager + PubSubClient publishing to Home
  Assistant via **MQTT auto-discovery**. Keeps the custom colour display UI. *Workshop default.*
- **Alternative — `code/esphome/smart_home_sensor.yaml`** (ESPHome): native HA API, no broker.
  Exposes the same metrics but only a minimal display. Not verified on hardware yet.

`code/legacy/test_wv_display/` is the original pre-networking sketch (display + sensor only),
kept for reference — it is the source the `display.ino` / `bme680.ino` modules were extracted
from.

## Build & Upload (main build)

Arduino IDE project — no Makefile.

1. Arduino IDE + esp32 board package (Espressif). Board: **ESP32C6 Dev Module**.
2. Libraries: GFX Library for Arduino, BSEC2 Software Library, BME68x Sensor library,
   WiFiManager (tzapu), PubSubClient (Nick O'Leary).
3. **BSEC ESP32-C6 blob gotcha (critical):** the BSEC2 library ships no esp32c6 precompiled
   blob. The C6 is soft-float RISC-V (`rv32imac`), ABI-compatible with the C3 blob, so create
   it as a copy:
   ```bash
   cd ~/Arduino/libraries/BSEC2_Software_Library/src
   mkdir -p esp32c6 && cp esp32c3/libalgobsec.a esp32c6/libalgobsec.a
   ```
   A library update removes `esp32c6/` — recreate it if linking fails.
4. Upload. Deep-sleep/port-busy recovery: hold BOOT while plugging in USB, release after ~2 s.

## Code Architecture (`code/shs_modular/`)

Arduino multi-file sketch: all `.ino` files are concatenated into one translation unit, so
functions/globals are mutually visible and prototypes are auto-generated. `config.h` is the
single include shared by every module.

**Feature flags** in `config.h`:
```c
#define USE_DISPLAY 1   // ST7789 UI
#define USE_MQTT    1   // Wi-Fi + MQTT to Home Assistant (0 = standalone display)
```

**Flow** (`shs_modular.ino`): `setup()` → `displayInit()` → `bme680Init()` → `wifiConnect()` →
`mqttConnect()`; `loop()` → `bme680Run()` + `mqttLoop()`. BSEC samples every ~3 s in LP mode
and fires `newDataCallback`, which fills a `SensorPacket` and calls `displayUpdate()` +
`mqttPublish()`.

| File | Responsibility |
|------|----------------|
| `config.h` | Flags, pins, device id, MQTT creds, calibration, `SensorPacket`, status colours |
| `shs_modular.ino` | Entry point; wires modules together |
| `display.ino` | ST7789 UI; stubbed out when `USE_DISPLAY 0` |
| `bme680.ino` | BSEC2 init, `newDataCallback`, NVS state persistence, error handling |
| `wifi.ino` | WiFiManager connect + `<DEVICE_NAME>-Setup` AP fallback |
| `mqtt.ino` | PubSubClient; HA discovery configs + JSON state topic + LWT; stubbed when `USE_MQTT 0` |
| `utils.ino` | `isValidFloat()` |

`SensorPacket` (in `config.h`) carries one reading from `bme680.ino` to both `display.ino` and
`mqtt.ino`. Status colours (`COLOR_INFO/OK/WARN/ERR`) are defined in `config.h` — not
`display.ino` — so other modules can pass them even when the display is compiled out.

## Hardware notes

- **Pins:** display SPI on GPIO5/6/7/14/15/21/22; BME680 I2C SDA=GPIO3, SCL=GPIO2. **Avoid
  GPIO16/17** (UART0 — bootloader chatter disturbs I2C).
- **BME680 I2C address:** probes `0x76` then `0x77`.
- **Self-heating:** `TEMP_OFFSET_C` (default 5.0 °C) compensates ESP32 + backlight heat. Final
  value depends on the production enclosure — see TODO.
- **BSEC accuracy 0–3:** IAQ only trustworthy at 3; first calibration takes hours, 4-day window.
  State saved to NVS and restored on boot.

## Home Assistant integration

- **MQTT (main):** retained discovery configs to
  `homeassistant/sensor/<DEVICE_ID>/<metric>/config`; readings as JSON to
  `smart_home_sensor/<DEVICE_ID>/state`; availability via LWT on `.../status`. Needs the
  Mosquitto broker + MQTT integration.
- **ESPHome (alt):** native API, auto-discovered, no broker.

## Documentation

- `instructions/build_instructions.md` — student-facing build guide (start here).
- `instructions/background_information.md` — BME680/BSEC/IAQ theory, MQTT-vs-ESPHome, system path.
- `instructions/quick_reference/quick_reference.md` — printable one-pager.
- `TODO.md` — outstanding items (photos, enclosure, on-hardware verification, MQTT defaults).
