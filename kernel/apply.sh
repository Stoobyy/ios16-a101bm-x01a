#!/bin/bash
# Put the X01A kernel changes (kernel/xnu-x01a.patch) into a HakiOS checkout, before or after
# its xnu/apply.sh:
#   kernel/apply.sh HAKIOS_DIR
# Then build as HakiOS says (xnu-deps/setup.sh, ./xnu-build.sh -j8 -k).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "$1" && pwd)
cd "$H"
if patch -p1 -R --dry-run -s < "$HERE/kernel/xnu-x01a.patch" >/dev/null 2>&1; then
	echo "kernel/apply.sh: xnu/files already has the X01A changes"
else
	patch -p1 < "$HERE/kernel/xnu-x01a.patch"
fi
# the new driver in the kernel's file list (that list is in xnu/hakios.patch, not in xnu/files)
SRC=${XNU_SRC:-$H/xnu-src/xnu-8796.121.2}
if [ -d "$SRC" ]; then
	cp xnu/files/bsd/dev/hakios.ilitek.c xnu/files/bsd/dev/hakios.buttons.c xnu/files/bsd/dev/hakios.input.c \
	    xnu/files/bsd/dev/hakios.goodix.c "$SRC/bsd/dev/"
	cp xnu/files/bsd/sys/hakios.input.h "$SRC/bsd/sys/"
	cp xnu/files/osfmk/arm64/gic_v3_irq.c "$SRC/osfmk/arm64/"
	grep -q "hakios.ilitek.c" "$SRC/bsd/conf/files" ||
		sed -i 's#^bsd/dev/hakios.goodix.c\tstandard$#&\nbsd/dev/hakios.ilitek.c\tstandard#' "$SRC/bsd/conf/files"
	grep -q "hakios.ilitek.c" "$SRC/bsd/conf/files" && echo "kernel/apply.sh: done ($SRC)"
else
	echo "kernel/apply.sh: xnu/files patched; run xnu/apply.sh, then this again for the file list"
fi
