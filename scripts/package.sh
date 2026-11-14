#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=${1:-"$ROOT/dist/FroggyThemes"}

[ -x "$ROOT/froggythemes.mipsel" ] || {
    echo "build froggythemes.mipsel first with make" >&2
    exit 1
}

mkdir -p "$OUT/roms/ports/froggythemes" \
         "$OUT/frogui/theme-packs" "$OUT/frogui/icon-packs"
cp "$ROOT/froggythemes.mipsel" "$OUT/roms/ports/froggythemes/"
cp -a "$ROOT/pack/." "$OUT/roms/ports/froggythemes/"
cp -a "$ROOT/theme-packs/." "$OUT/frogui/theme-packs/"
cp -a "$ROOT/icon-packs/." "$OUT/frogui/icon-packs/"

echo "Packaged FroggyThemes under $OUT"
