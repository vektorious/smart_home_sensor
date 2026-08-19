# Smart Home Sensor — Web Flasher

A browser-based installer built on [ESP Web Tools](https://esphome.github.io/esp-web-tools/).
Students open a page, pick a firmware, click **Connect**, and flash the board — no
Arduino IDE and no drivers.

## Files

| File | Purpose |
|------|---------|
| `index.html` | The flasher page: firmware picker + install button |
| `manifest-workshop.json` | European Impact Sprint — keyed, time-limited |
| `manifest-sensorboard.json` | diy-sensor.org, keyless — the standard build |
| `manifest-ha.json` | Home Assistant over MQTT |
| `manifest-display.json` | Display only, no networking |
| `build.sh` | Builds all four images into `firmware/` |
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

- **Chrome, Edge, or Opera** on a desktop. Web Serial does not exist in Firefox or
  Safari, or anywhere on iOS.
- The page must be served over **HTTPS** (or `http://localhost`). Web Serial refuses
  to run on plain `http://`.
- A USB-C **data** cable.

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

**Why it carries a key at all.** Not for persistence — that is switched *off* for the
workshop policy. It is the per-IP limits: a class sits behind one NAT address, and the
anonymous policy allows 10 active devices, 5 new devices an hour, and 1,000 requests a
day. A keyless class of 80 boards would stop at the fifth. With a key those become
500 / 100 / 50,000.

**Treat it as a temporary artefact.** Anything in that header is inside a binary served
publicly, so `strings` recovers it in seconds. The **API key** is therefore effectively
published: anyone who downloads the image can write to the instance under the workshop
policy, and because the write budget is charged per credential they can drain the same
bucket the class draws from.

The **salt** is weaker than it looks, but not as weak as "the key is public":

- The write key is `HMAC-SHA256(salt, mac)` over the **full 48-bit MAC**, while the
  public device ID carries only the low 32 bits — and those are the OUI plus one NIC
  byte, the predictable part. A device ID therefore leaves 2^16 = 65,536 candidates,
  with no offline way to test one: each guess costs a POST. Under the per-IP limits
  that is hours per device and a wall of 403s in the ingest log.
- Proximity defeats it entirely. The ESP32's Wi-Fi station MAC **is** the base efuse
  MAC — the exact derivation input — broadcast in the clear in every frame. Anyone in
  radio range reads all 48 bits passively, without joining the network.

Inherent to deriving from the MAC; no salt scheme changes it, and it is the price of a
key that survives a flash erase so a student cannot brick their device ID. For a
classroom it is a prank vector, not a breach.

### Retiring it

The page states a withdrawal date, and the date is the point:

1. delete `manifest-workshop.json` and `firmware/shs-workshop.bin` from `gh-pages`,
   and drop the option from `index.html`;
2. remove the key from `API_KEYS` in the server `.env` and restart the service;
3. `python -m app.admin delete-key-data <sha256>` for anything left behind.

With `POLICY_<NAME>_PERSISTENT_DEVICES=false` step 3 is mostly a formality: the
devices expire 48 h after their last reading on their own.

**Set that policy before the first board claims its ID.** `persistent` is a column
stamped at creation (`routes/ingest.py`: `persistent=authenticated and
policy.persistent_devices`) and the sweep reads the column, not the current policy.
Applied afterwards it changes nothing for devices that already exist: they stay
persistent forever, are never swept, and their IDs never become claimable again
without deleting them by hand.

Students keep their hardware: the keyless **sensorboard** image stays on the page and
publishes to the same dashboard. It claims the same device ID, so if the workshop
device has not expired yet, paste the write key shown on the workshop portal page into
the keyless build's *Write key* field to adopt it. After expiry the ID is simply free
again.

## Hosting

GitHub Pages publishes from a branch root or `/docs`, not an arbitrary subfolder, so
the site lives on a dedicated `gh-pages` branch mirroring this folder at its root:

```
gh-pages/
├── index.html
├── manifest-workshop.json
├── manifest-sensorboard.json
├── manifest-ha.json
├── manifest-display.json
├── .nojekyll
└── firmware/
    ├── shs-workshop.bin
    ├── shs-sensorboard.bin
    ├── shs-ha.bin
    └── shs-display.bin
```

The binaries live only on `gh-pages` — four 4 MB images per release would bloat
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
