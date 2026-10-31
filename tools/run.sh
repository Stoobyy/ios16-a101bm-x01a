#!/bin/bash
# One run of iOS on the phone, from Android or the bootloader, and back with the log:
#   tools/run.sh HAKIOS_DIR [SECONDS]
# Boots out/xnu-usb.img (loader/build.sh) with fastboot, uploads HAKIOS_DIR/userland/system.apfs
# into the phone's RAM (about 90 s), stages the kernel with userland/root.apfs, runs it for
# SECONDS (default 240), then fetches the log into out/logs/.
#   ARGS="..."    the kernel's arguments (default: cpus=8 touchscreen=1 buttons=1; add -v for the boot text)
#   RESIDENT=1    the loader is up and still has the system image: skip the upload
#   KERNEL=FILE   another kernel than HAKIOS_DIR's build
#   WIFI=1        tested X01A auto-join + picker mode; WIFI_MAC or private/wifi.mac required
#   ROOT_APFS, SYSTEM_IMAGE, MKPAYLOAD, LOADER_IMAGE: optional local input overrides
#   DRY_RUN=1     check inputs/arguments without touching USB
# Works from Linux and from Git Bash on Windows (fastboot and adb on the PATH, python3 or python).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "$1" && pwd)
SECS=${2:-240}
case "$SECS" in ''|*[!0-9]*) echo "run.sh: SECONDS must be a positive integer" >&2; exit 1;; esac
[ "$SECS" -gt 0 ] || { echo "run.sh: SECONDS must be positive" >&2; exit 1; }
if [ "${WIFI:-0}" = 1 ]; then
 MAC=${WIFI_MAC:-$(cat "$HERE/private/wifi.mac" 2>/dev/null || true)}
 MAC=$(printf '%s' "$MAC" | tr -d '\r\n')
 [[ "$MAC" =~ ^([[:xdigit:]]{2}:){5}[[:xdigit:]]{2}$ ]] || { echo "run.sh: set WIFI_MAC to this phone's permanent WLAN MAC (wifi/README.md)" >&2; exit 1; }
 ARGS="$(tr '\n' ' ' < "$HERE/modem/soc.args") ${ARGS:-cpus=8 touchscreen=1 buttons=1 modem_step=10 modem_pause=0 modem_listen=30 modem_keep=1 wlan_ath10k=1 wifi_station=1 wifi_join=1 wifi_mac=$MAC}"
fi
ARGS=${ARGS:-cpus=8 touchscreen=1 buttons=1}
case " $ARGS " in *" battery=1 "*) echo "run.sh: battery=1 prevents reliable USB return on X01A; remove it" >&2; exit 1;; esac
ROOT_APFS=${ROOT_APFS:-$(if [ "${WIFI:-0}" = 1 ]; then echo "$HERE/out/wifi-root.apfs"; else echo "$H/userland/root.apfs"; fi)}
SYSTEM_IMAGE=${SYSTEM_IMAGE:-$(if [ -f "$H/userland/system.apfs" ]; then echo "$H/userland/system.apfs"; else echo "$HERE/out/system.apfs"; fi)}
MKPAYLOAD=${MKPAYLOAD:-$(if [ -f "$H/xnuloader/mkpayload.py" ]; then echo "$H/xnuloader/mkpayload.py"; else echo "$HERE/out/mkpayload.py"; fi)}
LOADER_IMAGE=${LOADER_IMAGE:-$HERE/out/xnu-usb.img}
# The kernel's boot-args hold 607 characters, and the loader puts about 95 of its own in front
# (debug, rd, vtimer_intid, hakios_board, fb_fit, ufs_shared_pa). What doesn't fit is cut off the
# end without a word: that once dropped modem_ipa=0, and the phone hung.
CMDLINE="loader_return_after=$SECS epoch=0000000000 sb_delay=8 $ARGS"
if [ ${#CMDLINE} -gt 500 ]; then
	echo "run.sh: the arguments are ${#CMDLINE} characters, more than the 500 that fit after the loader's own: shorten them"
	exit 1
fi
KERNEL=${KERNEL:-$(if [ -f "$HERE/out/kernel" ]; then echo "$HERE/out/kernel"; else echo "$H/xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837"; fi)}
# (on Windows "python3" can be the Store's stand-in, which only says it is not installed)
for PY in python3 python py ""; do
	[ -n "$PY" ] && "$PY" -c "import sys" >/dev/null 2>&1 && break
done
[ -n "$PY" ] || { echo "run.sh: no working python3/python on the PATH"; exit 1; }
for file in "$KERNEL" "$ROOT_APFS" "$SYSTEM_IMAGE" "$MKPAYLOAD" "$LOADER_IMAGE"; do
 [ -f "$file" ] || { echo "run.sh: missing input: $file" >&2; exit 1; }
done
if [ "${DRY_RUN:-0}" = 1 ]; then
 echo "run.sh: inputs verified; boot arguments (${#CMDLINE} characters): $CMDLINE"
 exit 0
fi
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
		timeout 60 fastboot boot "$LOADER_IMAGE" >/dev/null
		for i in $(seq 1 30); do sleep 2; is_loader && break; done
		is_loader || { echo "run.sh: the loader did not come up on USB"; exit 1; }
	fi
	echo "run.sh: uploading the system image (about 90 s)"
	timeout 10 fastboot oem ramdisk >/dev/null
	timeout 1800 fastboot stage "$SYSTEM_IMAGE"
fi

"$PY" "$MKPAYLOAD" -o "$OUT/run-payload.bin" kernel="$KERNEL" ramdisk="$ROOT_APFS" \
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
