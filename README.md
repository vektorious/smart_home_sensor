# Smart Home Sensor Starter Kit

![Finished Smart Home Sensor showing live readings on the colour display](instructions/img/device_hero2.jpeg)

A hands-on introduction to building a Wi-Fi-connected indoor air-quality sensor that lives on
your shelf and reports what it measures. You'll learn the basics of microcontroller
programming, I2C sensors, air-quality processing, and how a DIY device shows up as a
first-class entity in a smart-home platform — while building something genuinely useful.

The device reads a Bosch **BME680** environmental sensor, runs Bosch's **BSEC2** algorithm to
derive an air-quality index, shows the live values on a colour display, and publishes them
either to [Home Assistant](https://www.home-assistant.io/) or to the
[diy-sensor.org](https://diy-sensor.org) dashboard — **pick one when you flash the board.**

**Short on time?** The [90-minute path](instructions/short_workshop.md) skips the toolchain
entirely: flash from a browser, name your device, watch it appear on a shared dashboard.



---

## What You'll Build

By the end of the workshop you'll have a mains/USB-powered sensor that:
- Measures **temperature, humidity, and barometric pressure**
- Derives an **Indoor Air Quality (IAQ)** index, a **CO₂-equivalent**, and a **breath-VOC**
  estimate from the BME680's gas sensor via Bosch BSEC2
- Shows all readings live on a **240×240 colour display**, colour-coded by air quality
- Reports every reading to **Home Assistant** (where you can chart it, automate on it, and
  trigger an "open a window" notification when the air gets stuffy) — or to the
  **diy-sensor.org** dashboard, which needs no server of your own

No soldering of a circuit board required — the sensor connects to the board with four jumper
wires.

---

## What's in the Kit

- Waveshare **ESP32-C6-LCD-1.3** microcontroller (ESP32-C6 + 1.3" ST7789 display)
- **BME680** sensor module (temperature, humidity, pressure, gas / air quality)
- 4× jumper wires (or a 4-pin Dupont cable)
- USB-C cable
- _Optional:_ 3D-printed enclosure

---

## Before You Begin

Everyone needs:
- A computer with a USB-C port (or adapter)
- A 2.4 GHz Wi-Fi network (the ESP32-C6 will not see a 5 GHz-only network)

For the **short workshop** that is the whole list — flashing happens in Chrome, Edge or Opera.

For the **Home Assistant** build, additionally:
- A running Home Assistant instance (a Raspberry Pi, mini-PC, or VM is fine)
- The **Mosquitto broker** add-on and the MQTT integration
- The [Arduino IDE](https://www.arduino.cc/en/software), if you want to build from source
  rather than flash a prebuilt image

Everything else — board support, libraries, and configuration — is covered step by step in the
build instructions.

---

## Build Instructions

**Short workshop (~90 min):** [`instructions/short_workshop.md`](instructions/short_workshop.md)
— web flasher, setup portal, shared dashboard. No toolchain.

**Full build:** [`instructions/build_instructions.md`](instructions/build_instructions.md)

That guide covers wiring, flashing, and connecting the device to Home Assistant for both
firmware variants below.

---

## Learning Goals

- Wire and read an I2C sensor
- Understand what **IAQ**, **CO₂-equivalent**, and **VOC** actually mean, and why a gas sensor
  needs a multi-day self-calibration window
- Drive a colour SPI display and design a readable at-a-glance UI
- Connect a DIY device to Home Assistant via **MQTT auto-discovery** and understand how
  ESPHome's native API offers an alternative approach
- See the same readings reach a plain HTTP dashboard, and understand why a device that
  is commissioned over its own access point never needs recompiling

---

## Firmware Variants

One Arduino sketch in [`code/shs_modular/`](code/shs_modular/) builds four images.
All four share the display UI, the BSEC read path and the setup portal — they differ
only in where the readings go.

| Image | Reports to | Needs | Who it's for |
|-------|-----------|-------|--------------|
| **European Impact Sprint** | [diy-sensor.org](https://diy-sensor.org), keyed | nothing but Wi-Fi | The sprint itself — flash, name the device, done. Withdrawn 26 Aug 2026 |
| **diy-sensor.org** | the same dashboard, keyless | nothing but Wi-Fi | Everyone else, and sprint participants afterwards. Stays available |
| **Home Assistant** | MQTT auto-discovery | an MQTT broker | Keeping the device permanently, in your own smart home |
| **Display only** | nothing | nothing | Checking the hardware before dealing with a network |

The two diy-sensor images are the same firmware; the sprint build just carries the
event's credentials so it needs no configuration on the day.

Every per-device value — device name, broker address, API key, publish interval,
temperature offset — lives in the device's flash and is edited from a **setup portal**
the device serves over its own Wi-Fi access point. Nothing needs recompiling per
device, which is what makes the [web flasher](web-flasher/) practical: students install
a prebuilt image from a browser and configure it on the device.

An **ESPHome** alternative lives in [`code/esphome/`](code/esphome/) — native HA API,
no broker, minimal display, and not yet verified on hardware.

---

## Repository Structure

```
code/
  shs_modular/             Firmware (Arduino + BSEC2 + display + backends)
    config.h               Pins, build variant, factory defaults — no credentials
    workshop_secrets.example.h  Template for the workshop API key (copy, gitignored)
    shs_modular.ino        Entry point
    settings.ino           Runtime settings in NVS + derived device identity
    portal.ino             Setup portal: Wi-Fi, settings, live readings, send test
    resetdetect.ino        Double-reset detection (re-opens the portal)
    display.ino            ST7789 UI
    bme680.ino             BME680 read path via Bosch BSEC2
    wifi.ino               Station connect + background reconnect
    mqtt.ino               MQTT: HA auto-discovery / flat topics / both
    sensorboard.ino        HTTPS publishing to diy-sensor.org
    utils.ino              Shared helpers
    bsec_config_33v_3s_4d.h  BSEC tuning blob (3.3 V, 3 s sample rate)
  esphome/
    smart_home_sensor.yaml Alternative ESPHome firmware
  legacy/
    test_wv_display/       Original display+sensor sketch (no networking, reference)

web-flasher/               Browser-based installer (ESP Web Tools) + build.sh

hardware/
  3d-print/                Enclosure parts (Main/thin body, lid, FreeCAD source)

instructions/
  build_instructions.md    Full build guide — start here
  short_workshop.md        90-minute path: web flasher → dashboard
  background_information.md Sensor theory, IAQ explained, MQTT vs ESPHome, system overview
  quick_reference/         One-page summary

img/                       Photos and diagrams (TODO)
```

---

## Background Reading

[`instructions/background_information.md`](instructions/background_information.md) covers:

- **Sensor selection** — why the BME680 + BSEC over simpler temperature/humidity sensors, and
  what IAQ / CO₂-equivalent / VOC numbers actually represent
- **Self-heating** — why the reported temperature needs an offset, and how to calibrate it
- **How the system works** — the full path from the BME680 through the firmware to a Home
  Assistant entity, comparing MQTT auto-discovery against the ESPHome native API
- **Similar projects** — related open-source air-quality builds for further inspiration

---

## Licenses

| Component | License |
|-----------|---------|
| Software (`code/`) | [MIT](LICENSE.code) |
| Hardware (`hardware/`) | [CERN-OHL-W 2.0](LICENSE.hardware) |
| Documentation (`instructions/`, `README.md`, `img/`) | [CC-BY 4.0](LICENSE.docs) |
