# X01A Wi-Fi

Tested on BALMUDA A101BM/X01A, iOS 16.5: saved WPA2 network auto-join, DHCP, DNS,
Safari, and the Settings network picker. The strength indicator is incomplete. Selecting a
different network, off/on rejoin, roaming and long-duration stability are not yet validated.
Captive-portal handling is deliberately disabled. Late USB-return failures remain possible.

## Source and build

The validated HakiOS baseline is commit `88513176b1e96ecf0b764cd62c6f4e6ac71345e3`.
Follow the root README to set up its XNU toolchain and apply this repo's kernel overlays.
This repo contains only port source/patches, not a full XNU tree, iOS images or device firmware.

Place these upstream archives in `downloads/`, or point `WLAN_DL` at their directory:

| Archive | SHA-256 |
| --- | --- |
| Linux `linux-7.0.14.tar.xz` | `de9999b784d2293f00d39c62d8f92a08ab8a54bc4e80ffd250a0c09cb07a0f98` |
| FreeBSD `freebsd-15.1-src.txz` (release src.txz, locally named) | `cf5762da53fd52e1eaf0f9ceee9bf58cbe314c821031d0d9ffa76823185a89e1` |

The build verifies both archives and checks driver source license headers. HakiOS's shared
LinuxKPI patches/glue come from that HakiOS checkout; this repo supplies the WCN3990 layer
and its tested station-source copy. Generated source and objects stay under ignored `out/`.

```sh
kernel/apply.sh ~/hakios
(cd ~/hakios && xnu/apply.sh && xnu-deps/setup.sh)
kernel/apply.sh ~/hakios
WLAN_DL=/path/to/archives tools/build_wifi.sh ~/hakios
tools/build_kernel.sh ~/hakios build -j12
```

The `build` target builds the kernel. The unrelated IOKit `_Dispatch` symbolset failure is
still unresolved. `WIFI_BUILD_DIR`, `ATH10K_OBJECT` and `X01A_XNU_SRC` can override output,
object or generated kernel locations when maintaining an isolated workspace.

## Firmware and private image

Set up this phone's own MPSS firmware/filesystems using [modem/README.md](../modem/README.md).
The system image must contain its modem firmware and `fw/image/bdwlan.bin`; no firmware or
modemst partitions are distributed here. The ath10k firmware-5 metadata is supplied by the
kernel integration. Do not combine the diagnostic `wcn3990=1` exchange with `wlan_ath10k=1`.

Create credentials locally. The password is prompted without echo or a command-line argument:

```sh
python3 tools/wifi_credentials.py private/network
```

The file has two lines, SSID and WPA2 passphrase. It and the resulting image are private.
For a new userland/system build:

```sh
WIFI=1 MODEM=1 SYSTEM=1 MODEM_DTB=/path/to/x01a-live.dtb \
  tools/build_userland.sh ~/hakios Asia/Kolkata
```

To derive only the Wi-Fi root image from an already built canonical root image:

```sh
python3 tools/build_wifi_userland.py ~/hakios --network-file private/network
```

This compiles the host-wrapper-based helper, restores every base inode's canonical metadata,
adds the delayed DHCP/Setup publication job and diagnostic replay, excludes only configd's
CaptiveNetworkSupport bundle, and writes `out/wifi-root.apfs`. The credential inode is root-owned
mode 0600. Do not replace manifest metadata with Windows staging permissions. The image and all
build/firmware/credential output are ignored by Git.

## Running

Use the phone's permanent WLAN MAC, not the router BSSID or Android's randomized connection MAC.
Read it while Android is connected and keep it in the ignored private directory:

```sh
mkdir -p private
adb shell cat /sys/class/net/wlan0/address | tr -d '\r' > private/wifi.mac
WIFI=1 tools/run.sh ~/hakios 360
```

`WIFI_MAC=xx:xx:xx:xx:xx:xx` is an alternative to the file. `WIFI=1` adds all required X01A
modem overrides and selects `wifi_station=1 wifi_join=1`: auto-join first, then publish iOS
station callbacks. A bootstrap association timeout is retried up to three times; other errors
are reported. `wifi_station=1` alone is an unfinished diagnostic path and may go black.

Build `out/xnu-usb.img` as the root README describes. `KERNEL`, `ROOT_APFS`, `SYSTEM_IMAGE`,
`MKPAYLOAD` and `LOADER_IMAGE` can point to locally staged copies. The kernel builder also
stages `out/kernel`. For a self-contained Git Bash runtime bundle, run this in WSL after builds:

```sh
tools/stage_runtime.sh ~/hakios
```

Then run `WIFI=1 tools/run.sh . 360` from this repository in Git Bash. The bundle under ignored
`out/` contains system/root images, kernel and payload tool; it has no dependency on the old
GLM folder. Keep its private images local.
`ARGS` replaces the default device arguments; retain all Wi-Fi flags and the MAC if overriding it.
`DRY_RUN=1` verifies inputs/argument length without touching USB. `RESIDENT=1` skips a system
upload only while the loader still holds that image. Reboot through Android between driver tests
to reset modem state. `battery=1` is rejected; keep arguments within 500 characters. Do not poll
USB during iOS. A hang can still need a 15-second power hold.

## Implementation

`kernel/xnu-wifi.patch`: early pool reservation; X01A SMMU/DMA and firmware binding; modem
startup/stop hooks; station checkpoints. `lx10k/`: ath10k platform compatibility and station
bootstrap. The generated SNOC transformation also gates CE polling until RX posting completes;
it is required behavior, not merely logging. CE worker/IRQ teardown waits for active callbacks.

`userland/net_test.m`: bounded DHCP request, real DHCP service-ID reuse, temporary Setup
publication, route/DNS/HTTP diagnostics and rank inspection. `ip_replay.c` replays RAM-only
helper output. No helper writes credentials or network settings to Android storage.
