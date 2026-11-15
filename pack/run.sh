#!/bin/sh
set -u

APP_DIR=${FROGGYTHEMES_DIR:-$(CDPATH= cd -- "$(dirname "$0")" && pwd)}
cd "$APP_DIR"
export LD_LIBRARY_PATH="$APP_DIR:/mnt/sdcard/cubegm/lib:/mnt/sdcard/cubegm/usr/lib:${LD_LIBRARY_PATH:-}"
rc=0
./froggythemes.mipsel >"$APP_DIR/log.txt" 2>&1 || rc=$?
echo "EXIT CODE: $rc" >>"$APP_DIR/log.txt"
exit "$rc"
