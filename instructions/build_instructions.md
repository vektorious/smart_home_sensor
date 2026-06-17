# Build Instructions — Smart Home Sensor

This guide covers the full build: wiring the **BME680** to the **Waveshare ESP32-C6-LCD-1.3**,
flashing the firmware, and connecting the device to **Home Assistant**.

There is **no custom circuit board** — the sensor connects with four jumper wires. The device
is **always on** (USB powered): the live display and the BME680's multi-day air-quality
self-calibration mean it never goes to sleep.

Two firmware variants are covered:

- **Main build — Arduino + MQTT** (§4–6): keeps the colour display UI and teaches the full
  firmware. *Recommended for the workshop.*
- **Alternative — ESPHome** (§7): the quickest path, fewer steps, minimal display.

Do the wiring (§1–3) first; then pick **one** firmware path.

---

## 1. Collect All Parts

**Electronics**
- Waveshare ESP32-C6-LCD-1.3 microcontroller (ESP32-C6 + 1.3" ST7789 display)
- BME680 sensor module
- 4× female–female jumper wires (or a 4-pin Dupont cable)
- USB-C cable

**Optional**
- 3D-printed enclosure (see [`hardware/3d-print/`](../hardware/3d-print/))
- Styrofoam packing insert from the ESP32 box — keep it, it's used as a heat barrier in §3

**Tools**
- A computer with a USB-C port
- _(Only if your BME680 ships without a soldered header:)_ soldering iron + solder

---

## 2. Wire the BME680 to the Board

The BME680 talks to the board over **I2C** — two data lines plus power and ground.

| BME680 pin | Board pin | Notes |
|------------|-----------|-------|
| VCC / VIN  | 3V3       | **3.3 V only** — do not use 5 V |
| GND        | GND       | |
| SDA        | GPIO3     | I2C data |
| SCL        | GPIO2     | I2C clock |

> ⚠️ **Avoid GPIO16 / GPIO17.** Those are UART0 TX/RX on the ESP32-C6 and emit bootloader
> chatter at startup that can disturb I2C devices. The firmware uses GPIO2/GPIO3 — stick to
> them unless you also change the pins in `config.h`.

If your BME680 module came with a loose pin header, solder it on first so the jumper wires
have something to grip.

> 📷 _Wiring photo / diagram — TODO (see [`TODO.md`](../TODO.md))._

---

## 3. Assemble the Enclosure (optional)

If you printed an enclosure, follow these steps. The enclosure isolates the BME680 from heat
radiated by the LCD backlight and ESP32, which would otherwise skew temperature and humidity
readings (see [`background_information.md`](background_information.md) for details).

> **Before you start:** keep the small piece of **styrofoam** the ESP32 board ships with.
> It goes between the board and the BME680 as a heat barrier.

1. **Push the ESP32 board into the main body** — display facing the open front.
2. **Place the styrofoam piece** on top of the board, between the board and the BME680.
   The foam reduces heat transfer from the LCD/chip to the sensor.
3. **Position the BME680** on top of the styrofoam so it sits just under the lid opening.
4. **Close with the lid** — the BME680 should end up directly beneath the lid, exposed to
   room air rather than heat from the board.

Enclosure files and FreeCAD source: [`hardware/3d-print/`](../hardware/3d-print/).

---

## 4. Main Build — Flash the Arduino Firmware

### 4.1 Install Arduino IDE and the ESP32 Board Package

1. Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Open **File ▸ Preferences** and add this URL to *Additional Boards Manager URLs*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Open **Tools ▸ Board ▸ Boards Manager**, search for `esp32`, and install
   **esp32 by Espressif Systems**.
4. Select **Tools ▸ Board ▸ ESP32 Arduino ▸ ESP32C6 Dev Module**.

### 4.2 Install Required Libraries

In **Tools ▸ Manage Libraries**, search for and install:

| Library | Author |
|---------|--------|
| GFX Library for Arduino | Moon On Our Nation |
| BSEC2 Software Library | Bosch Sensortec |
| BME68x Sensor library | Bosch Sensortec |
| WiFiManager | tzapu |
| PubSubClient | Nick O'Leary |

### 4.3 Add the ESP32-C6 BSEC Blob (important!)

Bosch's `BSEC2` library ships precompiled algorithm blobs for several chips but, as of
v1.10.x, **not for the ESP32-C6**. The C6 is soft-float RISC-V (`rv32imac`) and is
ABI-compatible with the ESP32-C3 blob, so you create the C6 folder as a copy of the C3 one.

In your Arduino libraries folder (usually `~/Arduino/libraries/BSEC2_Software_Library/src/`):

```bash
cd ~/Arduino/libraries/BSEC2_Software_Library/src
mkdir -p esp32c6
cp esp32c3/libalgobsec.a esp32c6/libalgobsec.a
```

> If you ever get a **linker error** about `libalgobsec` after updating the library, the
> update removed the `esp32c6/` folder — just re-run the copy above.

### 4.4 Configure the Firmware

1. In the `code/shs_modular/` folder, copy `config.example.h` to `config.h`
   (the file is gitignored so your credentials stay out of the repository).
2. Open `code/shs_modular/shs_modular.ino` in the Arduino IDE (it opens all the `.ino`
   modules as tabs).
3. Open the **`config.h`** tab and set, near the top:

```c
#define DEVICE_NAME  "Smart Home Sensor"
#define DEVICE_ID    "shs-livingroom"     // unique per device: a-z 0-9 '-'

#define MQTT_HOST    "192.168.1.10"        // your broker (e.g. the HA host)
#define MQTT_PORT    1883
#define MQTT_USER    "mqtt-user"
#define MQTT_PASS    "mqtt-password"
```

4. Leave `USE_DISPLAY` and `USE_MQTT` at `1`. (Set `USE_MQTT 0` if you only want the display.)

### 4.5 Upload

1. Connect the board via USB-C.
2. Select the port: **Tools ▸ Port**.
3. Click **Upload**.

> **Upload fails / port busy?** Put the board in download mode:
> 1. Unplug the board.
> 2. Hold the **BOOT** button.
> 3. Plug back in while holding BOOT.
> 4. Release after ~2 seconds, then retry Upload.

### 4.6 Connect to Wi-Fi

The firmware uses WiFiManager. On first boot (or when no saved network is found):

1. The device starts a temporary access point named **`Smart Home Sensor-Setup`**.
2. Connect to it from your phone or laptop — no password required.
3. A configuration page opens automatically. Enter your Wi-Fi credentials.
4. The device connects, saves the credentials, and starts measuring.

From the next boot onward it reconnects automatically.

---

## 5. Main Build — Connect to Home Assistant (MQTT)

The Arduino firmware uses **MQTT discovery**: it announces its own entities to Home Assistant,
so there is nothing to configure by hand in HA.

1. In Home Assistant, install the **Mosquitto broker** add-on
   (*Settings ▸ Add-ons ▸ Add-on Store*) and start it.
2. Add the **MQTT integration** if it isn't already set up
   (*Settings ▸ Devices & Services ▸ Add Integration ▸ MQTT*).
3. Create an MQTT user for the device (e.g. in the Mosquitto add-on config or a HA user) and
   put those credentials in `config.h` (§4.4), then re-upload.
4. Power the device. Within a few seconds it connects and a new device named
   **Smart Home Sensor** appears under *Settings ▸ Devices & Services ▸ MQTT*, with entities:
   IAQ, IAQ Accuracy, CO₂ equivalent, Breath VOC equivalent, Temperature, Humidity, Pressure.

The device publishes readings every 30 s (`MQTT_PUBLISH_MS` in `config.h`).

> **Don't see it?** Open the **Serial Monitor** at **115200 baud** and watch for
> `MQTT connecting... connected` and `MQTT discovery configs published`. If it says
> `failed (rc=...)`, the broker host/credentials in `config.h` are wrong, or the MQTT user
> lacks publish rights.

---

## 6. Understand the Air-Quality Readings

The BME680's gas sensor needs to **self-calibrate** before IAQ is trustworthy. The footer on
the display (and the *IAQ Accuracy* entity in HA) reports progress:

| Accuracy | Meaning |
|----------|---------|
| 0 | Stabilizing — gas heater warming up; ignore IAQ |
| 1–2 | Calibrating — collecting its baseline (can take hours) |
| 3 | Calibrated — IAQ is now reliable |

Reaching accuracy 3 the first time can take a few hours of varied air; full calibration uses a
4-day window. The firmware saves the calibrated state to flash and restores it on boot, so it
doesn't start from scratch every time. See
[`background_information.md`](background_information.md) for what IAQ, CO₂-equivalent, and VOC
actually mean.

---

## 7. Alternative — ESPHome

> ⚠️ **This path has not been tested on real hardware.** The YAML is provided as a starting
> point; pin assignments, `bme680_bsec2` platform support, and the ST7789 display block have
> not been verified against a physical board. Use it at your own risk and expect some trial
> and error.

Prefer ESPHome? It needs no Arduino IDE and no MQTT broker — Home Assistant discovers the
device over ESPHome's native API.

1. In Home Assistant, install the **ESPHome** add-on and open its dashboard.
2. In the add-on's config directory, add `code/esphome/smart_home_sensor.yaml` (copy its
   contents into a new device, or drop the file in).
