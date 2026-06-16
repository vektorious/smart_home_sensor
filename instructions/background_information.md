# Background Information

Useful context for understanding *why* this device is built the way it is.

## Sensor Choice

### Environmental & Air-Quality Sensors

#### Overview

| Sensor | Measures | Interface | Advantages | Limitations |
|--------|----------|-----------|------------|-------------|
| DHT11 / DHT22 | Temperature, humidity | 1-Wire | Cheap, very common in tutorials | Slow (1 reading/s), low accuracy |
| BME280 | Temperature, humidity, pressure | I2C / SPI | All-in-one, accurate, low power | No gas / air-quality sensing |
| **BME680** | **Temp, humidity, pressure, gas (VOC)** | **I2C / SPI** | **Adds a gas sensor → air-quality index via BSEC** | **Gas sensor needs multi-day self-calibration; self-heats** |
| SCD40 / SCD41 | **True** CO₂ (NDIR), temp, humidity | I2C | Measures real CO₂ concentration | More expensive, larger, higher power |

#### Background

Temperature and humidity sensors appear in countless IoT projects; the main trade-offs are
accuracy, speed, and *what* they measure. The BME280 is a popular all-in-one for temperature,
humidity, and pressure. The **BME680** adds a fourth element: a small heated metal-oxide (MOX)
**gas sensor** whose resistance changes in the presence of volatile organic compounds (VOCs) —
the gases given off by people, cooking, cleaning products, furniture, and so on.

That extra channel is what lets the BME680 estimate **air quality**, not just comfort. It does
*not* measure true CO₂ — for that you'd need an NDIR sensor like the SCD40 — but for a learning
project it packs the most interesting physics into one cheap, well-supported chip.

#### Workshop Sensor Choice

We use the **Bosch BME680** with Bosch's **BSEC2** software library. The raw MOX gas resistance
on its own is hard to interpret, so Bosch ships BSEC2: a closed-source algorithm that fuses the
gas, temperature, and humidity signals over time and outputs human-meaningful values. It
communicates over I2C (address `0x76`, sometimes `0x77`).

---

## What the Air-Quality Numbers Mean

BSEC2 turns the raw sensor signals into several **virtual sensors**:

- **IAQ (Indoor Air Quality index)** — a unitless score where lower is cleaner air. Bosch's
  scale runs from 0 (excellent) to 500 (extremely polluted). The firmware colour-codes it:

  | IAQ | Meaning |
  |-----|---------|
  | 0–50 | Good |
  | 51–100 | Moderate |
  | 101–150 | Lightly polluted |
  | 151–200 | Moderately polluted |
  | 201–300 | Heavily polluted |
  | 300+ | Severely polluted |

- **CO₂-equivalent (ppm)** — an *estimate* of CO₂ derived from VOC trends, scaled to look like
  a CO₂ reading (outdoor air ≈ 400 ppm; ~1000 ppm is the common "ventilate" threshold). It is
  **not** a true CO₂ measurement, but it tracks the same thing in occupied indoor spaces
  (people exhale both CO₂ and VOCs).

- **Breath-VOC-equivalent (ppm)** — an estimate of the concentration of exhaled VOCs.

### Why calibration takes days

A MOX gas sensor has no absolute zero — it only senses *changes* in gas resistance, and that
baseline drifts with the sensor and its environment. BSEC continuously learns the baseline from
the cleanest air it has recently seen. It reports an **accuracy** level (0–3); IAQ is only
trustworthy at level 3. Reaching it needs exposure to a *range* of air over time — Bosch's
tuning here uses a **4-day** calibration window. The firmware saves the learned state to the
ESP32's flash (NVS) and restores it on boot, so the device doesn't relearn from scratch every
power-cycle.

### Self-heating and the temperature offset

The ESP32 and the LCD backlight produce heat, which warms the BME680 above true room
temperature. BSEC subtracts a fixed **temperature offset** to compensate (`TEMP_OFFSET_C` in
`config.h`, `temperature_offset` in the ESPHome YAML). To calibrate it: let the device run
20–30 minutes until the reported temperature plateaus, compare it to a real thermometer, and
set the offset to the difference. The humidity reading is derived from temperature, so fixing
the offset improves humidity too. This is also why an enclosure should give the sensor some
airflow and keep it away from the warm side of the board.

