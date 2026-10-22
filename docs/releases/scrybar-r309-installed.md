# ScryBar r309: original-device installation

Date: 2026-10-22. Device installation is separate from public firmware release.

- The owner confirmed ScryBar connected by USB and Meguru powered separately.
  USB ownership was coordinated with the Meguru chat; no Meguru serial access.
- Identified the original ESP32-S3, 16 MiB flash and unencrypted/unsecured
  download mode. The service-name identity follows `buildScryBarMdnsIdentity`:
  `ESP.getEfuseMac()` receives the MAC bytes into a little-endian integer, so
  the byte-8/byte-0 shifts select printed MAC bytes 1/0, not the final octets.
- Read and preserved a private full 16 MiB rollback backup. Its partition table
  matched the candidate. The Arduino-bundled esptool 5.1 read path stalled;
  already available esptool 5.4 completed backup and readback. No dependencies
  or global toolchains were installed or changed.
- Wrote only the application at `0x10000`: 6,746,400 bytes. Preserved bootloader,
  partition table, OTA data, NVS and filesystem partitions. No full erase or
  eFuse changes.
- Before first boot, reread all 16 MiB. Application bytes matched exactly;
  every byte outside the erased application sectors matched the original backup.
  NVS and partition table were also checked explicitly.
- First boot: one `r309` marker in 45 seconds, no panic/reset/watchdog markers;
  LVGL initialized at 640 × 172 and 35 real Mac Stats posts were observed.
  A real serial framebuffer confirmed the compact hardware/network layout.
- The optional FAT mount probe reports failure. Filesystem bytes are preserved
  exactly, and current firmware has no filesystem consumer beyond that probe;
  no formatting or repair was attempted.

Network-rate acceptance is tracked with Companion: initial physical testing
showed hardware live but network rates unavailable due to the Companion 0.3.11
collector becoming stale. This is not a complete network-display PASS until the
Companion correction is installed and real rates are observed.

Private backups, build artifacts, serial logs and framebuffer captures remain
outside version control. No local credentials, device addresses or identifying
screenshots are published here. Stable public firmware Latest remains v0.9.4.
