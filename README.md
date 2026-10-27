# ios16-a101bm-x01a

iOS 16.5 on the BALMUDA Phone (A101BM, Kyocera X01A, Snapdragon 765). It's a port of
[HakiOS](https://github.com/oosexgang/hackios-xnu-kernel), which runs Apple's open-source XNU
kernel and the stock iOS user space on Qualcomm phones, from its POCO X5 Pro to this phone.

Nothing is flashed. Everything boots from RAM with `fastboot boot`, and a reboot puts the phone
back in Android.

## Status

Works:

- boots to the home screen, all 8 cores (about 80 s from launchd to SpringBoard)
- touch screen, multitouch
- power, volume up and volume down
- battery level (read once at boot), local time zone
- a clean boot screen: no text, just the logo, then iOS
- the picture in its real proportions (iOS thinks it's an iPhone X, 1125x2436, so it's drawn at
  887x1920 in the middle of the 1080x1920 panel)

Doesn't work yet: sound, Wi-Fi, Bluetooth, cellular, cameras, GPU (rendering is in software),
charging, and anything that needs an Apple account. See [NOTES.md](NOTES.md) for what was found on
the way, and what the next pieces would take.

## How it boots

```
ABL --fastboot boot--> Mu-Silicium UEFI --> HakiOS loader (XNU payload) --> XNU --> iOS from RAM
```

The UEFI is [mu-silicium-saipan](https://github.com/Stoobyy/mu-silicium-saipan), a Mu-Silicium
port to the SM7250 that stays separate from this repo. This repo adds HakiOS on top of it.

## What's in here

```
uefi/     HakiOS's UEFI hook, adapted: boot shim payload copy, XNU loader start, payload region
loader/   X01A patch for HakiOS's loader (xnuloader/) and a build script
kernel/   X01A patch for HakiOS's kernel (xnu/files) and an apply script
tools/    mkxnuimg.py (the boot image) and run.sh (one run, start to log)
```

What the patches change, compared to the POCO:

- The SM7250 has the CPU timer on PPI 1/2/3/0, so the virtual timer is INTID 19, not 27. The kernel
  takes it from `vtimer_intid=` and stops that timer before going back to UEFI. Without this XNU
  hangs right after the framebuffer driver starts.
- The loader has an `x01a` platform (framebuffer at 0xA0000000, 6 small and 2 big cores). Without it
  the loader takes the phone for QEMU. It passes `vtimer_intid=19 hakios_board=x01a` to the kernel,
  and has an `oem peek ADDR LEN` command for reading RAM over USB.
- Touch: a new driver for the Ilitek ILI7807S on I2C (`hakios.ilitek.c`). The bus's serial engine is
  set up for GPI DMA only, so the driver drives SDA and SCL by hand on GPIO 6 and 7 for now.
- Buttons: power and volume up on the PM8150's power-on block, volume down on GPIO5 of the PM8150L.
- The UEFI hands over to the XNU loader right after the Volume Up check instead of after its USB,
  PXE and disk attempts. Holding Volume Up still opens the boot manager.
- The framebuffer can draw the display in its own proportions (`fb_fit=1`, which the loader passes
  here), and touches map into the same rectangle.
- The loader reads the battery from this phone's own UEFI charger drivers, without restarting them,
  and keeps its text off the screen.

## Building

Linux or WSL. You need a HakiOS checkout with its kernel build working (theos with the iOS
toolchain and `iPhoneOS16.5.sdk`, see its README), and mu-silicium-saipan applied to a Mu-Silicium
tree.

```sh
# UEFI
mu-silicium-saipan/apply.sh ~/Mu-Silicium
uefi/apply.sh ~/Mu-Silicium

# kernel
kernel/apply.sh ~/hakios                  # patches xnu/files
(cd ~/hakios && xnu/apply.sh && xnu-deps/setup.sh)
kernel/apply.sh ~/hakios                  # again, for the kernel's file list
(cd ~/hakios && ./xnu-build.sh -j12 -k)

# iOS images and the loader's identity, from Apple's IPSW
(cd ~/hakios && sysimage/fetch.sh && xnuloader/mkidentity.sh)
tools/build_userland.sh ~/hakios Asia/Kolkata    # your time zone; HakiOS would take the PC's

# loader + UEFI boot image (RELEASE UEFI: no driver text on the screen; MODE=DEBUG for it)
cd ~/Mu-Silicium && python3 build_uefi.py -d x01a -r RELEASE && cd -
loader/build.sh ~/hakios ~/Mu-Silicium    # -> out/xnu-usb.img
```

`xnu-build.sh` ends with errors about `.symbolset` files. Those are kext export lists, and the
kernel itself is fine. Its lldb macros want `python`, so on Ubuntu install `python-is-python3`.

## Running

```sh
tools/run.sh ~/hakios 240
```

That boots the loader, uploads the 2.4 GB system image into RAM, and runs iOS for 240 seconds.
Then it brings back the log. The phone has to be unlocked (`fastboot boot` allowed). The kernel
gets `cpus=8 touchscreen=1 buttons=1 battery=1`; `ARGS="... -v"` shows the boot text instead.
`RESIDENT=1` skips the upload when the loader is still up from the last run.

Things to know:

- Don't touch USB while the kernel runs (no `fastboot getvar` polling). Windows resets a device that
  doesn't answer, and UEFI's USB driver can't recover from that once the kernel hands back.
- On Windows the loader shows up as `xnuloader` (18D1:D00D). The first time, pick "Android
  Bootloader Interface" for it in Device Manager.
- There's no watchdog on this phone, so if something hangs, hold power for about 15 seconds.
- The loader writes its log to the raw `logdump` partition, as HakiOS does on the POCO. On the X01A
  it's 64 MB of zeros. Back it up first anyway.

## Licenses

Each part keeps the license of what it changes: `kernel/` is APSL 2.0 (a derivative of XNU, like
HakiOS's `xnu/`), `loader/` and the scripts are MIT (like HakiOS's loader and tools), and `uefi/` is
BSD-2-Clause-Patent (Mu-Silicium). No Apple files are in this repo. The
iOS images and the loader's identity are made on your machine from Apple's IPSW. Not affiliated
with Apple, BALMUDA or Kyocera.
