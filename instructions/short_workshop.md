# Short Workshop — build a sensor in 90 minutes

This is the fast path. You flash a prebuilt firmware from your browser, wire up the
sensor, name your device, and watch your readings appear on a shared dashboard at
[diy-sensor.org](https://diy-sensor.org). No Arduino IDE, no libraries, no account.

If you want to keep the device on your own smart home afterwards, the
[full build instructions](build_instructions.md) cover the Home Assistant path — the
same hardware, a different image, and you can reflash any time.

**What you need:** the board, the BME680, four jumper wires, a USB-C **data** cable, and
a laptop running **Chrome, Edge, or Opera** (Web Serial does not exist in Firefox or
Safari, or on iPads).

---

## 1. Wire the sensor (10 min)

Four wires between the BME680 breakout and the board:

| BME680 pin | Board pin | Wire |
|---|---|---|
| VCC (or VIN/3V3) | **3V3** | red |
| GND | **GND** | black |
| SDA | **GPIO 3** | usually blue |
| SCL | **GPIO 2** | usually yellow |

Double-check **3V3, not 5V** before plugging in the USB cable, and check SDA and SCL are
not swapped — that is the single most common reason a board reports no sensor.

> **Why not GPIO 16/17?** Those are the serial console pins. The bootloader chatters on
> them at every boot, which is enough to disturb an I2C device sharing the line.

---

## 2. Flash the firmware (10 min)

1. Open the [web flasher](https://vektorious.github.io/smart_home_sensor/).
2. Leave **Short workshop — diy-sensor.org** selected.
3. Plug the board into your laptop with a USB-C **data** cable.
4. Click **Connect**, choose the port named `USB JTAG/serial debug unit`, and click
   **Install**. It takes about a minute.

**Nothing appears in the port list?** Start the board in flashing mode: unplug it, hold
**BOOT**, plug the cable back in while still holding, release BOOT after ~2 seconds. If
it still doesn't appear, your cable is probably charge-only — try another.

---

## 3. Set it up (15 min)

When flashing finishes, the board reboots and the display shows **Setup mode** with a
Wi-Fi network name and an address.

1. On your laptop or phone, join the Wi-Fi network named **`SHS-xxxxxxxx-Setup`**. It has
   no password.
2. A setup page should open by itself. If not, browse to **`192.168.4.1`**.
3. Tap **Configure WiFi**, pick the workshop network, enter the password, and save.
4. Go back to the setup page and open **Setup** to give your device a **name** — this is
   what everyone will see on the dashboard, so make it recognisable. Save.
5. Open **Live readings & connection test** and press **Send a test reading**.

A green **✓ 201 — reading stored** means you are done: 201 is the server telling you your
device just claimed its ID. Press **Finish setup**.

> Your **device ID** is printed on the display and on the setup page. It is derived from
> the chip itself, so it never changes — even if you reflash the board.

---

## 4. Find yourself on the dashboard (5 min)

Go to **`diy-sensor.org/dashboard/device/<your-device-id>`**, or find your device name in
the workshop project your instructor will point you at.

Readings arrive every 60 seconds. Charts need a few points before they look like
anything, so give it a couple of minutes.

---

## 5. Now make it read something (the rest of the time)

The gas sensor is the interesting part, and it needs to warm up. Try these while it does:

- **Breathe on the sensor** from a few centimetres away and watch VOC and CO₂-equivalent
  climb, then fall back over a minute or two.
- **Compare temperature** against a real thermometer. Yours will read high — the ESP32
  and the backlight warm the sensor board. The setup page has a **Temperature offset**
  field to correct it: run the device 20–30 minutes, then set the offset to
  (displayed − actual).
- **Watch IAQ accuracy.** It starts at 0 and works up to 3. Until it reaches 3, the IAQ
  number is a placeholder — BSEC is still learning what clean air looks like in your room.
  Reaching 3 takes hours, and full convergence takes up to four days of running.

---

## Troubleshooting

| Symptom | What to check |
|---|---|
| Display says **BME68x err** | Wiring: 3V3 not 5V, SDA→GPIO 3, SCL→GPIO 2. Reseat the jumpers and press RESET. |
| Never leaves **Setup mode** | Wrong Wi-Fi password, or a 5 GHz-only network. The ESP32-C6 is 2.4 GHz only. |
| Test says **✗ 403** | The device ID belongs to a different write key — tell your instructor; the ID needs freeing. |
| Test says **✗ could not reach…** | The device is on Wi-Fi but has no internet. Captive-portal networks (hotel/campus guest Wi-Fi) will not work. |
| Nothing on the dashboard | Check the device ID in the URL character for character. Readings take up to a minute. |
| **I need to change a setting** | Press **RESET twice quickly**. The display tells you when the window is open, then setup mode returns. |

---

## Taking it home

The device keeps publishing wherever it finds the workshop Wi-Fi. Two things to know:

- **The dashboard is public and temporary.** Anyone can see it, and the workshop API key
  is withdrawn after the event. Your data is not private and is not permanent.
- **To keep it for good,** reflash the **Home Assistant** image from the same flasher page
  and point it at your own broker, or set your own API URL in the setup page. The
  [full instructions](build_instructions.md) walk through it.
