# Companion 0.3.13 (49): responsive Now Playing

Date: 2026-10-22. Corrects a music regression observed after the shared network
preview was installed. One Mac app continues to serve ScryBar and Meguru.

## Cause and correction

The system Now Playing reader returned the current permitted music correctly.
Its caller then synchronously resolved artwork before returning the metadata.
A native process sample showed repeated TIDAL cache enumeration, file metadata
sorting and content decoding on this path. A slow cover lookup therefore delayed
the app's song updates and the same feed sent to both displays.

The network changes did not edit this provider path. The installed release
exposed its blocking behavior; prior network/display acceptance did not include
an end-to-end current-music check. That coverage gap is recorded explicitly.

Metadata polling now reads a nonblocking artwork cache. One serial utility worker
owns cover resolution; the full track request, including album, must match before
the result can be used. TIDAL fallback scans have a time/work budget, bounded
file decoding and reusable catalogs. Late artwork remains deliverable after a
metadata-only first payload on both display transports. Source privacy gates
still precede cover lookup and the 150/512-pixel variants are retained.

## Validation

| Check | Result |
| --- | --- |
| Native suite without foreground windows | 75 tests passed; existing secure-paste window test excluded |
| Actual permitted music | Six live TIDAL snapshots, maximum metadata latency 48.4 ms; no playback changes |
| Regression checks | Blocked cover does not delay metadata; late covers cannot cross album editions; blocked sources start no artwork work; late artwork reaches both mocked transports |
| Final DMG | Universal x86_64/arm64, strict ad-hoc signature, checksum and readonly mounted build 49 executable parity verified |
| Installed runtime and device delivery | Pending coordinated normal quit, installation and actual live verification |

DMG SHA-256:
`967d45e13dddeb64b821d1bb6cd368e90653dda7a7d4d1aff5119e5b2ed6d9b7`.
Universal executable SHA-256:
`b84ba43f6cbdfd5202b88a0e81a4a23a394699b0e9401638501e79a51d4577fc`.

The app is ad-hoc signed and not notarized. Installed-runtime, physical-device and
publication evidence will be recorded separately from tests and package checks.
No firmware update is required for this correction.
