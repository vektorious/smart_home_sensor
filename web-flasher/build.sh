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

# Image name -> the flags that define it. The two diy-sensor images are the same
# variant: "workshop" compiles in workshop_secrets.h, "sensorboard" is forced
# keyless so it can outlive the event.
declare -A VARIANTS=(
  [workshop]="-DSHS_VARIANT=2"
  [sensorboard]="-DSHS_VARIANT=2 -DSHS_NO_WORKSHOP_SECRETS"
  [ha]="-DSHS_VARIANT=1"
  [display]="-DSHS_VARIANT=3"
)
# Deterministic order, so a full run always builds them in the same sequence.
ORDER=(workshop sensorboard ha display)

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

  # Refuse to build a workshop image without credentials. It would compile and
  # look right, then publish anonymously — indistinguishable from the standard
  # image except that it claims to be the workshop one.
  if [[ "$name" == "workshop" && $HAS_SECRETS -eq 0 ]]; then
    echo "!!  Cannot build the workshop image: workshop_secrets.h is missing." >&2
    echo "!!  Copy workshop_secrets.example.h to workshop_secrets.h and fill it in," >&2
    echo "!!  or build the keyless image instead:  ./build.sh sensorboard" >&2
    exit 3
  fi

  echo "==> Building $name ($flags)"
  arduino-cli compile \
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
