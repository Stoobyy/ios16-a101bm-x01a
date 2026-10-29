# Modem (first step towards Wi-Fi)

On the X01A the Wi-Fi firmware (WCN3990) runs inside the modem, so Wi-Fi starts with the modem.
HakiOS already boots the POCO's modem (`bsd/dev/hakios.mpss.c`). The X01A's numbers are almost all
the same.

Status (2026-10-29): the modem boots, serves its files, starts its Wi-Fi domain, loads
`wlanmdsp.mbn` and offers the Wi-Fi firmware's service (QRTR service 69, WLFW). 45 services in all,
and the phone keeps running. Nothing talks to the Wi-Fi firmware yet.

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
ARGS="cpus=8 touchscreen=1 buttons=1 modem_step=10 modem_listen=30 $(tr '\n' ' ' < modem/soc.args)" tools/run.sh LOCAL_DIR 180
```

Look for `mpss: qrtr: a service: 69 (0x45)`. The modem is stopped after the last step.

- A modem that crashed (it didn't acknowledge the stop) is not started again. Reboot to Android
  first (`fastboot reboot`).
- If the phone goes black and drops off USB, it is a hard hang. Hold power 15 s. The log is lost
  with it.

## Next

1. WLFW: memory info, the board file (`bdwlan.*` in image/), mission mode. `hakios.wlfw.c` does this
   for the WCN6750.
2. The radio's supplies (the icnss node's `vdd-*`).
3. The data path: ath10k SNOC copy engines at 0x18800000, behind the SMMU. HakiOS has ath11k only.
4. The iOS side, `hakios.apple80211.c`, should mostly carry over.
