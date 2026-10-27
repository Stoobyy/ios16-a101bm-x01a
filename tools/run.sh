#!/bin/bash
# One run of iOS on the phone, from Android or the bootloader, and back with the log:
#   tools/run.sh HAKIOS_DIR [SECONDS]
# Boots out/xnu-usb.img (loader/build.sh) with fastboot, uploads HAKIOS_DIR/userland/system.apfs
# into the phone's RAM (about 90 s), stages the kernel with userland/root.apfs, runs it for
# SECONDS (default 240), then fetches the log into out/logs/.
#   ARGS="..."    the kernel's arguments (default: cpus=8 touchscreen=1 buttons=1 battery=1; add -v for the boot text)
#   RESIDENT=1    the loader is up and still has the system image: skip the upload
#   KERNEL=FILE   another kernel than HAKIOS_DIR's build
# Works from Linux and from Git Bash on Windows (fastboot and adb on the PATH, python3 or python).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "$1" && pwd)
SECS=${2:-240}
ARGS=${ARGS:-cpus=8 touchscreen=1 buttons=1 battery=1}
KERNEL=${KERNEL:-$H/xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837}
# (on Windows "python3" can be the Store's stand-in, which only says it is not installed)
for PY in python3 python py ""; do
	[ -n "$PY" ] && "$PY" -c "import sys" >/dev/null 2>&1 && break
done
[ -n "$PY" ] || { echo "run.sh: no working python3/python on the PATH"; exit 1; }
OUT=$HERE/out
mkdir -p "$OUT/logs"

is_loader() { timeout 8 fastboot getvar product 2>&1 | grep -q 'product: xnuloader'; }

if [ -z "$RESIDENT" ] || ! is_loader; then
	if ! is_loader; then
		if [ "$(adb get-state 2>/dev/null)" = device ]; then
			echo "run.sh: Android -> bootloader"
			adb reboot bootloader
		fi
		for i in $(seq 1 45); do fastboot devices | grep -q fastboot && break; sleep 2; done
		echo "run.sh: booting the loader"
		timeout 60 fastboot boot "$OUT/xnu-usb.img" >/dev/null
		for i in $(seq 1 30); do sleep 2; is_loader && break; done
		is_loader || { echo "run.sh: the loader did not come up on USB"; exit 1; }
	fi
	echo "run.sh: uploading the system image (about 90 s)"
	timeout 10 fastboot oem ramdisk >/dev/null
	timeout 1800 fastboot stage "$H/userland/system.apfs"
fi

"$PY" "$H/xnuloader/mkpayload.py" -o "$OUT/run-payload.bin" kernel="$KERNEL" ramdisk="$H/userland/root.apfs" \
    --cmdline "loader_return_after=$SECS epoch=$(date +%s) sb_delay=8 $ARGS" >/dev/null
timeout 60 fastboot stage "$OUT/run-payload.bin" >/dev/null
timeout 20 fastboot oem run >/dev/null
echo "run.sh: iOS is starting ($ARGS); back in $SECS s"
# Nothing on USB until the kernel is back: an unanswered device gets reset by the host, and UEFI's
# USB driver does not recover from that once the kernel hands back.
sleep $((SECS + 20))
r=$(timeout 15 fastboot getvar result 2>&1 | grep '^result:' || true)
echo "run.sh: ${r:-no answer from the loader (hold power ~15 s to reset)}"
if [ -n "$r" ]; then
	log="$OUT/logs/$(date +%Y%m%d-%H%M%S).log"
	timeout 60 fastboot get_staged "$log.raw" >/dev/null
	tr -d '\0' < "$log.raw" > "$log" && rm -f "$log.raw"
	echo "run.sh: log in $log (fastboot reboot: back to Android)"
fi
