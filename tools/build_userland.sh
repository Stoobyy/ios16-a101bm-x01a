#!/bin/bash
# HakiOS's REST=1 userland/build.sh with a time zone of your choice. HakiOS takes the zone from the
# build machine (timedatectl), which in WSL is UTC; this hands it the one you name instead.
#   tools/build_userland.sh HAKIOS_DIR [ZONE]      e.g. Asia/Kolkata (default: the machine's)
# MODEM=1 SYSTEM=1 puts HAKIOS_DIR/fw-modem into the system image as HakiOS does, but checks it
# against MODEM_DTB (the X01A's own device tree) instead of the POCO's.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "$1" && pwd)
ZONE=${2:-$(timedatectl show -p Timezone --value 2>/dev/null || echo UTC)}
SHIM=$(mktemp -d); trap 'rm -rf "$SHIM"' EXIT
printf '#!/bin/sh\necho %s\n' "$ZONE" > "$SHIM/timedatectl"
chmod +x "$SHIM/timedatectl"
cd "$H"
BUILD=userland/build.sh
if [ -n "$MODEM" ] && [ -n "$MODEM_DTB" ]; then
	BUILD=userland/.build-x01a.sh
	trap 'rm -rf "$SHIM" "$H/$BUILD"' EXIT
	# ... and the modem's own file systems, copied from Android into fw-modem/ (modemst1, modemst2,
	# fsg, fsc), go to /fw too: the kernel serves them from there, as the X01A has no storage for it
	sed -e "s#--dtb ../devices/poco-x5-pro/prebuilt/device-tree.dtb#--dtb $(readlink -f "$MODEM_DTB")#" \
	    -e 's#^\(\t*\)cp ../fw-modem/modem.mdt ../fw-modem/modem.b\[0-9\]\[0-9\] sysroot/fw/$#&\n\1for p in modemst1 modemst2 fsg fsc; do [ -f ../fw-modem/$p ] \&\& cp ../fw-modem/$p sysroot/fw/; done#' \
	    userland/build.sh > "$BUILD"
	grep -q -- "--dtb $(readlink -f "$MODEM_DTB")" "$BUILD" || { echo "build_userland.sh: could not point the modem check at $MODEM_DTB"; exit 1; }
	grep -q "for p in modemst1" "$BUILD" || { echo "build_userland.sh: could not add the modem's file systems"; exit 1; }
fi
PATH="$SHIM:$PATH" REST=1 bash "$BUILD"
grep -q "zoneinfo/$ZONE" userland/root/etc/var.skel && echo "build_userland.sh: time zone $ZONE"

# The Wi-Fi image is derived from the canonical root image, retaining its manifest metadata.
if [ "${WIFI:-0}" = 1 ]; then
 python3 "$HERE/tools/build_wifi_userland.py" "$H" \
   --network-file "${WIFI_NETWORK_FILE:-$HERE/private/network}" \
   --output "${WIFI_ROOT_APFS:-$HERE/out/wifi-root.apfs}"
fi
