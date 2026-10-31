#!/bin/bash
# Stage a local RAM-boot bundle, including the private Wi-Fi image, for Git Bash on Windows.
# Usage: tools/stage_runtime.sh HAKIOS_DIR
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
H=$(cd "${1:?usage: stage_runtime.sh HAKIOS_DIR}" && pwd)
for file in "$H/userland/system.apfs" "$H/xnuloader/mkpayload.py" "$HERE/out/kernel" "$HERE/out/wifi-root.apfs"; do
 [ -f "$file" ] || { echo "stage_runtime.sh: missing input: $file" >&2; exit 1; }
done
cp "$H/userland/system.apfs" "$HERE/out/system.apfs"
cp "$H/xnuloader/mkpayload.py" "$HERE/out/mkpayload.py"
echo "stage_runtime.sh: bundle staged under out/; it contains private network credentials"
echo "Use WIFI=1 tools/run.sh . 360 from this repo (loader/build.sh must also create out/xnu-usb.img)."
