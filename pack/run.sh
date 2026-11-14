#!/bin/sh
set -u

PORT_DIR=/mnt/sdcard/roms/ports/froggythemes
cd "$PORT_DIR"
export LD_LIBRARY_PATH="$PORT_DIR:/mnt/sdcard/cubegm/lib:/mnt/sdcard/cubegm/usr/lib:${LD_LIBRARY_PATH:-}"
rc=0
./froggythemes.mipsel >log.txt 2>&1 || rc=$?
echo "EXIT CODE: $rc" >>log.txt
exit "$rc"
