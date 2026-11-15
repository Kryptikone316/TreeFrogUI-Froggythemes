#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=${1:-"$ROOT/dist/FroggyThemes"}

[ -x "$ROOT/froggythemes.mipsel" ] || {
    echo "build froggythemes.mipsel first with make" >&2
    exit 1
}

mkdir -p "$OUT/roms/ports/froggythemes" \
         "$OUT/cubegm/froggythemes" \
         "$OUT/frogui/theme-packs" "$OUT/frogui/icon-packs"
cp "$ROOT/froggythemes.mipsel" "$OUT/roms/ports/froggythemes/"
cp -a "$ROOT/pack/." "$OUT/roms/ports/froggythemes/"
cp "$ROOT/froggythemes.mipsel" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/active-frog.rgba" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/run.sh" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/libfreetype.so.6" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/libjpeg.so.9" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/libpng16.so.16" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/libz.so.1" "$OUT/cubegm/froggythemes/"
cp "$ROOT/pack/Froggythemes-app.sh" "$OUT/cubegm/froggythemes.sh"
chmod 0755 "$OUT/cubegm/froggythemes/run.sh" "$OUT/cubegm/froggythemes.sh"
cp -a "$ROOT/theme-packs/." "$OUT/frogui/theme-packs/"
cp -a "$ROOT/icon-packs/." "$OUT/frogui/icon-packs/"

echo "Packaged FroggyThemes under $OUT"
