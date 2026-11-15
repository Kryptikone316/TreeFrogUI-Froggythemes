#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=${1:-"$ROOT/dist/releases"}

command -v zip >/dev/null 2>&1 || {
    echo "zip is required to create release assets" >&2
    exit 1
}
command -v sha256sum >/dev/null 2>&1 || {
    echo "sha256sum is required to create release checksums" >&2
    exit 1
}

"$ROOT/scripts/validate_packs.sh"

for pack_root in theme-packs icon-packs; do
    pack="$ROOT/$pack_root/PrettyTreeFrog"
    [ -d "$pack" ] || {
        echo "missing cleared pack: $pack" >&2
        exit 1
    }
    [ -f "$pack/LICENSE" ] || {
        echo "missing pack license: $pack/LICENSE" >&2
        exit 1
    }
    [ -f "$pack/README.md" ] || {
        echo "missing pack readme: $pack/README.md" >&2
        exit 1
    }
done

mkdir -p "$OUT"
rm -f "$OUT/PrettyTreeFrog-theme.zip" \
      "$OUT/PrettyTreeFrog-icons.zip" \
      "$OUT/SHA256SUMS"

(cd "$ROOT/theme-packs" && zip -X -q -r "$OUT/PrettyTreeFrog-theme.zip" PrettyTreeFrog)
(cd "$ROOT/icon-packs" && zip -X -q -r "$OUT/PrettyTreeFrog-icons.zip" PrettyTreeFrog)

(cd "$OUT" && sha256sum PrettyTreeFrog-theme.zip PrettyTreeFrog-icons.zip > SHA256SUMS)

echo "Created release assets in $OUT"
cat "$OUT/SHA256SUMS"
