#!/usr/bin/env bash
# Build all three firmware images into web-flasher/firmware/.
#
# Usage:  ./build.sh [variant ...]        (default: all three)
#         ./build.sh sensorboard
#
# The output is a single merged image flashed at offset 0x0 — the ESP32 Arduino
# core produces it automatically, so no esptool merge_bin step is needed. That is
# exactly what the manifests expect: one part at offset 0.
set -euo pipefail

FQBN="esp32:esp32:esp32c6:PartitionScheme=huge_app"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="$REPO_ROOT/code"
OUT_DIR="$REPO_ROOT/web-flasher/firmware"
TOOLCHAIN="$REPO_ROOT/.toolchain"
CLI="${ARDUINO_CLI:-arduino-cli}"
CLI_ARGS=()
if [[ -x "$TOOLCHAIN/bin/arduino-cli" && -f "$TOOLCHAIN/arduino-cli.yaml" ]]; then
  CLI="$TOOLCHAIN/bin/arduino-cli"
  CLI_ARGS=(--config-file "$TOOLCHAIN/arduino-cli.yaml")
fi
BUILD_ROOT="$(mktemp -d)"
trap 'rm -rf "$BUILD_ROOT"' EXIT

declare -A VARIANTS=(
  [workshop]="-DSHS_VARIANT=2"
  [sensorboard]="-DSHS_VARIANT=2 -DSHS_NO_WORKSHOP_SECRETS"
  [ha]="-DSHS_VARIANT=1"
  [display]="-DSHS_VARIANT=3"
)
ORDER=(workshop sensorboard ha display)

BSEC_SRC="${BSEC_SRC:-$HOME/Arduino/libraries/bsec2/src}"
if [[ -d "$TOOLCHAIN/user/libraries/bsec2/src" ]]; then
  BSEC_SRC="$TOOLCHAIN/user/libraries/bsec2/src"
fi
if [[ -d "$BSEC_SRC/esp32c3" && ! -f "$BSEC_SRC/esp32c6/libalgobsec.a" ]]; then
  echo "==> Creating the missing esp32c6 BSEC blob from the esp32c3 one"
  mkdir -p "$BSEC_SRC/esp32c6"
  cp "$BSEC_SRC/esp32c3/libalgobsec.a" "$BSEC_SRC/esp32c6/libalgobsec.a"
fi

mkdir -p "$OUT_DIR"
targets=("$@")
if [[ ${#targets[@]} -eq 0 ]]; then targets=("${ORDER[@]}"); fi

HAS_SECRETS=0
[[ -f "$SKETCH_DIR/shs_modular/workshop_secrets.h" ]] && HAS_SECRETS=1

for name in "${targets[@]}"; do
  flags="${VARIANTS[$name]:-}"
  if [[ -z "$flags" ]]; then
    echo "Unknown image '$name' (expected: ${ORDER[*]})" >&2
    exit 2
  fi

  if [[ "$name" == "workshop" && $HAS_SECRETS -eq 0 ]]; then
    echo "!!  Cannot build the workshop image: workshop_secrets.h is missing." >&2
    echo "!!  Copy workshop_secrets.example.h to workshop_secrets.h and fill it in," >&2
    echo "!!  or build the keyless image instead:  ./build.sh sensorboard" >&2
    exit 3
  fi

  echo "==> Building $name ($flags)"
  "$CLI" "${CLI_ARGS[@]}" compile \
    --fqbn "$FQBN" \
    --build-property "compiler.cpp.extra_flags=$flags" \
    --output-dir "$BUILD_ROOT/$name" \
    "$SKETCH_DIR/shs_modular"

  cp "$BUILD_ROOT/$name/shs_modular.ino.merged.bin" "$OUT_DIR/shs-$name.bin"
  echo "    -> firmware/shs-$name.bin"
done

echo
echo "Done. Bump \"version\" in the matching manifest-*.json so returning users"
echo "get the update, then publish (see README.md § Hosting)."
