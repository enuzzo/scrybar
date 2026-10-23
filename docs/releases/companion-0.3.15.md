# Companion 0.3.15 (53): native album-art recovery

Date: 2026-10-23. Shared Companion for ScryBar and Meguru, macOS 14+.

The intermittent blank covers had more than one cause. A live trace confirmed
that the old JXA producer kept supplying valid TIDAL metadata while its artwork
lookup returned no pixels for a particular track. That producer did not read the
player’s native image bytes: it depended on finding a matching TIDAL cache entry
or catalog result. One live delivery probe waited 22.9 seconds without a cover
and stopped before sending anything. Covers had also reappeared spontaneously
before these changes; that earlier recovery was not counted as a fix.

Builds 52/53 use a bundled read-only MediaRemote stream to obtain the actual player
cover. Metadata polling does not wait for network artwork searches. The stream
checks source policy before image decoding or retention, and matches source PID,
bundle/parent, title, artist, album and available content/unique/artwork IDs.
Transiently missing pixels are retained only for the same complete identity;
changed identities discard old pixels and blocked sources clear immediately.
TIDAL partial updates use the upstream one-second debounce. While the native
reader initializes, a second blocking JXA probe and stale TIDAL metadata fallback
are suppressed; the first coherent frame may briefly be pending.

The native 150 px cover is delivered without waiting for optional HD enrichment.
A higher-resolution candidate must have at least 512 actual source pixels on
both axes, match the edition metadata, and match the native 150 px pixels with
an explicit JPEG tolerance. A thumbnail is never enlarged and called HD. The
current observed TIDAL source exposed 150 × 150 pixels even when native requests
asked for 512/768; this is an observation of that source, not a universal limit.
ScryBar retains 150 px RGB565 JSON; Meguru keeps its negotiated separate binary
lane and receives genuine 512 px detail when available, otherwise native 150 px.

The update also fixes two independent recovery paths:

- Successful artwork uploads refresh every 15 seconds per display using monotonic
  uptime. Refresh state advances only after success. This restores RAM-only covers
  after a display restart even when the current track/identifier is unchanged.
- The Companion preview observes the actual URL/dimensions/pixels as well as the
  track, so late artwork reloads the SwiftUI task. A canceled old load cannot
  replace a newer image.

Incomplete or invalid inline pixels cannot consume the first real delivery.
Content fingerprints invalidate delivery identity when real pixels change, and
wire identifiers longer than ScryBar’s 255-byte field receive a stable short hash
without changing the original resolver identity.

