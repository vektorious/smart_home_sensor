# Sensor-board serial identity diagnostics

This is a standalone Arduino sketch for investigating board identity collisions.
It does **not** compile the full sensor-board project and does not use sensors,
the display, NVS, the setup portal, the API, MQTT, or workshop credentials.
It does not join a Wi-Fi network.

## Flash

From the repository root, with an ESP32-C6 board connected:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c6 \\
  code/debug/sensor_board_serial_diagnostics
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn esp32:esp32:esp32c6 \\
  code/debug/sensor_board_serial_diagnostics
arduino-cli monitor -p /dev/cu.usbmodemXXXX -c baudrate=115200
```

On Linux the port will usually be `/dev/ttyACM0` or `/dev/ttyUSB0`. On macOS,
list ports with:

```sh
arduino-cli board list
```

If the board does not auto-reset into the bootloader, hold **BOOT**, tap
**RESET**, release **BOOT**, then retry the upload.

## Record a board

1. Disconnect other sensor boards from USB.
2. Flash this sketch to one board.
3. Open the serial monitor at **115200 baud**.
4. Wait for a complete block between `REPORT_BEGIN` and `REPORT_END`.
5. Save the complete block, including the board number and date.
6. Repeat with every board, keeping one report per physical board.

The report repeats every five seconds. The important comparison fields are:

- `efuse_mac_u64`
- `efuse_mac_human`
- `esp_idf_factory_mac`
- `esp_idf_base_mac`
- `wifi_sta_mac`
- `arduino_wifi_sta_mac`
- `production_derived_device_id`

`esp_idf_factory_mac` is the factory MAC read directly through ESP-IDF;
`esp_idf_base_mac` is the base MAC currently exposed by the IDF; the interface
MACs are derived from the base MAC. `esp_idf_base_mac` is diagnostic only—the
sketch does not modify it.

If two physical boards have the same `efuse_mac_u64` or the same
`production_derived_device_id`, preserve both complete reports. Do not edit or
retype the values; the raw reports are the evidence needed for diagnosis.
