#!/bin/bash
# Builds one KeySpeaker sketch for the M5Stack Cardputer ADV with arduino-cli and writes the app-only image
# (for a launcher) and its checksum next to the sketch.
# Needs: arduino-cli, the "m5stack:esp32" core 3.3.10, libraries M5Cardputer, M5Unified, M5GFX.
#   ./build.sh KeySpeaker_S0
# The compiler is told to replace the build machine's folders ($HOME and this repository) in the paths it embeds,
# so the published .bin files do not carry your user name or folder layout.
# The linker sends TinyUSB's dcd_edpt_iso_activate() through the sketch's guard first (KeySpeaker_S0/UsbIsoFix.h),
# and the USB interface names through KeySpeaker_S0/UsbNames.h.
set -e
cd "$(dirname "$0")"
SKETCH=${1:?usage: ./build.sh <sketch folder, e.g. KeySpeaker_S0>}
[ -f "$SKETCH/$SKETCH.ino" ] || { echo "$SKETCH/$SKETCH.ino not found" >&2; exit 1; }
REPO=$(cd .. && pwd)
MAPS="-ffile-prefix-map=$REPO=/repo -ffile-prefix-map=$HOME=/home/user"
for m in $EXTRA_PREFIX_MAPS; do MAPS="$MAPS -ffile-prefix-map=$m"; done
OUT=$(mktemp -d)
arduino-cli compile --clean --fqbn m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,FlashSize=8M,USBMode=default,CDCOnBoot=cdc \
  --build-property "compiler.c.extra_flags=$MAPS" --build-property "compiler.cpp.extra_flags=$MAPS" \
  --build-property "compiler.c.elf.extra_flags=-Wl,--wrap=dcd_edpt_iso_activate -Wl,--wrap=tinyusb_add_string_descriptor" \
  --output-dir "$OUT" "$SKETCH"
cp "$OUT/$SKETCH.ino.bin" "$SKETCH/"
(cd "$SKETCH" && shasum -a 256 "$SKETCH.ino.bin" > SHA256SUMS.txt && cat SHA256SUMS.txt)
rm -rf "$OUT"
