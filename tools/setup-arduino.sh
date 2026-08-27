#!/usr/bin/env bash
# Install the Arduino CLI toolchain into this checkout's persistent workspace.
# Nothing is installed system-wide: the CLI, Arduino data, downloads, and
# libraries all live under .toolchain/ and can be reused by future sessions.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLCHAIN="$ROOT/.toolchain"
BIN="$TOOLCHAIN/bin"
CLI_VERSION="1.3.1"
CLI="$BIN/arduino-cli"

mkdir -p "$BIN" "$TOOLCHAIN/data" "$TOOLCHAIN/downloads" "$TOOLCHAIN/user"

if [[ ! -x "$CLI" ]]; then
  case "$(uname -m)" in
    aarch64|arm64) arch=ARM64 ;;
    x86_64|amd64) arch=64bit ;;
    *) echo "Unsupported architecture: $(uname -m)" >&2; exit 2 ;;
  esac
  archive="arduino-cli_${CLI_VERSION}_Linux_${arch}.tar.gz"
  url="https://github.com/arduino/arduino-cli/releases/download/v${CLI_VERSION}/${archive}"
  curl -fL "$url" -o "$TOOLCHAIN/downloads/$archive"
  tar -xzf "$TOOLCHAIN/downloads/$archive" -C "$BIN" arduino-cli
  chmod +x "$CLI"
fi

export PATH="$BIN:$PATH"
export ARDUINO_DATA_DIR="$TOOLCHAIN/data"
export ARDUINO_DOWNLOADS_DIR="$TOOLCHAIN/downloads"
export ARDUINO_USER_DIR="$TOOLCHAIN/user"

if [[ ! -f "$TOOLCHAIN/arduino-cli.yaml" ]]; then
  "$CLI" config init --dest-dir "$TOOLCHAIN" >/dev/null
fi
"$CLI" config set directories.data "$ARDUINO_DATA_DIR" --config-file "$TOOLCHAIN/arduino-cli.yaml"
"$CLI" config set directories.downloads "$ARDUINO_DOWNLOADS_DIR" --config-file "$TOOLCHAIN/arduino-cli.yaml"
"$CLI" config set directories.user "$ARDUINO_USER_DIR" --config-file "$TOOLCHAIN/arduino-cli.yaml"
"$CLI" core update-index --config-file "$TOOLCHAIN/arduino-cli.yaml"
"$CLI" core install esp32:esp32@3.2.0 --config-file "$TOOLCHAIN/arduino-cli.yaml"

"$CLI" lib install \
  "WiFiManager@2.0.17" \
  "Adafruit BME680 Library@2.0.5" \
  "Adafruit Unified Sensor@1.1.15" \
  "Adafruit GFX Library@1.12.1" \
  "Adafruit ST7735 and ST7789 Library@1.11.0" \
  "PubSubClient@2.8.0" \
  --config-file "$TOOLCHAIN/arduino-cli.yaml"

echo "Toolchain installed under $TOOLCHAIN"
"$CLI" version
