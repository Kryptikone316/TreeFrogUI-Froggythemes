#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
failed=0

for pack in "$ROOT/theme-packs"/*; do
    [ -d "$pack" ] || continue
    image="$pack/main.jpg"
    if [ ! -f "$image" ]; then
        printf 'FAIL %-28s missing main.jpg\n' "$(basename "$pack")"
        failed=1
        continue
    fi
    info=$(identify -format '%m %w %h %[channels]' "$image")
    if [ "$info" != "JPEG 854 480 sRGB" ] && [ "$info" != "JPEG 854 480 srgb" ]; then
        printf 'FAIL %-28s %s\n' "$(basename "$pack")" "$info"
        failed=1
    else
        printf 'OK   %-28s %s\n' "$(basename "$pack")" "$info"
    fi
done

for pack in "$ROOT/icon-packs"/*; do
    [ -d "$pack" ] || continue
    while IFS= read -r image; do
        info=$(identify -format '%w %h %[channels]' "$image")
        width=$(printf '%s' "$info" | awk '{print $1}')
        height=$(printf '%s' "$info" | awk '{print $2}')
        channels=$(printf '%s' "$info" | awk '{print $3}')
        case "$channels" in sRGB|srgb|srgba|RGBA|rgba) ;; *)
            printf 'FAIL %s %s\n' "${image#"$ROOT/"}" "$info"
            failed=1
            continue
            ;; esac
        if [ "$width" -gt 160 ] || [ "$height" -gt 130 ]; then
            printf 'FAIL %s %s\n' "${image#"$ROOT/"}" "$info"
            failed=1
        fi
    done <<EOF
$(find "$pack" -maxdepth 1 -type f -name '*.png' -print)
EOF
done

exit "$failed"
