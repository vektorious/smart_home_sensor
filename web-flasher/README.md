# Smart Home Sensor — Web Flasher

A browser-based installer built on [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
Students open a page, pick a firmware, click **Connect**, and flash the board — no
Arduino IDE and no drivers.

## Files

| File | Purpose |
|---|---|
| `index.html` | The flasher page: firmware picker + install button |
| `manifest-workshop.json` | European Impact Sprint — keyed, time-limited |
| `manifest-sensorboard.json` | diy-sensor.org, keyless — the standard build |
| `manifest-ha.json` | Home Assistant over MQTT |
| `manifest-display.json` | Display only, no networking |
| `build.sh` | Builds all four images into `firmware/` |
| `vendor/` | ESP Web Tools, served from this site rather than a CDN (see `vendor/README.md`) |
| `firmware/*.bin` | Merged images (generated; gitignored on `main`) |

## The four images

All four come from the same sources in `code/shs_modular/`. None carries per-device
configuration: that lives in NVS and is entered in the setup portal after flashing,
which is what lets one image serve a whole room.

| Image | Build flags | Backend | Needs |
|---|---|---|---|
| **workshop** | `SHS_VARIANT=2` | diy-sensor.org, keyed | nothing — a Wi-Fi network |
| **sensorboard** | `SHS_VARIANT=2 SHS_NO_WORKSHOP_SECRETS` | diy-sensor.org, keyless | nothing |
| **ha** | `SHS_VARIANT=1` | MQTT auto-discovery | a broker + the HA MQTT integration |
| **display** | `SHS_VARIANT=3` | none | nothing |

`workshop` and `sensorboard` are the *same* variant and differ only in whether
`workshop_secrets.h` is compiled in — `SHS_NO_WORKSHOP_SECRETS` forces the keyless
build even on a machine that has the header, so both come out of one run of
`build.sh`. The include is also gated on `USE_SENSORBOARD`, which keeps the
credentials out of the `ha` and `display` images: those still copy
`DEFAULT_API_KEY` into settings, so without the gate the literal would be linked
into two binaries that have no use for it and are served publicly.

`build.sh workshop` refuses to run without `workshop_secrets.h` rather than quietly
producing an anonymous binary that calls itself the workshop image.

## Requirements (for whoever is flashing)

- A desktop browser with **Web Serial**: **Chromium**, a **recent Firefox**, Chrome, Edge, or Opera. It does not exist in older Firefox or
  Safari, or anywhere on iOS.
- The page must be served over **HTTPS** (or `http://localhost`). Web Serial refuses
  to run on plain `http://`.
- A USB-C **data** cable.

## Reproducible local toolchain

The optional `tools/setup-arduino.sh` installs a pinned Arduino CLI toolchain into
`.toolchain/` inside the checkout (which is gitignored): the CLI binary, ESP32 core,
archives, Arduino data, and libraries all remain available across sandbox sessions.
Run it once, then `web-flasher/build.sh sensorboard` uses that local CLI automatically.
The setup intentionally does **not** install the Arduino `QRCode` library: the sketch
uses Espressif's `qrcode.h` from the ESP32 core, and the third-party library shadows it.

## Building the images

```bash
./build.sh                 # all four
./build.sh sensorboard     # just one
```

The script needs `arduino-cli` with the `esp32` core and the libraries listed in
[`instructions/build_instructions.md`](../instructions/build_instructions.md). It
creates the missing `esp32c6` BSEC blob automatically if a library update has
removed it — that failure otherwise surfaces as a bare linker error.

Each build is one merged image flashed at offset `0x0`; the ESP32 Arduino core emits
it as `shs_modular.ino.merged.bin`, so no `esptool merge_bin` step is needed. The
**Huge APP (3MB No OTA/1MB SPIFFS)** partition scheme is required — the default is
too small for BSEC.

After building, bump `"version"` in the matching manifest so returning users are
offered the update.

### Doing it by hand in the Arduino IDE

1. Set `SHS_VARIANT` in `config.h`.
2. Board **ESP32C6 Dev Module**, **Tools → Partition Scheme → Huge APP**.
3. **Sketch → Export Compiled Binary**, then copy `build/…/shs_modular.ino.merged.bin`
   to `web-flasher/firmware/shs-<image>.bin`. For the keyless build, add
   `SHS_NO_WORKSHOP_SECRETS` to the sketch's build flags or temporarily move
   `workshop_secrets.h` aside.

## Workshop image

Built against the gitignored `code/shs_modular/workshop_secrets.h` (see
`workshop_secrets.example.h`), which bakes in an API key, the dashboard project, and
the salt each device derives its write key from.
