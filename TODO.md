# TODO

Outstanding items to finish the workshop repository. Most need hardware, photos, or a decision
from the maintainer — they couldn't be completed during the initial scaffolding.

## Verify on hardware
- [ ] **Compile the Arduino main build** (`code/shs_modular/`) in the Arduino IDE with the
      ESP32-C6 BSEC blob in place (see `build_instructions.md` §4.3). `arduino-cli` was not
      available during scaffolding, so this has not been compile-checked.
- [ ] **End-to-end MQTT test:** flash, confirm the device appears in Home Assistant via MQTT
      discovery and all seven entities update (IAQ, IAQ Accuracy, CO₂-eq, VOC, temp, humidity,
      pressure). Confirm the LWT marks it *offline* when powered down.
- [ ] **Verify the ESPHome variant** (`code/esphome/smart_home_sensor.yaml`) on real hardware —
      especially the `bme680_bsec2` platform and the ST7789 `display:`/`spi:` pins. Confirm the
      `board:`/`variant:` values are accepted by the installed ESPHome version.

## Configuration to finalize
- [ ] Replace the **placeholder MQTT broker defaults** in `config.h` (`MQTT_HOST`, `MQTT_USER`,
      `MQTT_PASS`) — decide what students are told to enter, and whether there's a shared
      workshop broker or each student uses their own HA.
- [ ] Confirm the **`DEVICE_ID` convention** students should follow (must be unique per device).
- [ ] Determine the **final `TEMP_OFFSET_C`** for the production enclosure (current 5.0 °C is
      from the bare board). Re-measure once an enclosure exists.

## Photos & diagrams (→ `img/`)
- [ ] Wiring photo / Fritzing-style diagram of the BME680 ↔ Waveshare board (referenced in
      `README.md` and `build_instructions.md` §2).
- [ ] Photo of the finished build (referenced at the top of `README.md`).
- [ ] Photo of the display showing live readings.
- [ ] Screenshot of the device + entities in Home Assistant (MQTT device card).

## Documentation polish
- [ ] Add an example **Home Assistant automation** (e.g. notify when CO₂-eq > 1000 ppm) once the
      entity names are confirmed on hardware.
