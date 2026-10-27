#!/bin/bash
# HakiOS's REST=1 userland/build.sh with a time zone of your choice. HakiOS takes the zone from the
# build machine (timedatectl), which in WSL is UTC; this hands it the one you name instead.
#   tools/build_userland.sh HAKIOS_DIR [ZONE]      e.g. Asia/Kolkata (default: the machine's)
set -e
H=$(cd "$1" && pwd)
ZONE=${2:-$(timedatectl show -p Timezone --value 2>/dev/null || echo UTC)}
SHIM=$(mktemp -d); trap 'rm -rf "$SHIM"' EXIT
printf '#!/bin/sh\necho %s\n' "$ZONE" > "$SHIM/timedatectl"
chmod +x "$SHIM/timedatectl"
cd "$H"
PATH="$SHIM:$PATH" REST=1 bash userland/build.sh
grep -q "zoneinfo/$ZONE" userland/root/etc/var.skel && echo "build_userland.sh: time zone $ZONE"
