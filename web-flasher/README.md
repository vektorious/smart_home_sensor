# Smart Home Sensor — Web Flasher

A browser-based installer built on [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
Students open a page, pick a firmware, click **Connect**, and flash the board — no
Arduino IDE and no drivers.

## Files

| File | Purpose |
|------|---------|
| `index.html` | The flasher page: firmware picker + install button |
| `manifest-sensorboard.json` | Short workshop → diy-sensor.org |
| `manifest-ha.json` | Home Assistant over MQTT |
| `manifest-display.json` | Display only, no networking |
| `build.sh` | Builds all three images into `firmware/` |
| `firmware/*.bin` | Merged images (generated; gitignored on `main`) |

## The three images

All three come from the same sources in `code/shs_modular/`, selected by
`SHS_VARIANT`. None of them carries per-device configuration: that lives in NVS and
is entered in the setup portal after flashing, which is what lets one image serve a
whole room.

| Image | Backend | Needs |
|---|---|---|
| **sensorboard** | `POST` to diy-sensor.org | nothing — a Wi-Fi network |
| **ha** | MQTT auto-discovery | an MQTT broker + the HA MQTT integration |
| **display** | none | nothing |

## Requirements (for whoever is flashing)

- **Chrome, Edge, or Opera** on a desktop. Web Serial does not exist in Firefox or
  Safari, or anywhere on iOS.
- The page must be served over **HTTPS** (or `http://localhost`). Web Serial refuses
  to run on plain `http://`.
- A USB-C **data** cable.

## Building the images

```bash
./build.sh                 # all three
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
   to `web-flasher/firmware/shs-<variant>.bin`.

## Workshop image

The sensorboard image is built against the gitignored
`code/shs_modular/workshop_secrets.h` (see `workshop_secrets.example.h`), which bakes
in an API key, a project slug, and the salt each device derives its write key from.

**Treat it as a temporary artefact.** Anything in that header is inside a binary you
are serving publicly: anyone who downloads it can read the API key, and can derive
any workshop device's write key from its (public) device ID. That is an accepted
trade-off for a short workshop, not a safe steady state. So:

- generate a fresh salt (`openssl rand -hex 32`) and a fresh API key per workshop;
- take the image down from `gh-pages` when the workshop ends;
- revoke the API key afterwards.

Without `workshop_secrets.h` the build still works — devices then publish
anonymously with a random write key and expire 48 h after their last write.

## Hosting

GitHub Pages publishes from a branch root or `/docs`, not an arbitrary subfolder, so
the site lives on a dedicated `gh-pages` branch mirroring this folder at its root:

```
gh-pages/
├── index.html
├── manifest-sensorboard.json
├── manifest-ha.json
├── manifest-display.json
├── .nojekyll
└── firmware/
    ├── shs-sensorboard.bin
    ├── shs-ha.bin
    └── shs-display.bin
```

The binaries live only on `gh-pages` — three 4 MB images per release would bloat
`main`'s history for no benefit, which is why `firmware/.gitignore` excludes them.

```bash
git worktree add /tmp/ghpages gh-pages
cp index.html manifest-*.json /tmp/ghpages/
cp firmware/*.bin /tmp/ghpages/firmware/
# bump "version" in the manifests you changed
git -C /tmp/ghpages commit -am "Update flasher firmware" && git -C /tmp/ghpages push
git worktree remove /tmp/ghpages
```

## Local testing

```bash
python3 -m http.server 8000
# open http://localhost:8000 — localhost counts as secure for Web Serial
```
