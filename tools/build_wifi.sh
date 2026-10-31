#!/bin/bash
# Build pinned Linux ath10k using HakiOS's LinuxKPI glue and the X01A platform layer.
# Usage: tools/build_wifi.sh HAKIOS_DIR
# WLAN_DL: directory holding the two checksum-pinned source archives (wifi/README.md).
set -e
HERE=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
H=$(cd "${1:?usage: build_wifi.sh HAKIOS_DIR}" && pwd)
DL=${WLAN_DL:-$HERE/downloads}
B=${WIFI_BUILD_DIR:-$HERE/out/wifi-build}
X=$HERE/wifi/lx10k
W=$H/wifi/lx
ATH=drivers/net/wireless/ath
VERSION=7.0.14
SHA256=de9999b784d2293f00d39c62d8f92a08ab8a54bc4e80ffd250a0c09cb07a0f98
FREEBSD=15.1
FREEBSD_SHA256=cf5762da53fd52e1eaf0f9ceee9bf58cbe314c821031d0d9ffa76823185a89e1
mkdir -p "$B"
cd "$B"

# --- the driver's sources, with each file's licence checked
if [ "$(cat src/.release 2>/dev/null)" != "$VERSION $SHA256" ]; then
	echo "$SHA256  $DL/linux-$VERSION.tar.xz" | sha256sum -c --quiet
	rm -rf src unpacked
	mkdir unpacked
	tar xJf "$DL/linux-$VERSION.tar.xz" -C unpacked --strip-components=1 linux-$VERSION/$ATH/ath10k \
		$(for f in ath.h main.c regd.c regd.h regd_common.h reg.h key.c hw.c debug.c dfs_pattern_detector.c \
		    dfs_pattern_detector.h dfs_pri_detector.c dfs_pri_detector.h spectral_common.h testmode_i.h trace.h; do
			echo linux-$VERSION/$ATH/$f; done)
	for f in unpacked/$ATH/ath10k/*.[ch] unpacked/$ATH/*.[ch]; do
		case "$(head -16 "$f")" in
		*"SPDX-License-Identifier: ISC"*) ;;
		*"SPDX-License-Identifier: BSD-3-Clause-Clear"*) ;;
		*"Permission to use, copy, modify, and/or distribute this software"*) ;;      # (ISC, spelt out)
		*) echo "build_wifi.sh: $f does not carry the licence expected" >&2; exit 1 ;;
		esac
	done
	mkdir src
	mv unpacked/$ATH/ath10k src/ath10k
	mv unpacked/$ATH/*.[ch] src/
	rm -rf unpacked
	echo "$VERSION $SHA256" > src/.release
fi

# --- FreeBSD's LinuxKPI, patched as HakiOS patches it (taken afresh when the patches change)
FSYS="compat/linuxkpi sys arm64/include net net80211 netinet netinet6 vm kern tools libkern ddb
	security fs/pseudofs crypto opencrypto geom bsm isa cam rpc xdr x86/include ofed/include
	dev/pci dev/ofw dev/fdt dev/acpica dev/usb dev/iicbus dev/backlight
	contrib/ck/include contrib/dev/acpica/include contrib/libnv"
PATCHES=$(cat "$W"/patches/*.patch 2>/dev/null | sha256sum | cut -d' ' -f1)
if [ "$(cat freebsd/.release 2>/dev/null)" != "$FREEBSD $FREEBSD_SHA256 $PATCHES" ]; then
	echo "$FREEBSD_SHA256  $DL/freebsd-$FREEBSD-src.txz" | sha256sum -c --quiet
	rm -rf freebsd gen obj
	mkdir freebsd
	tar xJf "$DL/freebsd-$FREEBSD-src.txz" -C freebsd --strip-components=2 usr/src/COPYRIGHT \
		$(for d in $FSYS; do echo usr/src/sys/$d; done)
	for p in "$W"/patches/*.patch; do
		[ -f "$p" ] && patch -s --no-backup-if-mismatch -d freebsd -p1 < "$p"
	done
	echo "$FREEBSD $FREEBSD_SHA256 $PATCHES" > freebsd/.release
fi
S=$B/freebsd/sys

# --- what FreeBSD's build would have generated
if [ ! -d gen ]; then
	mkdir gen
	(
		cd gen
		ln -s $S/arm64/include machine
		for m in kern/device_if.m kern/bus_if.m dev/pci/pci_if.m dev/pci/pci_iov_if.m dev/pci/pcib_if.m \
			dev/iicbus/iicbus_if.m dev/iicbus/iicbb_if.m dev/backlight/backlight_if.m dev/usb/usb_if.m \
			dev/ofw/ofw_bus_if.m; do
			awk -f $S/tools/makeobjops.awk $S/$m -h
		done
		for o in h p q; do awk -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -$o; done
		for o in $(grep -rhoE "opt_[a-z0-9_]+\.h" $S | sort -u) lkpi_iic_if.h; do : > $o; done
		printf '#define INET 1\n#define INET6 1\n' > opt_inet.h
		printf '#define INET6 1\n' > opt_inet6.h
		printf '#define SMP 1\n' > opt_global.h
	)
fi

# Instrument only generated build sources; upstream references remain unchanged.
python3 "$HERE/tools/instrument-ath10k.py" "$B/src/ath10k/core.c"
python3 "$HERE/tools/instrument-snoc.py" "$B/src/ath10k/snoc.c"

# --- compile
# snoc.c and qmi.c are built on the X01A's platform headers (lx10k/include) and its platform device
# (lx10k/src); HakiOS's lx_bus.c (ath11k's bus) is not built. ath/debug.c is not
# built either: wifi/lx has what of it is used.
DRIVER="core debug htc htt htt_rx htt_tx txrx wmi wmi-tlv bmi hw p2p swap ce mac snoc qmi qmi_wlfw_v01"
COMMON="main regd key hw dfs_pattern_detector dfs_pri_detector"
# wifi/lx/src/lx_bus.c is ath11k's bus (hal.h, pcic.h): ours takes its place
GLUE="lx_base lx_wpa"
CC="clang -target arm64-apple-ios16.5"
LD=$H/xnu-deps/host/bin/ld64
CFLAGS="-mkernel -mgeneral-regs-only -fno-builtin -fno-common -ffreestanding -fno-strict-aliasing
	-funsigned-char -nostdinc -std=gnu17 -O2 -g -w
	-D__FreeBSD__=15 -D_KERNEL -DKLD_MODULE -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h
	-I$X/include -I$W/include -I$B/gen -I$S -I$S/contrib/ck/include
	-I$S/compat/linuxkpi/common/include -I$S/compat/linuxkpi/dummy/include
	-include $S/compat/linuxkpi/common/include/linux/kconfig.h -DLINUXKPI_VERSION=70000"
source "$H/xnu-env.sh"
for h in lx qmi; do
	[ -f "$XNU_SRC/bsd/sys/hakios.$h.h" ] || { echo "build_wifi.sh: the kernel tree has no bsd/sys/hakios.$h.h" >&2; exit 1; }
done
DFLAGS="-I$B/src/ath10k -I$B/src -include $W/include/lx_prelude.h -include $X/include/lx10k_more.h -DCONFIG_ATH10K_SNOC -DCONFIG_ATH10K_CE
	-DKBUILD_MODNAME=\"ath10k\"
	-DLX_HAKIOS_LX_H=\"$XNU_SRC/bsd/sys/hakios.lx.h\" -DLX_HAKIOS_QMI_H=\"$XNU_SRC/bsd/sys/hakios.qmi.h\""
mkdir -p obj out
rm -f obj/*.err
compile() {		# compile SOURCE OBJECT [FLAGS]: only when the object is older than anything it was made of
	local src=$1 obj=$2
	shift 2
	if [ -f "$obj" ] && [ -f "$obj.d" ] && [ -z "$(find $(sed -e 's/^[^:]*://' -e 's/\\$//' "$obj.d") "$0" -newer "$obj" 2>/dev/null | head -1)" ]; then
		return
	fi
	$CC $CFLAGS "$@" -MMD -MF "$obj.d" -c "$src" -o "$obj" 2> "$obj.err" || rm -f "$obj"
}
for f in $DRIVER; do compile src/ath10k/$f.c obj/ath10k_$f.o $DFLAGS & done
for f in $COMMON; do compile src/$f.c obj/ath_$f.o $DFLAGS & done
for f in $(for g in $GLUE; do echo "$W/src/$g.c"; done) "$X"/src/*.c; do [ -f "$f" ] && compile "$f" obj/lx_$(basename "$f" .c).o $DFLAGS -I$W/src -I$X/src & done
wait
bad=0
for f in obj/*.err; do
	[ -s "$f" ] && [ ! -f "${f%.err}" ] && { echo "--- $(basename "$f" .o.err)"; grep -m4 -A2 "error:" "$f"; bad=$((bad + 1)); }
done
built=$(ls obj/*.o 2>/dev/null | wc -l)
echo "build_wifi.sh: $built objects built, $bad files that do not compile (obj/*.err)"
[ "$bad" -eq 0 ] || exit 1
OBJECTS=$(ls obj/*.o)

# --- one object, and what it still wants
KERNEL="memcmp memcpy memmove memset strcmp strncmp strlen strchr strrchr strlcpy strlcat bzero
	snprintf vsnprintf __stack_chk_fail __stack_chk_guard"
$LD -r -arch arm64 $OBJECTS -o out/all.o 2> out/ld.err || { cat out/ld.err; exit 1; }
llvm-nm -u --format=just-symbols out/all.o | sed 's/^_//' | sort > out/undefined.txt
: > out/stubs.txt
: > out/stubs.s
: > out/missing.txt
for s in $(cat out/undefined.txt); do
	case " $(echo $KERNEL) " in *" $s "*) continue ;; esac
	case $s in
	lx_os_*|qmi_*|hakios_qmi_*|section\$*) continue ;;
	ath10k_*|__ath10k_*|ath_*) ;;
	*) echo $s >> out/missing.txt; continue ;;
	esac
	echo $s >> out/stubs.txt
	printf '\t.globl _%s\n_%s:\n\tadrp x0, 1f@PAGE\n\tadd x0, x0, 1f@PAGEOFF\n\tb _lx_stub\n\t.cstring\n1:\t.asciz "%s"\n\t.text\n\t.p2align 2\n' \
		$s $s $s >> out/stubs.s
done
echo "build_wifi.sh: $(wc -l < out/stubs.txt) stubs (out/stubs.txt), $(wc -l < out/missing.txt) names nothing defines (out/missing.txt)"
[ -s out/missing.txt ] && exit 1
$CC -c out/stubs.s -o out/stubs.o
printf '_lx_prepare\n_lx_start\n_lx_stop\n_lx_transmit\n' > out/exported.txt
$LD -r -arch arm64 -exported_symbols_list out/exported.txt out/all.o out/stubs.o -o out/ath10k.o 2> out/ld.err ||
	{ cat out/ld.err; exit 1; }
rm -f out/all.o
echo "build_wifi.sh: out/ath10k.o: $(llvm-size out/ath10k.o | awk 'NR == 2 { print $1 + $2 }') bytes"
