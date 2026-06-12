# Legacy / Reference Sketches

Unsupported, kept for reference only.

## `test_wv_display/`

The original monolithic sketch the workshop firmware grew out of. It drives the ST7789
display and reads the BME680 through Bosch BSEC2, but has **no networking** — it does not
connect to Wi-Fi or publish anything to Home Assistant.

Use the modular firmware in [`../shs_modular/`](../shs_modular/) for the actual build. This
sketch is useful as a minimal example for bringing up just the display and sensor (e.g. when
debugging hardware), and as the source the modular `display.ino` / `bme680.ino` modules were
extracted from.
