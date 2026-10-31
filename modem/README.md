# Modem and WCN3990 firmware bring-up

**2026-10-31 update:** the data path and iOS integration now work. Use
[the unified Wi-Fi workflow](../wifi/README.md). The sections below describe the earlier
firmware-control bring-up; `wcn3990=1` is a diagnostic alternative, not an option to combine
with the working `wlan_ath10k=1` path.

On the X01A the Wi-Fi firmware (WCN3990) runs inside the modem, so Wi-Fi starts with the modem.
HakiOS already boots the POCO's modem (`bsd/dev/hakios.mpss.c`). The X01A's numbers are almost all
the same.

Status (2026-10-29): the modem boots, serves its files, starts its Wi-Fi domain, loads
`wlanmdsp.mbn` and offers the Wi-Fi firmware's service (QRTR service 69, WLFW). 45 services in all,
and the phone keeps running. With `wcn3990=1` the kernel then powers the radio and has ath10k's
start-up exchange with the firmware (`hakios.wcn3990.c`), and the firmware says "firmware ready".

## What differs from the POCO

| What | POCO X5 Pro | X01A |
|---|---|---|
| Modem memory | 0x8b800000, 0xf600000 bytes | 0x8c000000, 0xf800000 bytes (`modem_wlan_region`) |
| RMTFS buffer | 0x9c700000 (its CDSP region) | 0x9c700000 too, which here is plain RAM in the gap 0x9b800000-0x9f400000 that neither UEFI nor the kernel uses. TrustZone refuses the CDSP region (0x87400000) with -2. |
| Buffer owners | HLOS, MSS, NAV | HLOS and MSS only (`modem_no_nav=1`). With NAV TrustZone says -2. |
| GLINK queue 479 | made by the boot chain | not there: the kernel makes it (16 KB, as Linux does) |
| Modem file systems | read from UFS (`ufs=2`) | copies in /fw (see below). `ufs=2` needs the loader on another core, which this phone can't do. |
| Wi-Fi domain | no such file | `modemuw.jsn` (msm/modem/wlan_pd), now in the registry's list |
| IPA | 4.11, set up by the kernel | 4.7 (`qcom,ipa-hw-ver = <0x12>`). The 4.11 setup hangs the whole phone, so it is kept out (`modem_ipa=0 modem_no_ipa_qmi=1 modem_ipa_smmu_off=1`). |
| IPA stream IDs | 0x480 / 0x482 | 0x520 / 0x522 |

The same on both: SMEM 0x80900000, command DB 0x80860000, AOSS 0xc300000, IPCC 0x408000 (client 2),
SMP2P items 435/428, TCSR mutex 3, TrustZone image 4, UFS 0x1d84000.

`kernel/xnu-x01a.patch` has the kernel side: `soc_mpss_size`, the GLINK queue, the /fw copies of the
file systems, and `modemuw.jsn`. Every boot-arg is in `soc.args`.

## Firmware

From the phone (root; Magisk's `su` can be at `/debug_ramdisk/su` right after a boot), into the
HakiOS checkout. The four partitions hold the modem's own data (IMEI and so on): keep them local.

```sh
adb exec-out 'su -c "tar -cf - -C /vendor/firmware_mnt image"' > firmware_mnt.tar
# in WSL: unpack, then modem.mdt and modem.b* go to HAKIOS_DIR/fw-modem/, the whole image/ to fw-modem/image/
for p in modemst1 modemst2 fsg fsc; do adb exec-out "su -c 'cat /dev/block/by-name/$p'" > HAKIOS_DIR/fw-modem/$p; done
MODEM=1 SYSTEM=1 MODEM_DTB=X01A/x01a-live.dtb tools/build_userland.sh HAKIOS_DIR Asia/Kolkata
```

What the modem writes to its file systems stays in RAM. Nothing is written to the phone's storage.

## Run

Copy `system.apfs`, `root.apfs`, the kernel and `xnuloader/mkpayload.py` to a local folder first
(same layout). The first upload straight from the WSL share broke on USB.

```sh
ARGS="cpus=8 touchscreen=1 buttons=1 modem_step=10 modem_listen=30 wcn3990=1 $(tr '\n' ' ' < modem/soc.args)" tools/run.sh LOCAL_DIR 220
```

Look for `mpss: qrtr: a service: 69 (0x45)` and `wcn3990: ... THE WI-FI FIRMWARE IS READY`. The
modem is stopped after the last step. `soc.args` holds only what differs from HakiOS's built-in
values, and the IPA switches come first.

- A modem that crashed (it didn't acknowledge the stop) is not started again. Reboot to Android
  first (`fastboot reboot`).
- If the phone goes black and drops off USB, it is a hard hang. Hold power 15 s. The log is lost
  with it.

## The Wi-Fi firmware

`hakios.wcn3990.c`, after step 10 with `wcn3990=1`. What the phone said:

| Step | What happens |
|---|---|
| 0 | The radio's supplies on through RPMh: ldoa1, ldoa14 1.8 V, ldoc2 1.304 V, ldoc10 and ldoc11 3.0 V. Left on after the run. |
| 1 | Indications registered. Status word 0. |
| 2 | MSA 0x8ba00000, 2 MB: the firmware makes a 64 KB secure region and the rest. TrustZone refuses every move of them (-2, 0x56). Android's icnss doesn't move them either ("hyperviser disabled"), so this is passed over. |
| 3 | "MSA ready". |
| 4 | Chip 0x320 of family 0x4001, no board id, firmware WLAN.HL.3.3.1-01557 (13 Oct 2022). |
| 5 | `bdwlan.bin` (26 KB) in 5 pieces, then an empty calibration report. Without the report the firmware never gets ready. |
| 6 | The pins' result (power 0x1, RF 0x1822b), then "firmware ready" 5.8 s after the report. Android takes 5.5 s. |
| 7 | The WLAN configuration (ath10k's 12 copy engines, 21 services on them, 12 shadow registers) and mode "mission", both taken. The radio is on; 2 s later mode "off", also taken. |

Step 7 uses ath10k's own message descriptions and WCN3990 tables. They come from Linux 7.0.14's
`drivers/net/wireless/ath/ath10k` through `tools/mk-wcn3990-tables.sh`, which writes them as a header
into the kernel's build directory. Linux's text is not kept in this repository. Without the header
the kernel still builds and step 7 says it is left out. After a first kernel build:

```sh
LINUX_SRC=path/to/linux-7.0.14 tools/mk-wcn3990-tables.sh HAKIOS_DIR    # cdn.kernel.org, sha256 de9999b7...
./xnu-build.sh -j12 -k                                                  # in HAKIOS_DIR, again
```

Boot-args: `wcn3990_upto=N` stops after step N, `wcn3990_power=0` leaves the supplies alone,
`wcn3990_marks=1` paints a band on the screen at each point (for runs that hang and lose the log).

The kernel's boot-args hold 607 characters and the loader adds about 95. `tools/run.sh` refuses
more than 500 of ours, after a cut-off line once dropped `modem_ipa=0` and hung the phone.

## Original next steps (completed by the Wi-Fi integration)

1. The data path: ath10k SNOC copy engines at 0x18800000, behind the SMMU, then HTC, WMI and HTT
   over them. HakiOS has ath11k only. This is the big piece.
2. The iOS side, `hakios.apple80211.c`, should mostly carry over.
