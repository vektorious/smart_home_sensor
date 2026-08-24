# TODO

Outstanding items to finish the workshop repository. Most need hardware, photos, or a decision
from the maintainer — they couldn't be completed during the initial scaffolding.

## Rework follow-ups

### Not yet verified on hardware
Everything below is verified by compilation and arithmetic only. A board has run the
firmware, but not since these landed.

- [ ] The QR screen shown for 20 s after setup: does it scan, and does the readings
      screen take over cleanly afterwards?
- [ ] The four-row setup grid, and the sensor status line updating while the portal runs.
- [ ] Spacing of the backend label (`API` / `MQ`) beside the Wi-Fi icon at the top right.
- [ ] Display rotation 1 as the default, and changing it in the portal.
- [ ] MQTT discovery entities appearing in Home Assistant.
- [ ] The double-reset window.
- [ ] The ESP Web Tools image picker in a browser (the manifest swap is untested there).

### Temperature offset is known to be wrong
- [ ] Re-measure `DEFAULT_TEMP_OFFSET_C`, in the production enclosure.

      The 5.0 °C default was characterised on `code/legacy/test_wv_display/`, which has no
      radio at all. With Wi-Fi modem sleep disabled a board needed roughly 7.5 °C. Modem
      sleep is now on (`wifi.ino`), so the true figure should sit between the two, but
      nobody has measured it since. Until then, expect a device to read warm and adjust in
      the portal — the field is prefilled with the value in use, so **add** the remaining
      error rather than replacing it.

### Workshop lifecycle
- [x] Generate the workshop API key + salt, create `workshop_secrets.h`, build against it.
- [x] Publish the flasher to `gh-pages`.
- [x] Apply the sprint policy on the server (the workshop image gets 401 until the key is
      known). See `web-flasher/README.md` § Workshop image.
- [ ] **26 Aug 2026 — retire the European Impact Sprint image.** Remove
      `manifest-workshop.json` and `firmware/shs-workshop.bin` from `gh-pages`, drop the
      option from `index.html`, and withdraw the key on the server. The flasher page states
      this date.
- [ ] **Back up `code/shs_modular/workshop_secrets.h` somewhere off this machine.** It is
      gitignored and exists nowhere else. The salt in it is the only thing that can
      reproduce the write key of every board flashed for the sprint; without it those
      device IDs can only be freed by deleting them server-side.

## Photos & diagrams (→ `img/`)
- [ ] Screenshot of the device + entities in Home Assistant (MQTT device card).
