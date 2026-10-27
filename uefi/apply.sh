#!/bin/bash
# Put HakiOS's UEFI side into a Mu-Silicium tree that already has the X01A port
# (github.com/Stoobyy/mu-silicium-saipan, its apply.sh):
#   uefi/apply.sh MU_SILICIUM_DIR
#  - BootShim.S: copy an "XNUP" payload appended after the FD to 0xF0000000
#  - BootManagerLib: start the XNU loader right after the Volume Up check (XnuLoader.c)
#  - x01aPkg's memory map: 0xF0000000 (64 MB) reserved for that payload, free DDR on this phone
set -e
cd "$(dirname "$(readlink -f "$0")")"
MU=$(cd "$1" && pwd)
MAP="$MU/Platforms/Balmuda/x01aPkg/Library/MemoryMapLib/MemoryMapLib.c"
[ -f "$MAP" ] || { echo "uefi/apply.sh: no x01aPkg in $MU (run mu-silicium-saipan's apply.sh first)"; exit 1; }
# start from clean copies of what the patch touches, so this can be run again
git -C "$MU" checkout -q -- BootShim/BootShim.S Silicon/Silicium/SiliciumPkg/Library/BootManagerLib
git -C "$MU" apply --whitespace=nowarn "$PWD/mu-silicium.patch"
cp BootManager.c XnuLoader.c XnuLoader.h "$MU/Silicon/Silicium/SiliciumPkg/Library/BootManagerLib/"
grep -q '"XNU Payload"' "$MAP" || sed -i 's#^\(  {"DBI Dump".*\)$#\1\n\n  // HakiOS: the boot shim copies an "XNUP" payload here. Free DDR on this phone.\n  {"XNU Payload",       0xF0000000, 0x04000000, AddMem, SYS_MEM, SYS_MEM_CAP, Reserv, WRITE_BACK_XN},#' "$MAP"
grep -q '"XNU Payload"' "$MAP"
echo "uefi/apply.sh: done; next: cd $MU && python3 build_uefi.py -d x01a -r DEBUG"