3. Create a `secrets.yaml` with `wifi_ssid`, `wifi_password`, and `ap_password`.
4. **Install** to the board (USB the first time, then over-the-air after).
5. Home Assistant auto-discovers the device — accept it under
   *Settings ▸ Devices & Services*.

> **Note:** the ESPHome variant exposes the same sensor entities but only a **minimal**
> display layout — the rich colour-coded UI exists only in the Arduino build. See the comments
> at the top of the YAML.

---

## 8. Troubleshooting

Open the **Serial Monitor** at **115200 baud** to see what the device is doing.

**Sensor not found (`BME68x err ...` on the display):**
- Re-check the four wires (§2). Swapped SDA/SCL is the usual cause.
- Some modules sit at I2C address `0x77` instead of `0x76`; the firmware probes both, so a
  wrong address is rarely the problem — a wiring fault is more likely.

**I2C scan sketch.** To confirm the board sees the sensor at all, upload this minimal sketch
and watch the Serial Monitor:

```cpp
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(3, 2);   // SDA=GPIO3, SCL=GPIO2
  Serial.println("Scanning I2C...");
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf("  device at 0x%02X\n", a);
  }
}
void loop() {}
```

Expect a device at `0x76` (or `0x77`). Nothing found ⇒ a wiring/power problem.

**IAQ stuck at "Stabilizing" / accuracy 0:** normal for the first minutes-to-hours. The gas
sensor calibrates against changing air — open a window, breathe near it, then ventilate.

**MQTT won't connect:** check `MQTT_HOST`/`MQTT_USER`/`MQTT_PASS` in `config.h`, that the
Mosquitto add-on is running, and that the MQTT user may publish.

**Linker error mentioning `libalgobsec`:** the BSEC2 ESP32-C6 blob is missing — redo §4.3.

**Upload fails:** use the BOOT-button download-mode recovery in §4.5.

---

## What's Next?

Your sensor now streams temperature, humidity, pressure, and air-quality data into Home
Assistant. Try building an automation — for example, send a phone notification when *CO₂
equivalent* rises above 1000 ppm ("time to open a window"), or chart IAQ over a week to see
how cooking and ventilation affect your air.

For background on how the sensors work and why the design choices were made, see
[`background_information.md`](background_information.md).
