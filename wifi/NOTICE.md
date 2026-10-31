# Source provenance and licenses

The shared station/WPA and LinuxKPI integration originate in HakiOS. `lx10k/src/lx_80211.c`
is the tested HakiOS station source with local startup checkpoints; the X01A bootstrap lives
in `lx10k_platform.c`. `userland/net_test.m` extends HakiOS's MIT-licensed diagnostic helper.
The HakiOS license text is retained in `LICENSE.HakiOS`.

Kernel changes are derivative XNU source under APSL 2.0; see `../kernel/LICENSE.APSL` and
existing notices. New integration scripts and the platform layer follow HakiOS's MIT terms.
Existing per-file notices remain in place.

Linux ath10k/common ath source is extracted at build time from the checksum-pinned archive;
its ISC and BSD-3-Clause-Clear notices are retained and checked. FreeBSD LinuxKPI is also
extracted at build time, retaining its per-file notices and source COPYRIGHT. These upstream
trees and generated objects are not vendored into this repository.

Apple iOS images, IPSW-derived userland, modem firmware, board calibration, modemst data,
credentials and device logs are not distributed. Each user supplies their own local inputs.
