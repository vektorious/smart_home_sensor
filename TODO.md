# TODO

Outstanding items to finish the workshop repository. Most need hardware, photos, or a decision
from the maintainer — they couldn't be completed during the initial scaffolding.

## Photos & diagrams (→ `img/`)
- [ ] Screenshot of the device + entities in Home Assistant (MQTT device card).

## Rework follow-ups (2026-08-19)
- [ ] Verify all three images on hardware — none of the rework has run on a real board yet.
      In particular: the portal's send test against a live diy-sensor.org, MQTT discovery
      entities appearing in Home Assistant, and the double-reset window.
- [x] Generate the workshop API key + salt, create `workshop_secrets.h`, build against it.
- [x] Publish the flasher to `gh-pages` (live at alexanderkutschera.com/smart_home_sensor/).
- [ ] Apply the sprint policy to the server `.env` and restart — the workshop image gets
      401 until the key is known. See web-flasher/README.md § Workshop image.
- [ ] **26 Aug 2026 — retire the European Impact Sprint image.** Remove
      `manifest-workshop.json` + `firmware/shs-workshop.bin` from `gh-pages`, drop the
      option from `index.html`, remove the key from `API_KEYS`, restart, then
      `python -m app.admin delete-key-data <sha256>`. The page promises this date.
- [ ] Confirm the ESP Web Tools manifest swap works in Chrome — the picker is untested in a
      browser.
- [ ] Re-measure `DEFAULT_TEMP_OFFSET_C` in the production enclosure.
