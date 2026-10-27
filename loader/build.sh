#!/bin/bash
# Build HakiOS's XNU loader with the X01A platform (loader/xnuloader-x01a.patch) and pack it,
# with the UEFI built after uefi/apply.sh, into the boot image `xl enter` would boot on the POCO:
#   loader/build.sh HAKIOS_DIR MU_SILICIUM_DIR [OUT_IMG]
#   MODE=DEBUG   the UEFI build to take (default RELEASE: no driver messages on the screen)
# HAKIOS_DIR/xnuloader/identity.h is used if it is there (xnuloader/mkidentity.sh makes it from
# your own IPSW); otherwise a placeholder one, enough for the loader but not for iOS.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
HAKIOS=$1; MU=$2; OUT=${3:-$HERE/out/xnu-usb.img}
mkdir -p "$(dirname "$OUT")"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp -r "$HAKIOS/xnuloader" "$HAKIOS/sysimage" "$W/"
cd "$W/xnuloader"
sed -i 's/\r$//' *.c *.h *.sh *.py
patch -p1 < "$HERE/loader/xnuloader-x01a.patch"
[ -f identity.h ] || bash mkidentity.sh --placeholder
clang --target=aarch64-unknown-windows -ffreestanding -fno-builtin -fno-stack-protector -fshort-wchar \
  -mgeneral-regs-only -O1 -Wall -Wextra -Wno-unused-parameter -c loader.c -o loader.obj
# 0xA8000000: free DDR on the X01A (HakiOS's 0xA1000000 is inside this phone's framebuffer region)
lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /base:0xA8000000 /out:loader.efi loader.obj
python3 mkpayload.py -o usb-payload.bin loader.efi=loader.efi --usb
python3 "$HERE/tools/mkxnuimg.py" "$MU" -p usb-payload.bin -o "$OUT" --mode "${MODE:-RELEASE}"
