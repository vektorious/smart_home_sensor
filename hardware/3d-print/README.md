# 3D-Printed Enclosure

An enclosure for the Waveshare ESP32-C6-LCD-1.3 + BME680 that keeps the display visible,
gives the BME680 airflow while shielding it from the warm side of the board, and routes the
USB-C cable for permanent powered operation.

## Parts

| File | Description |
|------|-------------|
| `SmartHomeCube-Main.3mf` | Main body — cubic version |
| `SmartHomeCube-Main_thin.3mf` | Main body — thin version (5 mm shorter than cubic) |
| `SmartHomeCube-Lid.3mf` | Lid |
| `SmartHomeCube-Lid_w_offset.3mf` | Lid with additional offset |

Print with standard PLA settings; no supports needed.

## Modifying the fit

The FreeCAD source is in `src/SmartHomeCube.FCStd`. All clearances and offsets are exposed in
the **Variable set** (`Model → Variable set`) so you can adjust them for a tight fit without
touching the geometry directly.
