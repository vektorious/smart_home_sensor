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

## One sketch, four images

Three variants via `SHS_VARIANT` in `config.h` (or `-DSHS_VARIANT=n`):

| `SHS_VARIANT` | Value | Backend | Flags set |
|---|---|---|---|
| `SHS_VARIANT_MQTT_HA` | 1 | MQTT → Home Assistant | `USE_MQTT` |
| `SHS_VARIANT_SENSORBOARD` | 2 | HTTPS → diy-sensor.org | `USE_SENSORBOARD` |
| `SHS_VARIANT_DISPLAY` | 3 | none | neither |

`USE_NETWORK` is the OR of the two — it gates `wifi.ino` and `portal.ino`.

Variant 2 ships as **two** images: `workshop` (compiles in `workshop_secrets.h`) and
`sensorboard` (`-DSHS_NO_WORKSHOP_SECRETS` forces the keyless build even when the
header is present). The include is gated on `USE_SENSORBOARD` too, or the credentials
would be linked into the HA and display images — `resetSettingsToDefaults()` copies
`DEFAULT_API_KEY` regardless of variant, so the literal survives into binaries that
have no use for it and are served publicly. `SHS_DERIVED_WRITE_KEY` (1 when a salt is
compiled in) drives the portal's write-key UI: a salted build re-derives every boot,
so the manual override field is only offered on keyless builds, where it can take
effect. `build.sh workshop` refuses to build without the secrets header.

**The workshop key is not about persistence** (that is switched off in the server
policy). It buys the per-IP limits: anonymous allows 10 active devices, 5 new per
hour and 1,000 requests/day per IP, and a class shares one NAT address.

**Alternative — `code/esphome/smart_home_sensor.yaml`** (ESPHome): native HA API, no broker.
Same metrics, minimal display. Not verified on hardware yet.

`code/legacy/test_wv_display/` is the original pre-networking sketch (display + sensor only),
kept for reference — it is the source the `display.ino` / `bme680.ino` modules were extracted
from.

## Build & Upload

`web-flasher/build.sh` builds all four images with `arduino-cli` and drops merged binaries
into `web-flasher/firmware/`; it also recreates the missing esp32c6 BSEC blob. For the IDE:

1. Arduino IDE + esp32 board package (Espressif). Board: **ESP32C6 Dev Module**.
2. Libraries: GFX Library for Arduino, BSEC2 Software Library, BME68x Sensor library,
   WiFiManager (tzapu), PubSubClient (Nick O'Leary).
   Partition scheme: **Huge APP** — the default is too small for BSEC.
3. **BSEC ESP32-C6 blob gotcha (critical):** the BSEC2 library ships no esp32c6 precompiled
   blob. The C6 is soft-float RISC-V (`rv32imac`), ABI-compatible with the C3 blob, so create
   it as a copy:
   ```bash
   cd ~/Arduino/libraries/bsec2/src
   mkdir -p esp32c6 && cp esp32c3/libalgobsec.a esp32c6/libalgobsec.a
   ```
   A library update removes `esp32c6/` — recreate it if linking fails.
4. Upload. Deep-sleep/port-busy recovery: hold BOOT while plugging in USB, release after ~2 s.

## Code Architecture (`code/shs_modular/`)

Arduino multi-file sketch: all `.ino` files are concatenated into one translation unit, so
functions/globals are mutually visible and prototypes are auto-generated. `config.h` is the
single include shared by every module.

**Configuration is runtime, not compile-time.** `config.h` holds only the pin map, the
variant, and `DEFAULT_*` factory values — no credentials, so it is committed. Per-device
values live in NVS (`settings.ino`, namespace `shs`, versioned) and are edited from the setup
portal. Workshop credentials go in the gitignored `workshop_secrets.h`, pulled in via
`__has_include` so a build without it still compiles.

**Device identity is derived, never stored:** `deviceId` = `shs-<hash8>` (first 32 bits of
SHA-256 over the full efuse MAC — truncating the MAC instead left every board sharing the OUI
prefix), `writeKey` = HMAC-SHA256(`WORKSHOP_KEY_SALT`, MAC) truncated to 32 hex chars, both
recomputed every boot. This survives a factory reset *and* a full flash erase — which matters because
diy-sensor.org has no write-key recovery and never sweeps an API-key device, so a lost key
would orphan that device ID permanently. Unsalted builds fall back to a random key in NVS.

**Flow** (`shs_modular.ino`): `setup()` → `loadSettings()` → `displayInit()` →
`detectDoubleReset()` → `bme680Init()` → `wifiConnect()` → [`runCommissioningPortal()` if
double-reset or offline] → `mqttConnect()` / `sensorboardConnect()`; `loop()` → `bme680Run()`
+ `wifiLoop()` + `mqttLoop()` + `displayTick()`. BSEC samples every ~3 s in LP mode and fires
`newDataCallback`, which fills a `SensorPacket` and calls `displayUpdate()`, `mqttPublish()`
and `sensorboardPublish()` — the unused backends are compiled to no-ops, so the callback has
no branching.

| File | Responsibility |
|------|----------------|
| `config.h` | Pins, `SHS_VARIANT`, `DEFAULT_*`, `Settings`/`SensorPacket` structs. Committed — no secrets |
| `workshop_secrets.example.h` | Template for `workshop_secrets.h` (gitignored): API key, project, key salt |
| `shs_modular.ino` | Entry point; wires modules together |
| `settings.ino` | NVS persistence, derived identity, the three reset actions |
| `portal.ino` | WiFiManager portal: params per variant, live readings, send test, resets |
| `resetdetect.ino` | Double-reset detection via an NVS flag + a 3 s window |
| `display.ino` | ST7789 UI + `displayPortal()`; stubbed when `USE_DISPLAY 0` |
| `bme680.ino` | BSEC2 init, `newDataCallback`, NVS state persistence, `sensorLatest()` |
| `wifi.ino` | Station connect + backoff reconnect; stubbed when `USE_NETWORK 0` |
| `mqtt.ino` | PubSubClient; discovery / flat / both modes; stubbed when `USE_MQTT 0` |
| `sensorboard.ino` | HTTPS POST to diy-sensor.org; stubbed when `USE_SENSORBOARD 0` |
| `utils.ino` | `isValidFloat()` |

**QR code:** `displayQr()` uses Espressif's encoder from the ESP32 core (`qrcode.h`,
linked by default via `flags/ld_libs`) — no extra library. Installing the Arduino
"QRCode" library (ricmoo) shadows the core header, since both are named `qrcode.h`,
and breaks the build.

