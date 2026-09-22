# Smart Home Sensor — Quick Reference

<img src="../build_instructions_qr.png" alt="QR code linking to the full build instructions" width="120" align="right">
**Full build instructions:** scan the QR code

**Web flasher** (on a computer, in a browser with Web Serial: Chromium, a recent Firefox, Chrome, Edge, Opera):
`alexanderkutschera.com/smart_home_sensor`

## 1. Wire the BME680

Thread the stripped ends through the pad holes **from the plain back**, bend them flat against the labelled front and solder there — the wires must leave the module **sideways**, in the plane of the board.

<img src="wiring.svg" class="wiring" alt="Six-pin microcontroller connector to BME680: 3V3 to VCC, GND to GND, GPIO3 to SDA, GPIO2 to SCL. Leave 5V and GPIO1 unused." width="760">

Swapped SDA/SCL is the most common reason a board reports no sensor.

## 2. Flash and test before assembly

With the sensor still outside the enclosure, web flasher → select the workshop image → **Connect** → port `USB JTAG/serial debug unit` → **Install**. Needs a USB-C **data** cable. No port listed? Unplug, hold **BOOT**, plug in, release after ~2 s.

After reboot, wait for green **OK** under **BME680 sensor** on the display. If it shows **NOT FOUND**, power off and re-check the wiring. Only assemble the enclosure once the check passes.

## 3. Assemble the enclosure

If you printed the enclosure, place the board and sensor into it only after the test above passes. Keep the BME680 on the upper side, component side facing the lid grille, and put insulation between the sensor and board before closing the lid.

## 4. Set up on the device

First boot opens a Wi-Fi network with a friendly name such as **`SHS-Bouncy-Alpaca`** (no password). Use the exact name shown on the display → browse to
**`192.168.4.1`** → **Configure WiFi** (2.4 GHz only) → **Setup**: give the device a name →
*Live readings & connection test* → **Send a test reading** (green **✓ 201** or **200** =
stored) → **Finish**.

**Back into setup later: press RESET twice quickly.** The enclosure covers that button — slide
the board out, or power up away from the saved Wi-Fi: with no known network it opens setup itself.

## 5. Your readings

**Missed the QR code or don’t know your ID?** You can always go to
**diy-sensor.org/dashboard**, find the name you gave your device during setup, and select it
to open its readings. New readings arrive every **5 minutes**; the display refreshes every few seconds.

## 6. Air quality

**IAQ accuracy: 0** stabilizing · **1–2** calibrating (hours) · **3** trusted. A drop back to
1 is normal, BSEC is rebuilding its baseline.
**IAQ:** 0–50 good · 51–100 moderate · 101–150 light · 151–200 moderate · 201–300 heavy ·
300+ severe pollution.
**Temperature** reads high (the board warms the sensor). After 20–30 min, *add* the remaining
error to the offset in the setup page: `new = current + (reported − real)`. It starts at 5 °C,
not 0.
