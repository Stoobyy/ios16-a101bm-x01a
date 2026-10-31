# Integration validation (2026-10-31)

Hardware baseline: owner confirmed the Settings network picker and Safari both work using
`wifi_station=1 wifi_join=1`, captive-network plugin excluded, and bounded bootstrap retry.
Earlier captured logs independently verified WPA2 four-way handshake/keys, DHCP, global en0
IPv4/DNS selection, default TCP and CFNetwork HTTP200. Signal display remains incomplete.

For this repository consolidation:

- Base and Wi-Fi patches apply to a pristine HakiOS checkout at the documented commit.
- Reapplication is idempotent, including generated-source synchronization and the new
  `hakios.lx10k.c` kernel file-list entry.
- All five modified/added kernel source files match the tested GLM tree.
- Driver rebuilt from checksum-pinned archives: 29 objects, zero compile failures, generated
  stubs or missing symbols. Unified kernel-only build passed.
- Repackaged root image matches every verified file byte, owner/group and symlink. Only the
  new `/fw` directory permission is tightened from staging permissions to 0755; network file
  stays root-owned 0600. Canonical base metadata is retained.
- Shell/Python syntax and no-USB launcher dry-run passed; battery=1 and oversized argument
  lists are rejected before touching the device.
- Private network file, images, firmware, logs and build output are excluded from Git.

The consolidation was validated by source/image equivalence and build checks; it was not a
new physical phone boot. The last successful picker run did not return its USB log. Do not
interpret UI success as evidence that all shutdown/recovery paths are fixed.

## Remaining limitations

Unjoined station-only mode previously went black and is not the default Wi-Fi workflow.
Network switching, off/on rejoin, roaming and long-duration stability remain unverified.
Captive portals are intentionally unsupported, strength display is incomplete, and late USB
handoff can still fail. No automatic watchdog reset is available on this Saipan setup.

The compatibility layer uses software CE interrupt polling and partial Linux NAPI semantics;
TXQ helpers are incomplete. Basic verified traffic is not a claim of full mac80211 compatibility.
The radio uses a bounded identity DMA mapping, unlike Android's separate IOVA pool; current
mapping limits/stream mask match the tested configuration. These gaps remain engineering work.
