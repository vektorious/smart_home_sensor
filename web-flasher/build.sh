#!/usr/bin/env bash
# Build all three firmware images into web-flasher/firmware/.
#
# Usage:  ./build.sh [variant ...]        (default: all three)
#         ./build.sh sensorboard
#
# The output is a single merged image per variant, flashed at offset 0x0 — the
# ESP32 Arduino core produces it automatically, so no esptool merge_bin step is
# needed. That is exactly what the manifests expect: one part at offset 0.
set -euo pipefail

FQBN="esp32:esp32:esp32c6:PartitionScheme=huge_app"
SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../code" && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/firmware"
BUILD_ROOT="$(mktemp -d)"
trap 'rm -rf "$BUILD_ROOT"' EXIT

# variant name -> SHS_VARIANT value (must match config.h)
declare -A VARIANTS=( [ha]=1 [sensorboard]=2 [display]=3 )

# The BSEC2 library ships no esp32c6 blob. The C6 is soft-float RISC-V
# (rv32imac) and ABI-compatible with the C3 one, so it is created as a copy —
# and a library update silently removes it again, which shows up as a link
# error rather than anything mentioning the missing folder.
BSEC_SRC="$HOME/Arduino/libraries/bsec2/src"
if [[ -d "$BSEC_SRC/esp32c3" && ! -f "$BSEC_SRC/esp32c6/libalgobsec.a" ]]; then
  echo "==> Creating the missing esp32c6 BSEC blob from the esp32c3 one"
  mkdir -p "$BSEC_SRC/esp32c6"
  cp "$BSEC_SRC/esp32c3/libalgobsec.a" "$BSEC_SRC/esp32c6/libalgobsec.a"
fi

if [[ ! -f "$SKETCH_DIR/shs_modular/workshop_secrets.h" ]]; then
  echo "!!  No workshop_secrets.h — the sensorboard image will publish anonymously"
  echo "!!  (temporary devices, 48 h idle expiry). Copy workshop_secrets.example.h"
  echo "!!  to workshop_secrets.h to bake in the workshop key and project."
fi

mkdir -p "$OUT_DIR"
targets=("${@:-${!VARIANTS[@]}}")

for name in "${targets[@]}"; do
  value="${VARIANTS[$name]:-}"
  if [[ -z "$value" ]]; then
    echo "Unknown variant '$name' (expected: ${!VARIANTS[*]})" >&2
    exit 2
  fi

  echo "==> Building $name (SHS_VARIANT=$value)"
  arduino-cli compile \
    --fqbn "$FQBN" \
    --build-property "compiler.cpp.extra_flags=-DSHS_VARIANT=$value" \
    --output-dir "$BUILD_ROOT/$name" \
    "$SKETCH_DIR/shs_modular"

  cp "$BUILD_ROOT/$name/shs_modular.ino.merged.bin" "$OUT_DIR/shs-$name.bin"
  echo "    -> firmware/shs-$name.bin"
done

echo
echo "Done. Bump \"version\" in the matching manifest-*.json so returning users"
echo "get the update, then publish (see README.md § Hosting)."
