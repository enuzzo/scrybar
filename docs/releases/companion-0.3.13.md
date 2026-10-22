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
| Installed Mac runtime | Owner-confirmed normal quit; final build 49 installed in Applications with build 48 preserved; exact executable hash/signature verified; single background launch from that path |
| Actual ScryBar music delivery | Six of six samples match current system title, artist and album; elapsed advances 82 to 98 seconds; receive age 18–1086 ms; cover 150 × 150 present |
| ScryBar physical display | Real 640 × 172 Now Playing frame shows song, artist, album, artwork and IN SYNC; two MacStats frames show LIVE and changing network/CPU values |
| Meguru joint feed | GUI-owning chat reports three of three samples receiving Now Playing, MacStats and Bambu together; late artwork becomes ready at 512 pixels, revision stable after arrival |
| Meguru physical music | Owner explicitly confirmed correct current song, cover and advancing elapsed through the GUI-owning chat; this physical confirmation is separate from the capability/receipt API, which does not expose title or elapsed |

DMG SHA-256:
`967d45e13dddeb64b821d1bb6cd368e90653dda7a7d4d1aff5119e5b2ed6d9b7`.
Universal executable SHA-256:
`b84ba43f6cbdfd5202b88a0e81a4a23a394699b0e9401638501e79a51d4577fc`.

The app is ad-hoc signed and not notarized. No firmware update is required for
this correction. Playback was not controlled or changed for these checks, and
no menu panel was opened on the owner's desktop.

## Publication

The source/package snapshot for `companion-v0.3.13` is
`c6a8265f21379a29565ee608844ffada12dc1838` on the isolated
`codex/companion-network-scopes` branch. Its validation record predates installed
runtime checks; this later record and the release notes carry those results.
Published source/assets remain immutable. Stable firmware Latest stays v0.9.4.