| Check | Result |
| --- | --- |
| Swift fixture suite | 92 tests passed: identity/edition changes, late pixels, privacy-before-resolution, per-display TTL, failed-upload retry, incomplete pixels, large split stream lines, oversized frames, no-upscale512 and independent display lanes |
| Owned reader lifecycle | Actual pipe fixture drains a 267 KB JSON frame before child exit. Final build 53 also fixes EOF/overflow cleanup: a child ignoring TERM survived before the patch, then the exact fixture passed with termination at 2.025 s. Explicit stop remained bounded and passed |
| Initial live failure | Cold startup ran JXA concurrently and hit a 3.008-second timeout; one nil snapshot. The trace was preserved, the exclusive-reader startup guard added, then the checks repeated |
| Final native source smoke | Actual build 52 capture code (unchanged in 53): first coherent frame 110.9 ms; next 20 snapshots all present/inSync, 19 with native 45,000-byte cover150, zero cover512, maximum snapshot latency 4.953 ms. The first artwork transcode is asynchronous |
| Natural track change | The preceding live run observed one natural change: old pixels discarded, matching new native cover available at the next poll; no playback controls used |
| Native HD availability | Read-only exact-current-item 512/768 requests both decoded to 150 × 150 for the observed source. No upscaled-HD success is claimed |
| ScryBar installed native 52 | Parent 4/4 HTTP200 active/playing/inSync/displayInSync, 150 × 150 cover; private display capture inspected and showed the current track’s correct cover. Separate from the earlier spontaneous recovery |
| ScryBar final installed 53 | 4/4 HTTP200 samples active/playing/inSync/displayInSync with coverID and 150 × 150 pixels, under the final installed app |
| Final performance regression | Unchanged firmware r322, 30.246 s under natural Companion 53 traffic plus delayed-header/slow-receiver stress: 1,170 active intervals at 60.0 fps, zero missed slots, P95 ≤18.00 ms/P99 ≤19.00 ms, maximum 20.443 ms. Whole-window rate including preparation 41.3 Hz; no HTTP busy/allocation/response/write failure. See [hardware receipt](../performance/2026-10-23-frame-pacing-measurements.json) |
| ScryBar physical TTL recovery | Parent reset the unchanged r322 device, without firmware/config writes. First successful HTTP sample 15.026 seconds later already had the same-track 150 px cover; no manual POST or Companion restart |
| Universal Release package | Final build 53 arm64+x86_64 Release passed outside Dropbox; DMG integrity/read-only mount, version 53, strict deep ad-hoc signature, bundled BSD notices and all 11 built/mounted regular files byte-equal |
| Installed Mac runtime | Final build 53 normal quit/install/relaunch passed; strict signature, complete built/installed byte parity, one app instance and one owned Perl reader, no orphan |
| Meguru live upload | Build 52 production transport (unchanged in 53) explicit current-track probe passed in 2.991 s using the installed bridge resources: metadata 2xx and native 150 binary artwork 2xx (45,000 bytes), with device capability 512 negotiated. This establishes delivery, not visible panel pixels |
| Publication | Local final package and installed checks complete; public upload/byte verification remains owned by the release coordinator. Meguru visual confirmation is still pending from the owner; HTTP delivery is established separately |

Final local build 53 DMG SHA-256:
`25cb9876d8437da91e5f054078fb7218bb398f7d027431ca3b6bb6d02e9aca96`
(9,028,104 bytes). Universal executable SHA-256:
`8ac847984a2251d417191cb28828ae30907638aa05e53d0072ce4e6b3ca0cf9d`.

Build 53 differs from 52 only in bounded reader termination on EOF/oversized output
and its build identifier. The source, artwork encoding and device transport were
not changed again; the live music/POST probes were not repeated for that lifecycle
correction. Final installed-device results are recorded separately.

The native helper is pinned to
[mediaremote-adapter commit 0e82bf1c](https://github.com/ungive/mediaremote-adapter/tree/0e82bf1c44ec39554fbfc402ec88a48c6ae2a0fd),
with two documented local patches for artwork identifiers and full-identity
retention. Its BSD 3-Clause license and complete notices accompany the bundle.
The framework is loaded at runtime by the bundled Perl loader, is not linked
into the app, and exposes no playback-control operation in this integration.
No TestClient helper is built or included.

The package is ad-hoc signed, not Developer ID signed or notarized. No valid
code-signing identity was available on this Mac; no certificate or Keychain
configuration was changed. [SwiftUI’s task identity behavior](https://developer.apple.com/documentation/swiftui/view/task(id:priority:_:))
supports the preview reload correction.

Rollback: installed 0.3.14/build50 and intermediate 0.3.15/build51 apps are
preserved in private ZIPs under Documents/Codex/ScryBar. Their distribution DMGs
are in `archive/companion/2026-10-23-artwork-fix/`, verified by ZIP CRC and complete
original byte comparisons before replacement. Final `dist/` keeps only the
latest 0.3.15 DMG. Existing archives and unrelated files were preserved.

Reproduce fixtures without live device access:

```sh
swift test --disable-sandbox \
  --package-path companion/mac/ScryBarCompanion \
  --scratch-path /tmp/scrybar-companion-artwork-tests.noindex
```

Live source and delivery probes require explicit environment opt-in. The native
source smoke reads current permitted music without changing playback. The Meguru
probe sends that current permitted metadata/cover to an already selected display
and must not run during ordinary fixture testing. Private trace receipts contain
only flags, sizes and timings, never music metadata, identifiers, URLs or pixels.