---

## How the System Works

### The Firmware

Unlike a battery sensor that sleeps between readings, this device is **always on** — it needs
to keep the gas sensor heated for BSEC to calibrate, and the display is meant to be glanceable.
The loop is simple:

1. **Read** — BSEC samples the BME680 every ~3 seconds (low-power mode) and produces the
   virtual outputs (IAQ, CO₂-eq, VOC, temperature, humidity, pressure).
2. **Display** — the latest values are drawn on the ST7789 screen, colour-coded by air quality.
3. **Publish** — at most every 30 seconds the readings are sent to Home Assistant.

The code lives in `code/shs_modular/`. `shs_modular.ino` ties the modules together; everything
you configure is in `config.h`.

### Getting Data into Home Assistant — Two Paths

A DIY device can talk to Home Assistant several ways. This project demonstrates the two most
common, and you pick one when you choose a firmware.

#### Path A — MQTT auto-discovery (the Arduino build)

**MQTT** is a lightweight publish/subscribe messaging protocol used widely in IoT. A central
**broker** (here, the Mosquitto add-on running inside Home Assistant) relays messages between
devices and HA.

Home Assistant supports **MQTT discovery**: if a device publishes a small *config* message to a
special topic, HA automatically creates the matching entity — no manual YAML in HA. On connect,
our firmware publishes one retained config message per metric to
`homeassistant/sensor/<DEVICE_ID>/<metric>/config`, then sends readings as a single JSON
message to its state topic:

```json
{
  "iaq_accuracy": 3,
  "iaq": 42,
  "co2": 612,
  "voc": 0.83,
  "temperature": 21.4,
  "humidity": 47.2,
  "pressure": 1013.6
}
```

It also uses an MQTT **Last Will and Testament**: if the device drops off the network, the
broker automatically marks it *offline* in HA.

```
[BME680] --I2C--> [ESP32-C6 firmware] --MQTT--> [Mosquitto broker] --> [Home Assistant]
```

This path keeps the full custom firmware (and the colour display), and teaches a protocol you'll
meet everywhere in IoT. It's also broker-portable — the same device works with any MQTT
broker, not just Home Assistant.

#### Path B — ESPHome native API (the ESPHome build)

[ESPHome](https://esphome.io/) is a system that builds ESP firmware from a YAML description.
Instead of MQTT it uses its **native API**, a direct, encrypted connection that Home Assistant
discovers and adopts automatically — no broker required. Our `bme680_bsec2` config exposes the
same metrics in a few lines of YAML.

```
[BME680] --I2C--> [ESP32-C6 / ESPHome] --native API--> [Home Assistant]
```

This is the simplest path and the most idiomatic for Home Assistant, at the cost of writing no
real firmware (so you learn less about what's happening) and only a minimal display.

#### Which to use?

| | MQTT (Arduino) | ESPHome |
|--|----------------|---------|
| Extra infrastructure | Needs an MQTT broker | None |
| Custom display UI | Full colour-coded UI | Minimal |
| What you learn | Firmware + a core IoT protocol | YAML configuration |
| Portability | Any MQTT broker / platform | Home Assistant ecosystem |
| Effort | More setup | Least setup |

The workshop uses the **MQTT/Arduino** build as the main path because it teaches more; ESPHome
is offered as a fast, low-friction alternative.

---

## Similar Projects

### Airrohr / sensor.community
A European citizen-science project for DIY outdoor particulate-matter (PM2.5/PM10) sensors,
originally from Stuttgart (luftdaten.info). Uses an ESP8266 + SDS011 laser dust sensor and
uploads readings to a public map. A good reference for how a grassroots network of DIY sensors
can produce city-scale air-quality data.
https://sensor.community/en/sensors/airrohr/

### ESPHome BME680 via BSEC
The official ESPHome component used by the alternative build — documents every option the
`bme680_bsec2` platform exposes.
https://esphome.io/components/sensor/bme680_bsec2.html

### Bosch BSEC2 / BME68x
Bosch Sensortec's documentation for the gas sensor and the BSEC fusion algorithm — the source
of the IAQ / CO₂-equivalent / VOC outputs.
https://www.bosch-sensortec.com/software-tools/software/bme688-software/
