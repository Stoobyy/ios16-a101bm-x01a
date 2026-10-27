#!/usr/bin/env python3
"""
Pack the x01a UEFI (built after `apply.sh --hakios`) and an optional HakiOS payload into an
Android boot image for `fastboot boot`. The X01A version of HakiOS's mkxnuimg.py.

  kernel = gzip(BootShim + UEFI FD + ["XNUP" container]) + DTB

The boot shim copies the FD to its base and the payload to PAYLOAD_BASE.

usage: mkxnuimg.py MU_SILICIUM_DIR [-p payload.bin] [-o out.img] [--mode DEBUG|RELEASE]
"""
import argparse, gzip, os, subprocess, sys

FD_BASE, FD_SIZE = 0x9FC00000, 0x300000
PAYLOAD_BASE, PAYLOAD_MAX = 0xF0000000, 0x04000000

ap = argparse.ArgumentParser()
ap.add_argument('mu')
ap.add_argument('-p', '--payload')
ap.add_argument('-o', '--output', default='xnu-x01a.img')
ap.add_argument('--mode', default='DEBUG')
args = ap.parse_args()

mu = args.mu
shim_dir = os.path.join(mu, 'BootShim')
for f in ('BootShim.elf', 'BootShim.bin'):
    try: os.unlink(os.path.join(shim_dir, f))
    except FileNotFoundError: pass
subprocess.check_call(['make', 'REQUIRES_KERNEL_HEADER=1', f'FD_BASE={FD_BASE:#x}', f'FD_SIZE={FD_SIZE:#x}',
                       f'CFLAGS=--target=aarch64-linux-gnu -DPAYLOAD_BASE={PAYLOAD_BASE:#x}'],
                      cwd=shim_dir, stdout=subprocess.DEVNULL)

shim = open(os.path.join(shim_dir, 'BootShim.bin'), 'rb').read()
fd = open(os.path.join(mu, f'Build/x01aPkg/{args.mode}_CLANGPDB/FV/SILICIUM_UEFI.fd'), 'rb').read()
assert len(fd) == FD_SIZE, hex(len(fd))

blob = shim + fd
if args.payload:
    data = open(args.payload, 'rb').read()
    # a complete "XNUP" container (HakiOS xnuloader/mkpayload.py)
    assert data[:4] == b'XNUP' and len(data) <= PAYLOAD_MAX - 0x1000, 'bad or oversized payload'
    blob += data
    blob += b'\0' * (-len(blob) % 16)
    print(f'payload: {len(data)} bytes, bytesum={sum(data) & 0xffffffff:08x}')

dtb = open(os.path.join(mu, 'Resources/DTBs/x01a.dtb'), 'rb').read()
kernel = args.output + '.kernel~'
open(kernel, 'wb').write(gzip.compress(blob, 6) + dtb)
try:
    subprocess.check_call([sys.executable, os.path.join(mu, 'Resources/Scripts/mkbootimg.py'), '--kernel', kernel,
                           '--ramdisk', os.path.join(mu, 'Resources/ramdisk'), '--header_version', '1',
                           '-o', args.output])
finally:
    os.unlink(kernel)
print(f'{args.output}: {os.path.getsize(args.output)} bytes (shim {len(shim)}, uncompressed {len(blob)})')
