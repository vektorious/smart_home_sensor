# Build Instructions — Smart Home Sensor

This is the standard build. You solder four wires to the BME680, plug it onto the board, flash a prebuilt firmware straight from your browser, and set the device up from a page it serves itself. No Arduino IDE, no libraries and no account needed.

**What you need:**

The ESP board, the BME680, four Dupont wires, a USB-C data cable, a soldering iron, and a laptop running a browser that supports Web Serial: Chromium, a recent Firefox, Chrome, Edge, or Opera. Older Firefox versions do not have it, and neither does Safari or anything on an iPad.

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

![Four Dupont wires and the BME680 module before assembly](img/wires_bme_01.jpg)

Cut one end off each Dupont wire and strip about 3 mm of insulation. Keep the female socket on the other end — that is the end that plugs onto the board's header pins.

![The four wires cut and stripped, ready to solder](img/wires_stripped_02.jpg)

| BME680 pin | Board pin |
| ---------- | --------- |
| VCC        | 3V3       |
| GND        | GND       |
| SDA        | GPIO 3    |
| SCL        | GPIO 2    |

Push each stripped end through its hole in the BME680's pad row **from the back**, the plain side without the components, and bend it flat against the labelled front so the wire runs **off the side edge of the module, in the plane of the board**. Soldering them this way keeps the wires out of the way of both the sensor and the enclosure — sticking straight up off either face, they fight the upper inlay when you seat the module later.

![Stripped wire ends pushed through the pad holes and bent flat, before soldering](img/wires_through_pads_03.jpg)

Solder each joint on that labelled side, then snip off anything sticking out past the pad.

![The four wires soldered to the BME680 pads, wires leaving flat to the side](img/wires_soldered_04.jpg)

![Side view of the soldered joints, showing the wires staying in the plane of the module](img/wires_soldered_side_05.jpg)

Double-check 3V3, not 5V before plugging in the USB cable, and check SDA and SCL are not swapped. That is the single most common reason a board reports no sensor.

Then push the four sockets onto the matching pins on the board. Only the sensor side is soldered, so the sensor-to-board link stays removable and the board can be reused for something else later.

![BME680 connected to the Waveshare board, which is resting on the open enclosure](img/board_in_housing_06.jpg)

---

## 3. Assemble the enclosure (optional, 10 min)

If you printed the enclosure, assemble it now. It keeps the display visible and holds the BME680 above the board, away from the heat the ESP32 and the backlight give off, which would otherwise push the temperature and humidity readings off (see [`background_information.md`](background_information.md)).

![Exploded assembly animation](../hardware/3d-print/SmartHomeCube-assembly.gif)

Assemble from bottom to top:

1. **Main housing**: the outer body.
2. **ESP32-C6-LCD-1.3 board**: display facing the open front.
3. **Lower inlay** *(optional)*: separates the board from the sensor above it.
4. **Upper inlay** *(optional)*: holds the BME680 under the lid.
5. **Lid**: snaps on, but does not drop straight down. See *Closing the lid* below.

The two inlays are optional. In any case, put something insulating between the board and the sensor: the small piece of styrofoam the ESP32 board ships with does the job, so keep it when you unpack the board.

**Board and lower inlay.** Drop the board into the main housing with the display facing the open front (the bottom), and feed the sensor wires out past it. Lay the lower inlay on top of the board, then press board and inlay down together until they sit at the bottom of the housing. Leave the BME680 itself outside for now.

![Lower inlay laid on top of the board, both still standing proud of the housing](img/lower_inlay_placed_07.jpg)

![Board and lower inlay pressed further in, the sensor still hanging outside](img/lower_inlay_pressed_08.jpg)

![Top view with board and lower inlay seated at the bottom, sensor still out](img/board_seated_top_09.jpg)

**Sensor and upper inlay.** The BME680 drops into the recess in the upper inlay with its **component side facing up**, towards the lid grille, so the sensor breathes room air instead of the air trapped over the board. The wires leave through the side opening.

![BME680 seated in the upper inlay, component side towards the lid, wires led out the side](img/sensor_in_upper_inlay_10.jpg)

Lay the styrofoam over the back of the sensor. It fills the space between sensor and board and blocks the heat rising off the ESP32.

![Styrofoam laid into the upper inlay behind the sensor as a heat shield](img/foam_in_upper_inlay_11.jpg)

Now lower the whole upper inlay into the housing, tucking the slack wire in as you go, and press it down until it sits flush.

![Upper inlay with sensor and foam going into the main housing](img/upper_inlay_into_housing_12.jpg)

![Upper inlay seated, sensor facing up ready for the lid](img/upper_inlay_seated_13.jpg)

**Closing the lid.** The lid does not press on from straight above. One edge carries a long tab, the opposite edge a short one, and they go in one after the other. Hold the lid at a slight angle, as in the photo, and slide the long tab into the two rails inside the main body. With that edge held in the rails, lower the other edge until the short tab snaps into its notch and the lid sits flush. To open it again, release the short tab first and then draw the long tab back out of the rails, rather than prying the lid straight up.

![Lid held at an angle, its long tab going into the rails in the main body first](img/lid_angled_14.jpeg)

![Lid closed flush, the BME680 visible through the grille](img/lid_closed_15.jpeg)

Once closed, the BME680 sits directly beneath the lid grille, exposed to room air rather than to heat from the board.

*(Some parts in these photos were printed in a different colour than the rest of the enclosure.)*

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
3. Tap **Configure WiFi**, pick the network, enter the password, and save. Every page here is served by the sensor itself, so give it a few seconds — saving takes longer still, because the board reconnects while you wait. Tapping a second time only queues a second request. Each subpage has a **← Back to setup** button at the bottom. It has to be a **2.4 GHz** network: the ESP32-C6 will not see a 5 GHz-only one.
4. Go back to the setup page and open **Setup** to give your device a **name**. On a shared dashboard this is what everyone else sees, so make it recognisable. Save.
5. Open **Live readings & connection test** and press **Send a test reading**.

On a diy-sensor.org image, a green **✓ 201 — reading stored** means the server accepted your reading and the device is ready. Press the button again and you will see **200** instead: 201 means the device claimed its ID with that first reading, 200 that it added another. Both mean stored.

On a Home Assistant image, the same page instead takes the broker host, port, and credentials, and the test reading confirms the broker accepts them. Full detail on the MQTT side is in [`build_instructions_extended.md`](build_instructions_extended.md).

Press **Finish setup** when the test passes. The portal also closes on its own after 10 minutes.

> Your **device ID** is printed on the display and on the setup page. It is derived from the chip itself, so it never changes, even if you reflash the board.

> **To change a setting later**, press **RESET twice in quick succession**. The display tells you when the window is open, then setup mode returns. This is how you move the device to another network, correct the temperature offset, or point it somewhere else, without reflashing.

> ⚠️ **The current enclosure covers the RESET button.** Until a revised case opens it up, reaching it means sliding the board out of the housing. The way in that needs no button: the device opens setup mode by itself at power-up whenever it cannot reach a Wi-Fi network it already knows. Power it up out of range of the saved network — or with that network switched off — and the `SHS-xxxxxxxx-Setup` network appears on its own. (A network that drops *while* the device is running does not reopen setup; it reconnects in the background instead.)

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

![Finished device showing live sensor readings, IAQ still stabilizing after first boot](img/final_16.jpeg)

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
| **I need to change a setting** | Press **RESET twice quickly**. The display tells you when the window is open, then setup mode returns. In the current enclosure the button is covered — either take the board out, or power the device up where its saved network is unreachable, which opens setup mode anyway. |

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
