# TODO

Outstanding items to finish the workshop repository. Most need hardware, photos, or a decision
from the maintainer — they couldn't be completed during the initial scaffolding.

## Photos & diagrams (→ `img/`)
- [ ] Screenshot of the device + entities in Home Assistant (MQTT device card).

## Rework follow-ups (2026-08-19)
- [ ] Verify all three images on hardware — none of the rework has run on a real board yet.
      In particular: the portal's send test against a live diy-sensor.org, MQTT discovery
      entities appearing in Home Assistant, and the double-reset window.
- [ ] Generate the workshop API key + salt, create `workshop_secrets.h`, and build the
      sensorboard image against it. Rotate both after the workshop and withdraw the image.
- [ ] Publish the flasher to a `gh-pages` branch and update the URL in
      `instructions/short_workshop.md` (currently guesses the repo's Pages URL).
- [ ] Confirm the ESP Web Tools manifest swap works in Chrome — the picker is untested in a
      browser.
- [ ] Re-measure `DEFAULT_TEMP_OFFSET_C` in the production enclosure.