**Arduino single-TU gotchas.** All `.ino` files concatenate into one translation unit, so a
`static` global in two files is a redefinition (`Preferences prefs` was). Auto-generated
prototypes are injected into the main sketch after *its* includes, so any signature naming a
library type (`bsecOutputs`, `WiFiManagerParameter`, `esp_qrcode_handle_t`) needs that
header included in `shs_modular.ino` — otherwise it is prototyped against an unknown
type, even when the .ino that uses it includes the header itself.

`SensorPacket` (in `config.h`) carries one reading from `bme680.ino` to `display.ino` and the
backend modules; `sensorLatest()` re-exposes the most recent one to the portal. Status colours (`COLOR_INFO/OK/WARN/ERR`) are defined in `config.h` — not
`display.ino` — so other modules can pass them even when the display is compiled out.

## Hardware notes

- **Pins:** display SPI on GPIO5/6/7/14/15/21/22; BME680 I2C SDA=GPIO3, SCL=GPIO2. **Avoid
  GPIO16/17** (UART0 — bootloader chatter disturbs I2C).
- **BME680 I2C address:** probes `0x76` then `0x77`.
- **Self-heating:** `settings.tempOffsetC` (default 5.0 °C, editable in the portal)
  compensates ESP32 + backlight heat. Final default depends on the production enclosure —
  see TODO.
- **Enclosure blocks RESET (known issue, not yet fixed):** the current housing covers the
  RESET button, so the documented double-reset route back into the portal means sliding the
  board out. The button-free path — and what the docs now give alongside it — is that
  `setup()` opens the portal whenever `wifiConnect()` fails at boot, so powering up away from
  the saved network reaches setup mode too. A network lost *while running* does not reopen it
  (`wifiLoop()` reconnects in the background). Fix belongs in the next case revision.
- **BSEC accuracy 0–3:** IAQ only trustworthy at 3; first calibration takes hours, 4-day window.
  State saved to NVS and restored on boot. The portal's *Clear IAQ calibration* is the only
  action that discards it — deliberately not bundled into the factory reset, which costs
  hours to undo.

## Home Assistant integration

- **MQTT (main):** `settings.mqttMode` picks the shape — `MQTT_MODE_DISCOVERY` (retained
  configs to `<haDiscPrefix>/sensor/<deviceId>/<metric>/config` + JSON to
  `<prefix>/<deviceId>/state`), `MQTT_MODE_FLAT` (one topic per metric), or `MQTT_MODE_BOTH`
  (flat topics with discovery configs pointing at them). Availability via LWT on
  `.../status`. Switching away from a discovery mode publishes empty retained configs to
  clear the orphaned entities. Needs the Mosquitto broker + MQTT integration.
- **diy-sensor.org (workshop):** one POST per reading to `settings.apiUrl`; `X-API-Key`
  header, `write_key` in the body; server assigns timestamps (sending one is an error).
  200 = appended, 201 = claimed the ID; 403 = the ID belongs to another write key.
  **Cadence is in minutes** (`settings.publishIntervalMin`, default 5, via
  `publishIntervalMs()`). The binding server-side limit is not the per-device write rate
  but the **per-credential write budget** — charged in stored values, not requests, and
  shared by every device carrying the same workshop API key. N devices cost
  N x sensors / interval against one bucket.
- **ESPHome (alt):** native API, auto-discovered, no broker.

## Documentation

- `instructions/build_instructions.md` — default student-facing build guide (start here): web flasher → setup portal → dashboard, ~90 min.
- `instructions/build_instructions_extended.md` — Arduino toolchain path: build from source, BSEC blob, MQTT/Home Assistant, ESPHome.
- `web-flasher/README.md` — building/publishing images; the workshop-secrets caveat.
- `instructions/background_information.md` — BME680/BSEC/IAQ theory, MQTT-vs-ESPHome, system path.
- `instructions/quick_reference/quick_reference.md` — printable one-pager.
