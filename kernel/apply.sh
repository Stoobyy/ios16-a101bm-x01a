#!/bin/bash
# Put the X01A kernel changes (kernel/xnu-x01a.patch) into a HakiOS checkout, before or after
# its xnu/apply.sh:
#   kernel/apply.sh HAKIOS_DIR
# Then build as HakiOS says (xnu-deps/setup.sh, ./xnu-build.sh -j8 -k).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "$1" && pwd)
cd "$H"
# The Wi-Fi layer modifies files in the base patch. Detect the complete stack first,
# otherwise reverse-checking the base alone incorrectly tries to reapply it.
if patch --force -p1 -R --dry-run -s < "$HERE/kernel/xnu-wifi.patch" >/dev/null 2>&1; then
 echo "kernel/apply.sh: base + Wi-Fi overlays already applied"
else
 if patch --force -p1 -R --dry-run -s < "$HERE/kernel/xnu-x01a.patch" >/dev/null 2>&1; then
  echo "kernel/apply.sh: base X01A overlay already applied"
 else
  patch --batch --forward -p1 --dry-run < "$HERE/kernel/xnu-x01a.patch" >/dev/null
  patch --batch --forward -p1 < "$HERE/kernel/xnu-x01a.patch"
 fi
 patch --batch --forward -p1 --dry-run < "$HERE/kernel/xnu-wifi.patch" >/dev/null
 patch --batch --forward -p1 < "$HERE/kernel/xnu-wifi.patch"
fi
# the new drivers in the kernel's file list (that list is in xnu/hakios.patch, not in xnu/files)
SRC=${XNU_SRC:-$H/xnu-src/xnu-8796.121.2}
if [ -d "$SRC" ]; then
	cp xnu/files/bsd/dev/hakios.ilitek.c xnu/files/bsd/dev/hakios.buttons.c xnu/files/bsd/dev/hakios.input.c \
	    xnu/files/bsd/dev/hakios.goodix.c xnu/files/bsd/dev/hakios.mpss.c xnu/files/bsd/dev/hakios.wcn3990.c \
        xnu/files/bsd/dev/hakios.lx.c xnu/files/bsd/dev/hakios.wifi.c xnu/files/bsd/dev/hakios.sta.c \
        xnu/files/bsd/dev/hakios.lx10k.c "$SRC/bsd/dev/"
	cp xnu/files/bsd/sys/hakios.input.h "$SRC/bsd/sys/"
	cp xnu/files/osfmk/arm64/gic_v3_irq.c "$SRC/osfmk/arm64/"
	grep -q "hakios.ilitek.c" "$SRC/bsd/conf/files" ||
		sed -i 's#^bsd/dev/hakios.goodix.c\tstandard$#&\nbsd/dev/hakios.ilitek.c\tstandard#' "$SRC/bsd/conf/files"
	grep -q "hakios.wcn3990.c" "$SRC/bsd/conf/files" ||
		sed -i 's#^bsd/dev/hakios.ilitek.c\tstandard$#&\nbsd/dev/hakios.wcn3990.c\tstandard#' "$SRC/bsd/conf/files"
	grep -q "hakios.lx10k.c" "$SRC/bsd/conf/files" || \
        printf "\nbsd/dev/hakios.lx10k.c\tstandard\n" >> "$SRC/bsd/conf/files"
	grep -q "hakios.wcn3990.c" "$SRC/bsd/conf/files" && echo "kernel/apply.sh: done ($SRC)"
else
	echo "kernel/apply.sh: xnu/files patched; run xnu/apply.sh, then this again for the file list"
fi
