# Smart Home Sensor — Quick Reference

<img src="../build_instructions_qr.png" alt="QR code linking to the full build instructions" width="120" align="right">
**Full build instructions:** scan the QR code

**Web flasher** (on a computer, in a browser with Web Serial: Chromium, a recent Firefox, Chrome, Edge, Opera):
`alexanderkutschera.com/smart_home_sensor`

## 1. Wire the BME680

Thread the stripped ends through the pad holes **from the plain back**, bend them flat against the labelled front and solder there — the wires must leave the module **sideways**, in the plane of the board.

| BME680 | VCC | GND | SDA | SCL |
|---|---|---|---|---|
| **Board** | **3V3** (not 5 V) | GND | **GPIO3** | **GPIO2** |

Swapped SDA/SCL is the most common reason a board reports no sensor.

## 2. Flash and test before assembly

With the sensor still outside the enclosure, web flasher → select the workshop image → **Connect** → port `USB JTAG/serial debug unit` → **Install**. Needs a USB-C **data** cable. No port listed? Unplug, hold **BOOT**, plug in, release after ~2 s.

After reboot, confirm the display shows live temperature, humidity, and pressure, with no **BME68x err**. Breathe near the BME680 and make sure VOC or CO₂-equivalent reacts. If it fails, power off and re-check 3V3, GND, SDA → GPIO3, and SCL → GPIO2. Do not put the board in the enclosure until this passes.

## 3. Assemble the enclosure

If you printed the enclosure, place the board and sensor into it only after the test above passes. Keep the BME680 on the upper side, component side facing the lid grille, and put insulation between the sensor and board before closing the lid.

## 4. Set up on the device

First boot opens the Wi-Fi network **`SHS-xxxxxxxx-Setup`** (no password) → browse to
**`192.168.4.1`** → **Configure WiFi** (2.4 GHz only) → **Setup**: give the device a name →
*Live readings & connection test* → **Send a test reading** (green **✓ 201** or **200** =
stored) → **Finish**.

**Back into setup later: press RESET twice quickly.** The enclosure covers that button — slide
the board out, or power up away from the saved Wi-Fi: with no known network it opens setup itself.

## 5. Your readings

`diy-sensor.org/dashboard/device/<device-id>` — the device ID is on the display and on the
setup page. It comes from the chip and never changes. New readings arrive every **5 minutes**;
the display itself refreshes every few seconds.

## 6. Air quality

**IAQ accuracy: 0** stabilizing · **1–2** calibrating (hours) · **3** trusted. A drop back to
1 is normal, BSEC is rebuilding its baseline.
**IAQ:** 0–50 good · 51–100 moderate · 101–150 light · 151–200 moderate · 201–300 heavy ·
300+ severe pollution.
**Temperature** reads high (the board warms the sensor). After 20–30 min, *add* the remaining
error to the offset in the setup page: `new = current + (reported − real)`. It starts at 5 °C,
not 0.
