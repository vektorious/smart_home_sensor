# Build Instructions — Smart Home Sensor

This is the standard build. You solder four wires to the BME680, plug it onto the board, flash a prebuilt firmware straight from your browser, and set the device up from a page it serves itself. No Arduino IDE, no libraries and no account needed.

**What you need:**

The ESP board, the BME680, four Dupont wires, a USB-C data cable, a soldering iron, and a laptop running a browser that supports Web Serial: Chromium, Chrome, Edge, or Opera. Web Serial does not exist in Firefox or Safari, or on iPads.

---

## 1. Collect all parts

**Electronics**
- Waveshare ESP32-C6-LCD-1.3 microcontroller (ESP32-C6 + 1.3" ST7789 display)
- BME680 sensor module
- 4× Dupont wires (single wires or a 4-pin cable, either works)
- USB-C data cable (a charge-only cable cannot flash the ESP, and they look identical)

**Optional**
- 3D-printed enclosure, see [`hardware/3d-print/`](../hardware/3d-print/)

**Tools**
- A computer with a USB-C port
- Soldering iron and solder

---

## 2. Wire the sensor (15 min)

The BME680 talks to the board over **I2C**: two data lines plus power and ground.

![Four Dupont wires and the BME680 module before assembly](img/jumper_cable_01.jpeg)

| BME680 pin | Board pin |
| ---------- | --------- |
| VCC        | 3V3       |
| GND        | GND       |
| SDA        | GPIO 3    |
| SCL        | GPIO 2    |

Solder the four wires to the BME680's pads **from the back of the board**, the plain side without the components, so the wires leave the board behind it and stay clear of the sensor. Then plug the other ends onto the board's pins. Only the sensor side is soldered, so the sensor-to-board link stays removable and the board can be reused for something else later.

![Dupont wires soldered onto the BME680 pin header](img/jumper_soldered_02.jpeg)

Double-check 3V3, not 5V before plugging in the USB cable, and check SDA and SCL are not swapped. That is the single most common reason a board reports no sensor.

![BME680 connected to the Waveshare board](img/sensor_attached_03.jpeg)

> ⚠️ **The wires go the other way round from what these two photos show.** Solder them from the **back** of the BME680, the plain side without the components, so they leave the board behind it. Soldered on the front as pictured, they cross the component face and get in the way of the sensor and of seating the board in the enclosure. The photos are being redone.

---

## 3. Assemble the enclosure (optional, 10 min)

If you printed the enclosure, assemble it now. It keeps the display visible and holds the BME680 above the board, away from the heat the ESP32 and the backlight give off, which would otherwise push the temperature and humidity readings off (see [`background_information.md`](background_information.md)).

![Exploded assembly animation](../hardware/3d-print/SmartHomeCube-assembly.gif)

Assemble from bottom to top:

1. **Main housing**: the outer body.
2. **ESP32-C6-LCD-1.3 board**: display facing the open front.
3. **Lower inlay** *(optional)*: separates the board from the sensor above it.
4. **Upper inlay** *(optional)*: holds the BME680 in place under the lid. See *Seating the sensor in the upper inlay* at the end of this section.
5. **Lid**: snaps on, but does not drop straight down. See *Closing the lid* below.

The two inlays are optional. In any case, put something insulating between the board and the sensor: the small piece of styrofoam the ESP32 board ships with does the job, so keep it when you unpack the board.

![ESP32 board seated in the enclosure body with the BME680 ready to fold in](img/esp_in_housing_04.jpeg)

![BME680 resting on the heat-shield packing material inside the enclosure](img/sensor_heat_shield_05.jpeg)

![Lid held at a slight angle, with its long tab about to slide into the rails in the main body](img/housing_lid_06.jpeg)

**Closing the lid.** The lid does not press on from straight above. One edge carries a long tab, the opposite edge a short one, and they go in one after the other. Hold the lid at a slight angle, as in the photo, and slide the long tab into the two rails inside the main body. With that edge held in the rails, lower the other edge until the short tab snaps into its notch and the lid sits flush. To open it again, release the short tab first and then draw the long tab back out of the rails, rather than prying the lid straight up.

Once closed, the BME680 sits directly beneath the lid grille, exposed to room air rather than to heat from the board.

![Completed enclosure with lid closed](img/housing_lid_closed_07.jpeg)

**Seating the sensor in the upper inlay.**

The BME680 drops into the recess in the upper inlay with its component side facing up, towards the lid grille, so the sensor breathes room air instead of the air trapped over the board.

![BME680 seated in the upper inlay, component side facing the lid and the wires led out through the side opening](img/sensor_inlay_placement_09.jpg)

*(This inlay was printed in a different colour than the rest of the enclosure above.)*

Print files and FreeCAD source: [`hardware/3d-print/`](../hardware/3d-print/).

---

## 4. Flash the firmware (10 min)

1. Open the [web flasher](https://alexanderkutschera.com/smart_home_sensor/).
2. Pick an image. In a workshop, take the one your instructor points you at: it usually carries the event's dashboard settings already, so there is less to fill in later. Otherwise pick **diy-sensor.org** to publish to the public dashboard, or **Home Assistant (MQTT)** if you already run Home Assistant with an MQTT broker.
3. Plug the board into your laptop with a USB-C data cable.
4. Click Connect, choose the port named `USB JTAG/serial debug unit`, and click Install. It takes about a minute.

In case nothing appears in the port list: Start the board in flashing mode: unplug it, hold BOOT, plug the cable back in while still holding, release BOOT after ~2 seconds. If it still doesn't appear, your cable is probably charge-only. Try another.

---

## 5. Set it up (15 min)

When flashing finishes, the board reboots and the display shows **Setup mode** with a Wi-Fi network name and an address.

1. On your laptop or phone, join the Wi-Fi network named **`SHS-xxxxxxxx-Setup`**. It has no password.
2. A setup page should open by itself. If not, browse to **`192.168.4.1`**.
3. Tap **Configure WiFi**, pick the network, enter the password, and save. It has to be a **2.4 GHz** network: the ESP32-C6 will not see a 5 GHz-only one.
4. Go back to the setup page and open **Setup** to give your device a **name**. On a shared dashboard this is what everyone else sees, so make it recognisable. Save.
5. Open **Live readings & connection test** and press **Send a test reading**.

On a diy-sensor.org image, a green **✓ 201 — reading stored** means the server accepted your reading and the device is ready. Press the button again and you will see **200** instead: 201 means the device claimed its ID with that first reading, 200 that it added another. Both mean stored.

On a Home Assistant image, the same page instead takes the broker host, port, and credentials, and the test reading confirms the broker accepts them. Full detail on the MQTT side is in [`build_instructions_extended.md`](build_instructions_extended.md).

Press **Finish setup** when the test passes. The portal also closes on its own after 10 minutes.

> Your **device ID** is printed on the display and on the setup page. It is derived from the chip itself, so it never changes, even if you reflash the board.

> **To change a setting later**, press **RESET twice in quick succession**. The display tells you when the window is open, then setup mode returns. This is how you move the device to another network, correct the temperature offset, or point it somewhere else, without reflashing.

---

## 6. Find your readings (5 min)

**diy-sensor.org images:** go to `diy-sensor.org/dashboard/device/<your-device-id>`, or find your device name in the project your instructor points you at.

**Home Assistant images:** the device appears by itself under *Settings ▸ Devices & Services ▸ MQTT*, with entities for IAQ, IAQ Accuracy, CO₂ equivalent, Breath VOC equivalent, Temperature, Humidity, and Pressure.

Readings are sent every **5 minutes** by default, so the first chart takes a little patience. The display updates every few seconds regardless, which is where to look for an immediate reaction. You can shorten the interval in the setup page, but on a shared workshop key every device's readings come out of the same budget, so leave it unless you have a reason.

---

## 7. Now make it read something

The gas sensor is the interesting part, and it needs to warm up. Try these while it does:

- **Breathe on the sensor** from a few centimetres away and watch VOC and CO₂-equivalent climb, then fall back over a minute or two.
- **Compare temperature** against a real thermometer. Yours will read high: the ESP32 and the backlight warm the sensor board. The setup page has a **Temperature offset** field to correct it. It already defaults to 5 °C, so add whatever error is left to that number rather than replacing it. If the display still reads 2.5 °C high, the offset becomes 7.5, not 2.5.
- **Watch IAQ accuracy.** It starts at 0 and works up to 3. Until it reaches 3, the IAQ number is a placeholder: BSEC is still learning what clean air looks like in your room. Reaching 3 takes hours, and full convergence takes up to four days of running.

![Finished device showing live sensor readings, IAQ still stabilizing after first boot](img/final_08.jpeg)

---

## Troubleshooting

| Symptom | What to check |
|---|---|
| Display says **BME68x err** | Wiring: 3V3 not 5V, SDA→GPIO 3, SCL→GPIO 2. Reseat the connectors and press RESET. |
| Never leaves **Setup mode** | Wrong Wi-Fi password, or a 5 GHz-only network. The ESP32-C6 is 2.4 GHz only. |
| Test says **✗ 403** | The device ID is already claimed with a different write key, which happens if this board previously ran another image. Tell your instructor: the ID needs freeing, or it frees itself 48 h after that device's last reading. |
| Test says **✗ could not reach…** | The device is on Wi-Fi but has no internet. Captive-portal networks (hotel/campus guest Wi-Fi) will not work. |
| Nothing on the dashboard | If you pressed **Send a test reading**, your data is already there: check the device ID in the URL character for character. If you did not, the next scheduled reading is up to 5 minutes away. |
| **IAQ accuracy fell from 3 back to 1** | Normal. BSEC is rebuilding its baseline after a change in the air, a restart, or air that never varies. It climbs back on its own. |
| **I need to change a setting** | Press **RESET twice quickly**. The display tells you when the window is open, then setup mode returns. |

The setup portal also offers three separate resets: *Forget Wi-Fi network*, *Clear IAQ calibration*, and *Factory reset* (settings + Wi-Fi). None of them changes the device ID, and only the middle one discards the air-quality calibration, which takes hours to rebuild, so it is deliberately not bundled into the others.

---

## Taking it further

The device keeps publishing wherever it finds a Wi-Fi network it knows. Press RESET twice to put your home network in.

If a workshop image was used, note that it is tied to that event and comes off the flasher page afterwards. The board you flashed is unaffected and keeps publishing, and you can still read its write key from the setup page at any time by pressing RESET twice.

On diy-sensor.org, your readings last exactly as long as your device keeps sending them. A device that stops publishing is removed 48 hours later, together with its history, and its device ID becomes free for anyone to claim again. The dashboard is public throughout, so treat nothing on it as private or as a backup.

Three ways on from here:

- **Keep using the dashboard**: reflash with the **diy-sensor.org** image, which needs no key and stays on the flasher page. Same device ID, same dashboard. If you do it before a workshop device has expired, copy the write key from the old setup page first and paste it into the new build's *Write key* field, and the device carries on uninterrupted.
- **Move it to Home Assistant**: flash the **Home Assistant** image and point it at your own broker. [`build_instructions_extended.md`](build_instructions_extended.md) walks through the broker side.
- **Build the firmware yourself**: [`build_instructions_extended.md`](build_instructions_extended.md) sets up the Arduino toolchain so you can change the code, the display, or the pins.

For background on how the sensors work and why the design choices were made, see [`background_information.md`](background_information.md).
