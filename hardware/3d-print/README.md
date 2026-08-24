# 3D-Printed Enclosure

An enclosure for the Waveshare ESP32-C6-LCD-1.3 + BME680 that keeps the display visible,
gives the BME680 airflow while shielding it from the warm side of the board, and routes the
USB-C cable for permanent powered operation.

## Assembly

![Exploded assembly animation](SmartHomeCube-assembly.gif)

The enclosure is assembled from bottom to top in this order:

1. Main housing
2. ESP32-C6-LCD-1.3-M
3. Lower inlay
4. Upper inlay
5. Lid

The higher-quality MP4 version is available as [`SmartHomeCube-assembly.mp4`](SmartHomeCube-assembly.mp4).

## Parts

| File | Description | |
|------|-------------|---|
| `SmartHomeCube-Main.3mf` | Main body | required |
| `SmartHomeCube-Inlay.3mf` | Lower inlay, sits on the board and carries the sensor above it | optional |
| `SmartHomeCube-Inlay_top.3mf` | Upper inlay, holds the BME680 under the lid opening | optional |
| `SmartHomeCube-Lid.3mf` | Lid | required |

The two inlays position the BME680 above the board and away from its heat. Without them the sensor still works, but something insulating should go between board and sensor instead, for example the styrofoam packing piece the ESP32 board ships with.

Print with standard PLA settings; no supports needed.

## Modifying the fit

The FreeCAD source is in `src/SmartHomeCube.FCStd`. All clearances and offsets are exposed in
the **Variable set** (`Model → Variable set`) so you can adjust them for a tight fit without
touching the geometry directly.
