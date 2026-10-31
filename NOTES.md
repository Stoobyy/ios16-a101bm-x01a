# Notes

What was found while bringing this up, for whoever picks up the next piece.

## The phone

- SM7250 (soc_id 400, "lito" / "Saipan"), board MIZUKI04 rev50, 6 GB in three DDR banks.
- UEFI runs at EL1 under Qualcomm's hypervisor (QHEE). The "Haven" virtual watchdog SMCs that the
  POCO uses return -1 here, so there is no watchdog: a hang needs a 15 s press of power.
- Free RAM under UEFI: the largest block is 0xF4000000-0x200000000 (4.2 GB). The XNU payload sits
  at 0xF0000000 (64 MB), and the loader at 0xA8000000.
- RAM does not survive a Power + Volume Down reset (checked with `oem peek` after a hang).

## Things that were different from the POCO

| What | POCO X5 Pro | BALMUDA Phone |
|---|---|---|
| Virtual timer | INTID 27 | INTID 19 (PPIs 1/2/3/0) |
| Framebuffer | 0xE1000000 | 0xA0000000, 1080x1920, stride 1080, BGRx |
| Touch | Goodix / FocalTech on SPI | Ilitek ILI7807S on I2C 0x41 (SE7, 0x984000) |
| Power-on block | PMK8350 0x1300 (gen3) | PM8150 0x800 (gen2): KPDPWR, RESIN = volume up |
| GPIO key | PM7325 GPIO6 = volume up | PM8150L GPIO5 = volume down |
| Battery | through the audio DSP | the UEFI's own QcomCharger / ChargerEx |
| Wi-Fi | WCN6750 (ath11k) | WCN3990 (ath10k SNOC, ICNSS) |

## Kernel and loader

- The touch I2C serial engine is set up by the firmware for GPI DMA only (GENI_IF_DISABLE_RO bit
  0), so the driver drives GPIO 6/7 by hand. 2551 reports in one run, 0 failed reads. A GPI DMA
  driver would let it use the engine instead.
- The touch chip runs from its own flash (Android's driver finds the firmware current), protocol
  5.6.0, 43-byte reports, coordinates 0-2047, customer type 0x1F (no info bytes), low resolution.
- The secondary cores start fine when the kernel starts them itself (`cpus=8`). Starting the kernel
  on another core from the loader (`bootcpu=N`) does not work here: the core arrives at the
  loader's entry and jumps, then the kernel says nothing. Not needed, left alone.
- The input drivers start when /dev is mounted, after the root file system. A kernel-only run (no
  system image) never gets to them.
- The battery is read once, when the kernel starts (the loader is on the same core as the kernel,
  so it can't poll during the run). QcomCharger's `initialize()` is not called on this phone, so
  the charger is left as the firmware set it.
- iOS has to be told the iPhone X's 1125x2436, or SpringBoard has no layout. `fb_fit=1` draws it in
  its own proportions (887x1920 in the middle), and touches map into the same rectangle.

## Things to know when running

- No USB traffic while a kernel runs on core 0. Windows resets the device that doesn't answer, and
  UEFI's DWC3 driver then can't recover after the kernel hands back ("Cannot Handle Reset ...
  USB_STATE_BYPASS_STATUS"). `tools/run.sh` waits quietly.
- The last 240 s run with `battery=1` and the quiet loader came up fine on screen, but the loader
  did not answer on USB afterwards. Not looked into yet. The runs before it, with the same steps,
  returned fine.
- The time zone comes from the image build (`tools/build_userland.sh HAKIOS_DIR Asia/Kolkata`).

## Next pieces, roughly by size (historical list; Wi-Fi update below)

- Sound, speaker only: the audio DSP (APR protocol on this SoC), then the NXP TFA98xx amplifier on
  I2C (i2c@880000, 0x34). The headphone side is a WCD938x on SoundWire behind the Bolero codec.
- GPU: Adreno 620 against HakiOS's Adreno 642L driver, same family, every address different.
- Wi-Fi: the WCN3990's firmware runs on the modem DSP, so the modem has to be brought up first,
  then an ath10k SNOC port through HakiOS's Linux shim.

## Wi-Fi integration, 2026-10-31

The formerly separate GLM/Codex WCN3990 work is integrated in `kernel/xnu-wifi.patch`,
`wifi/` and `tools/build_wifi*`. `kernel/apply.sh` applies both kernel layers; `tools/run.sh`
is the common launcher. The old experimental folder is not a build dependency.

Hardware-verified: scanning, WPA2 association/four-way handshake, key installation, DHCP,
global IPv4/DNS selection, Safari, and the Settings network picker. The successful picker
path auto-joins first, then offers the existing station callbacks to iOS. Bootstrap retries
at most three times on ETIMEDOUT. Unjoined station-only mode remains unstable and is not the
recommended mode. Signal-strength display, switching/rejoin and long-duration stability
remain incomplete or unverified. A late USB-return failure is still possible.

The main fixes are the DMA mapper's missing output address, live SMMU SID mask/mappings,
early 44 MiB contiguous-pool reservation, CE pending-bit filtering/start gating and callback
lifetime synchronization. Stop the radio before its modem transport. The source checkpoints
remain available for diagnosis; a clean driver build has 29 objects, zero compile failures,
zero generated stubs and zero missing symbols.

On iOS, DHCP alone did not make en0 primary. The helper publishes a matching Setup service
using the actual DHCP service ID. CaptiveNetworkSupport held a configd-owned Never rank on
en0; excluding that plugin removed the restriction, enabling default TCP, DNS and HTTP200.
Captive-portal support is deliberately disabled at the owner's request. The private root image
contains a network credential and must never be committed or shared. See `wifi/README.md`.

`battery=1` remains excluded. Keep the 500-character boot-argument guard and all X01A modem
memory/IPA/NAV overrides. No automatic reboot/watchdog fix was added.
