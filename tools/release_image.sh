#!/usr/bin/env bash
# Build a release's game file: Tithe-v<version>.bin, a full flash image
# (bootloader, partition table and game), and check it the way each installer
# reads it.
#
#   tools/release_image.sh [out]      default out: release/
#
# One file covers every way in. M5Launcher (any version) installs it from the
# SD card's /downloads or its OTA list: 2.6 and later read the partition table
# inside and give the game its own data area ("tithe", firmware/partitions.csv),
# older ones take the game and keep their own table. M5Burner and esptool write
# it whole at 0x0. The game alone (firmware.bin), installed by M5Launcher 2.6 or
# later, gets no data area, so it isn't published.
set -euo pipefail
REPO="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
FW="$REPO/firmware"
OUT="${1:-$REPO/release}"
PY="$(head -1 "$(command -v pio)" | sed "s/^#!//")"   # the Python PlatformIO runs on, which has what esptool needs
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
(cd "$FW" && pio run -e cardputer >/dev/null)
V="$(sed -n 's/.*Version = "\([^"]*\)".*/\1/p' "$FW/src/Config.h")"
B="$FW/.pio/build/cardputer"
mkdir -p "$OUT"
IMG="$OUT/Tithe-v$V.bin"
"$PY" "$ESPTOOL" --chip esp32s3 merge_bin -o "$IMG" --flash_mode keep --flash_freq keep --flash_size keep \
  0x0 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" 0xe000 "$BOOT_APP0" 0x10000 "$B/firmware.bin" >/dev/null
"$PY" "$REPO/tools/check_image.py" "$IMG" "$B/firmware.bin" "$V"
echo "$IMG"
