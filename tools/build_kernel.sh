#!/bin/bash
# Build the X01A kernel with ath10k, using the kernel-only target by default.
# Usage: tools/build_kernel.sh HAKIOS_DIR [make arguments...]
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "${1:?usage: build_kernel.sh HAKIOS_DIR [make arguments]}" && pwd)
shift
source "$H/xnu-env.sh"
export XNU_SRC=${X01A_XNU_SRC:-$XNU_SRC}
cd "$XNU_SRC"
LX=${ATH10K_OBJECT:-${WIFI_BUILD_DIR:-$HERE/out/wifi-build}/out/ath10k.o}
if [ "${NO_DRIVER:-0}" = 1 ]; then LX=; else
 [ -f "$LX" ] || { echo "build_kernel.sh: run tools/build_wifi.sh first" >&2; exit 1; }
fi
[ "$#" -gt 0 ] || set -- build -j12
STAMP=BUILD/obj/hakios-objects
mtime() { if [ "$XNU_HOST_OS" = Darwin ]; then stat -f %m "$1"; else stat -c %Y "$1"; fi; }
if [ -f "$LX" ]; then now="$LX $(mtime "$LX")"; else LX= now=; fi
if [ "$XNU_HOST_OS" = Darwin ]; then HOSTCC=clang; else
HOSTCC="clang -include $XNUDEPS/hostinc/hostcompat.h -I$XNUDEPS/hostinc -Wno-unused-command-line-argument -Wno-error=implicit-function-declaration"; fi
if [ -d BUILD/obj ] && [ "$(cat $STAMP 2>/dev/null)" != "$now" ]; then
	rm -f BUILD/obj/*/mach.*.unstripped
	echo "$now" > $STAMP
fi
make HAKIOS_OBJECTS="$LX" SDKROOT=iphoneos ARCH_CONFIGS=ARM64 MACHINE_CONFIGS=${MACHINE:-BCM2837} KERNEL_CONFIGS=DEVELOPMENT \
  ARCH_STRING_FOR_CURRENT_MACHINE_CONFIG=arm64 BUILD_WERROR=0 BUILD_LTO=0 DO_CTFMERGE=0 \
  HOST_CC="$HOSTCC" HOST_FLEX=flex HOST_BISON=bison HOST_GM4=m4 CC=xnu-clang CXX=xnu-clang++ LOGCOLORS=n "$@"

# Keep the runnable kernel in this repository's ignored output bundle.
BUILT="$XNU_SRC/BUILD/obj/DEVELOPMENT_ARM64_${MACHINE:-BCM2837}/mach.development.$(printf '%s' "${MACHINE:-BCM2837}" | tr '[:upper:]' '[:lower:]')"
if [ -f "$BUILT" ]; then
 mkdir -p "$HERE/out"
 cp "$BUILT" "$HERE/out/kernel"
 echo "build_kernel.sh: staged $HERE/out/kernel"
fi
