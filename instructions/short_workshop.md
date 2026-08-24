# Short Workshop — build a sensor in 90 minutes

Written for the **European Impact Sprint**. You flash a prebuilt firmware from your
browser, wire up the sensor, name your device, and watch your readings appear on a
shared dashboard at [diy-sensor.org](https://diy-sensor.org). No Arduino IDE, no
libraries, no account.

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
2. Leave **European Impact Sprint** selected.
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

A green **✓ 201 — reading stored** means the server accepted your reading and the device
is ready. Press **Finish setup**. (Press the button again and you will see **200** instead:
201 means the device claimed its ID with that first reading, 200 that it added another.
Both mean stored.)

> Your **device ID** is printed on the display and on the setup page. It is derived from
> the chip itself, so it never changes — even if you reflash the board.

---

## 4. Find yourself on the dashboard (5 min)

Go to **`diy-sensor.org/dashboard/device/<your-device-id>`**, or find your device name in
the workshop project your instructor will point you at.

Readings arrive every **5 minutes**, so the first chart takes a little patience — the
display updates every few seconds regardless, which is where to look for an immediate
reaction. You can shorten the interval in the setup page, but on a shared workshop key
every device's readings come out of the same budget, so leave it unless you have a reason.

---

## 5. Now make it read something (the rest of the time)

The gas sensor is the interesting part, and it needs to warm up. Try these while it does:

- **Breathe on the sensor** from a few centimetres away and watch VOC and CO₂-equivalent
  climb, then fall back over a minute or two.
- **Compare temperature** against a real thermometer. Yours will read high — the ESP32
  and the backlight warm the sensor board. The setup page has a **Temperature offset**
  field to correct it. It already ships set to 5 °C, so **add** whatever error is
  left to that number rather than replacing it: if the display still reads 2.5 °C
  high, the offset becomes 7.5, not 2.5.
- **Watch IAQ accuracy.** It starts at 0 and works up to 3. Until it reaches 3, the IAQ
  number is a placeholder — BSEC is still learning what clean air looks like in your room.
  Reaching 3 takes hours, and full convergence takes up to four days of running.

---

## Troubleshooting

| Symptom | What to check |
|---|---|
| Display says **BME68x err** | Wiring: 3V3 not 5V, SDA→GPIO 3, SCL→GPIO 2. Reseat the jumpers and press RESET. |
| Never leaves **Setup mode** | Wrong Wi-Fi password, or a 5 GHz-only network. The ESP32-C6 is 2.4 GHz only. |
| Test says **✗ 403** | The device ID is already claimed with a different write key, which happens if this board previously ran another image. Tell your instructor: the ID needs freeing, or it frees itself 48 h after that device's last reading. |
| Test says **✗ could not reach…** | The device is on Wi-Fi but has no internet. Captive-portal networks (hotel/campus guest Wi-Fi) will not work. |
| Nothing on the dashboard | If you pressed **Send a test reading**, your data is already there: check the device ID in the URL character for character. If you did not, the next scheduled reading is up to 5 minutes away. |
| **IAQ accuracy fell from 3 back to 1** | Normal. BSEC is rebuilding its baseline after a change in the air, a restart, or air that never varies. It climbs back on its own. |
| **I need to change a setting** | Press **RESET twice quickly**. The display tells you when the window is open, then setup mode returns. |

---

## Taking it home

The device keeps publishing wherever it finds a Wi-Fi network it knows. Press RESET
twice to put your home network in.

**The sprint firmware has an end date: 26 August 2026**, the day after the workshop.
On that day the image comes off the flasher page, so you can no longer install it on
another board. The board you flashed today is unaffected and keeps publishing, and you
can still read its write key from the setup page at any time by pressing RESET twice.

Your readings last exactly as long as your device keeps sending them. A device that
stops publishing is removed 48 hours later, together with its history, and its device
ID becomes free for anyone to claim again. The dashboard is public throughout, so
treat nothing on it as private or as a backup.

Your board does not expire with it. Three ways on:

- **Keep using the dashboard** — re-flash with the **diy-sensor.org** image, which
  needs no key and stays on the flasher page. Same device ID, same dashboard. If you
  do it before your sprint device has expired, copy the write key from the sprint
  setup page first and paste it into the new build's *Write key* field, and the device
  carries on uninterrupted.
- **Move it to Home Assistant** — flash the **Home Assistant** image and point it at
  your own broker. The [full instructions](build_instructions.md) walk through it.
- **Point it at your own server** — any diy-sensor.org instance works; set the API URL
  in the setup page.
