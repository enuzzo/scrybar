# Companion multi-display protocol v1

One ScryBar Companion application and one printer connection serve ScryBar,
Meguru and the Mac viewer, with an optional studio TV Now Playing destination.
The current package is 0.3.10 (45); publication and hardware evidence live in the
[release record](releases/companion-0.3.10.md). The additive
[live network extension](mac-stats-live-v1.md) needs ScryBar r309 for its compact
network view, or Meguru firmware advertising `mac-stats`. Older firmware keeps
its existing supported feeds. Bundle identifier and printer Keychain
entries remain unchanged. Dated evidence below distinguishes earlier checkpoints
from the 2026-10-19 privacy/TV changes and 2026-10-22 live network work.

## Now Playing privacy and STUDIO TV

**Binding studio rule (owner, 2026-10-19):** only Apple Music (`com.apple.Music`),
Spotify (`com.spotify.client`) and TIDAL (`com.tidal.desktop`) may expose Now Playing.
VLC, IINA, QuickTime, TV, browsers (Safari, Chrome, Firefox, Arc and others),
YouTube, YouTube Music and unknown apps must never expose their content to a
display, a log or a cover-art search. YouTube Music counts as a browser. These
displays face colleagues/clients; every future Companion/firmware change must
preserve this rule, including new providers, destinations and diagnostics.

`SourcePolicy` gates System snapshots after bundle-ID resolution, before artwork
resolution/cache or iTunes Search. `AppModel.refreshPayload` gates every provider
again before fanout. Rejection replaces the current snapshot with the empty
placeholder and the status `Hidden for privacy`; only the category is remembered.
`sourcePolicy.musicOnly` defaults on in every build when unset, with browser/video
extras off. Saved preferences persist. Disabling it also admits unknown apps;
the opt-out settings must not be used in this studio configuration.

STUDIO TV uses `YakamozPush`, not the Bonjour/capability or binary-artwork paths.
It is off by default. With Auto-send on, `yakamoz.url` / `yakamoz.enabled` configure
HTTPS delivery to `https://<HOST>/api/musica_push`; `<API_KEY>` stays in Keychain
service `com.netmilk.ScryBarCompanion.yakamoz` (account `push-key`) and is sent in
`X-Yakamoz-Key`. A nonempty permitted payload carries title/artist/album, `bundleId`,
playback state/timing and public `artworkURL`. Changes trigger delivery, plus a
10-second heartbeat for a nonempty session. `{"bundleId":"none"}` clears the TV
when no permitted session exists. The receiver has a separate music-bundle
allowlist and a documented 30-second silence expiry; live server behavior was
not reverified for this documentation update.

Unreleased `092b279d` adds internal `NowPlayingPayload.webArtworkURL` for the
actual resolved TIDAL-edition/iTunes-fallback cover. TV push prefers that URL;
it is not a new ScryBar/Meguru JSON field. `NowPlayingWirePayload` is unchanged.

## Discovery and selection

- Discover `_scrybar._tcp.local.` and `_meguru._tcp.local.` simultaneously.
- Meguru TXT must include `product=meguru` and `protocol=1`.
- Stable identity is service type + domain + instance name; host/port may change.
- Persist enabled displays and their last address in `displayTargets.v1`.
- Migrate the existing manual target. Retain the first discovered ScryBar and
  enable the first Meguru as requested for this integration; subsequent displays
  of the same product require selection. Previously disabled devices stay disabled.
- Discovery disappearance does not replace/remove selection. Reappearance updates
  its address. Manual addresses remain available alongside discovered displays.
- Settings show independent connection/error state for each destination.

## Capability handshake

`GET /api/companion`, uncached, 2-second timeout, response at most 4096 bytes:

```json
{
  "version": 1,
  "product": "meguru",
  "feeds": ["now-playing", "bambu"],
  "maxBodyBytes": 98304,
  "artwork": false,
  "camera": true
}
```

Use `camera:false` until camera endpoints are implemented. `mac-stats` is optional.
Only HTTP 404/405 on a ScryBar/manual legacy target enables legacy fallback:
all three feeds, artwork and existing camera protocol. Meguru discovery requires a
valid handshake; timeouts, malformed responses and unsupported versions never
silently enable capabilities. Cache capabilities for 60 seconds; invalidate after
feed failures. Changing address requires a new handshake.

## Telemetry

POST `/api/now-playing`, `/api/bambu` and (when accepted) `/api/mac-stats` preserve
legacy JSON field names and ISO8601 `updatedAt`. The additive optional `isDemo:true`
is sent only by the Mock Now Playing provider; absence means false. Meguru must
label demo metadata explicitly. No credentials are forwarded.
Meguru metadata bodies are capped at min(16384, maxBodyBytes). Artwork-disabled
endpoints receive none of the artwork fields. ScryBar retains artwork with a
separate per-address successful-send cache, cleared on transmission failure.

Independent endpoint/feed lanes allow at most one request in flight per lane.
There is no durable backlog: intermediate snapshots are dropped and the next
poll uses current data. HTTP timeout is 2 seconds. Metadata older than 15 seconds
is not submitted. Deselection/address replacement cancels and invalidates pending
responses. One unavailable destination never delays another. The firmware must
expire received metadata using its own monotonic 15-second freshness timer and
show unavailable/stale instead of treating the last snapshot as live indefinitely.

A successful HTTP POST means accepted, not that pixels have been inspected.
Per-feed errors and last-success time are separate. The aggregate connected
indicator becomes stale after eight seconds without a successful feed.

### Recommended firmware defence (not implemented)

Add a second music-source allowlist to ScryBar's `applyNowPlayingPayloadJson`
ingestion path. The current handler accepts `source`/`appName`, installs artwork
and logs title/artist without checking this policy. A Companion-only gate cannot
protect it from an older or misconfigured sender.

The proposed gate should show Now Playing only for the three allowed music apps,
using exact accepted `source` aliases and checking `appName` identity where
available. Preserve legacy `Music` / `Music.app` and `TIDAL` provider aliases and
System-provider bundle IDs; do not accept arbitrary names, substring matches or
`System` as a music identity. Unknown, missing or disallowed identities should
clear the previous title and artwork before rendering, metadata logging or artwork
processing. Accept the intentional empty Companion placeholder as a clearing
operation, without expanding the real-app allowlist to admit it.

This is a recommendation based on the current ingestion path, consistent with
[OWASP input validation guidance](https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html)
to validate fixed choices against an allowlist before processing/storage. It
limits accidental disclosure; caller-supplied source strings do not authenticate
the sender. Future validation should cover all three music apps, blocked/missing
sources, a transition from music to blocked content and empty-placeholder clearing,
including absence of rejected metadata in serial output. **No firmware or wire
contract change is implemented by this documentation update.**

### High-resolution Meguru artwork — local 0.3.8 (43) candidate

Meguru M0.14.16 optionally advertises `artworkSize:512` and
`artworkMaxBodyBytes:524288`, separately from the unchanged metadata/camera
limits. Companion prepares a 512 px RGB565 binary variant only for a target
that advertises both sufficient capabilities. Older/limited Meguru targets keep
150 px binary covers; ScryBar keeps its existing 150 px JSON artwork. An invalid
HD buffer falls back to the validated legacy variant. HD data is excluded from
metadata JSON, and artwork cache identity includes the delivered resolution.

Known TIDAL HTTPS image suffixes prefer a 640 px source while retaining the
cover ID. For an opaque System TIDAL identifier, the existing local TIDAL
metadata provider must match the active title/artist before choosing its cover.
The existing non-TIDAL URL and artwork-search fallback remain. Both variants
are generated once per cached artwork/track change.

Local validation: all 38 Swift tests pass, including HD/legacy negotiation,
malformed buffers, independent ScryBar delivery and fine red/blue stripe fidelity.
Apple Silicon Release build and ad-hoc signature verification pass. Meguru
M0.14.16 is installed. On 2026-10-14, after owner normal quit, Companion 0.3.8
was installed/relaunched with exact executable and strict signature verification,
preserving preferences/Keychain and the old app rollback. Actual Meguru reports
fresh matching 512 px covers. Native app UI inspection is still unavailable;
TIDAL appearance and physical track-change acceptance remain owner checks.
No publication or ScryBar firmware update is involved.

## Real capabilities and exclusions

Bambu currently supports telemetry, A1/P1 JPEG camera and absolute chamber-light
ON/OFF through MQTT `ledctrl`, with actual-state confirmation from `lights_report`.
There are **no print pause/resume/cancel commands** in this Companion.
Now Playing exposes metadata from System/Music/Mock. There are **no playback,
next-track or previous-track commands**. Do not present such controls in firmware.

## Shared camera and light

One TLS printer camera stream feeds the Mac and every enabled camera-capable
endpoint. Each endpoint has an independent lease (five seconds), session, light
revision and last delivered frame version. Poll each device independently once
per second (2-second HTTP timeout). Closing or losing one viewer does not stop
the stream while another viewer remains. Closing the final viewer stops the
stream and requests persisted light-off cleanup.

`GET /api/bambu/camera`:

```json
{
  "session": "unique-boot-and-view-token",
  "active": true,
  "lightRevision": 1,
  "light": true,
  "issuedAt": "2026-09-23T10:00:00Z",
  "expiresAt": "2026-09-23T10:00:05Z"
}
```

- Revision 0 has no light command. Revisions must increase within each session.
- Session changes on boot/view entry or exit; retired sessions cannot replace the
  current lease. Firmware must reject POST replies for a different session.
- Meguru requires an ISO8601 issuedAt/expiresAt on nonzero light commands (fractional
  seconds supported), TTL <=5 seconds. Without a synchronized device clock, disable
  light controls. Initial camera viewing does not itself turn on the light.
- Consume and persist each revision before forwarding a command. Duplicate/lower
  revisions and consumed revisions after Companion restart never resend the tap.
- Late responses from deselected/changed endpoints cannot issue commands.
- A new valid command supersedes the prior pending command across viewers.
- An ON command is never retried beyond its expiry; session closure/lease loss
  cancels its retries. Persisted OFF cleanup remains retryable for safety.
- Legacy ScryBar has no timestamps: revision/session deduplication is supported,
  but the original age of a previously unseen legacy tap cannot be established.

`POST /api/bambu/camera`:

```json
{
  "session": "unique-boot-and-view-token",
  "status": "Live · A1 / P1 camera",
  "lightKnown": true,
  "lightOn": true,
  "lightPending": false,
  "lightRevision": 1,
  "lightError": "",
  "rgb565": "base64 RGB565 big-endian 240x135"
}
```

`rgb565` is optional: only new frames younger than 12 seconds are sent, independently
per viewer. Meguru may render them as an ASCII glyph field without changing wire
format. Entire camera POST must fit advertised maxBodyBytes (98304 permits the
86400-byte base64 frame plus state). Firmware must expire displayed frames.

`lightRevision` acknowledges **consumption**, not printer success. `lightPending`
is true until actual MQTT state confirms the latest command. `lightError` is empty
on success, or `expired_or_invalid_command`, `already_consumed`, `superseded`,
`confirmation_timeout`, `lease_expired`, `view_closed`, `session_changed` or
`target_removed`. Read `lightKnown`/`lightOn` for actual observed state.
`already_consumed` after restart means not re-executed; do not label it a new success.
No pending request is an implicit toggle: all MQTT commands set an absolute state.

## Verification and installation

Tests cover legacy fallback and strict Meguru handshake, metadata limits,
artwork isolation, persistent selection/DHCP changes, concurrent fast/slow displays,
staleness, camera lease independence, revision deduplication, expiry, restart
replay suppression, image format and existing cleanup behavior. Full physical
acceptance requires both displays online and real camera/light feedback.

Install only after tests/build pass. Back up the current `/Applications` app before
replacement; preserve UserDefaults and Keychain. Roll back the app bundle if
needed; new preference keys are additive and old versions ignore them. Never
publish local credentials, private test endpoints, app backups or captures.

### Local evidence, 2026-09-23

- Candidate app version 0.3.6 (41), same bundle identifier, universal Release build.
- 35 Swift tests passed, including real parallel mock HTTP camera polling and
  positive `isDemo` identification; existing clipboard test included.
- Local installed baseline was 0.3.4 (40), with its manual target retained.
- Startup sampling exposed a pre-existing main-thread Keychain wait while the Mac
  was locked. Saved credential reads now run off the main thread; discovery and
  telemetry startup do not depend on responding to a Keychain consent prompt.
- App/credential access, visual verification and two physical displays remain
  separate acceptance gates. Do not treat host protocol tests as physical proof.

- Physical M0.11.1 discovery was observed: Companion automatically selected the
  Meguru Bonjour service while keeping the existing ScryBar selected. Two immediate
  HTTP capability reads timed out during the diagnostic run; simultaneous feed
  reception remains unverified until a stable normal-firmware run.

- Repaired M0.11.1 diagnostic run: two capability reads one minute apart
  confirmed `receivedNowPlaying=true`. At the same time, physical ScryBar reported
  the Music source in sync with a 688 ms payload age, proving automatic Now Playing
  fanout to both devices. Meguru still reported `receivedBambu=false`; live Bambu
  and camera acceptance remain pending credential access and printer connection.
- Installed candidate signature verified; installed executable SHA-256 matched
  the tested Release executable. No printer or light commands were issued for QA.

### Final local handoff, 2026-09-23

- Normal M0.11.1 firmware: one simultaneous read-only check confirmed Meguru
  `receivedNowPlaying=true` and physical ScryBar `active=true`, `inSync=true`,
  `displayInSync=true`, Music source and 950 ms payload age. This verifies
  automatic Now Playing delivery to both consumers after the final firmware boot.
- Meguru `receivedBambu=false`: saved credential access through macOS Keychain
  remains pending. Live Bambu telemetry and camera acceptance are still open;
  no credential bypass or printer/light command was attempted.
- Companion candidate remains locally installed as 0.3.6 (41), with the previous
  app and preferences backed up for rollback. Tests, Release build, signature
  and installed executable identity were verified as recorded above.
- At the 2026-09-23 handoff, Companion source, tests and this contract were
  uncommitted. No push, public package or release was created at that handoff.

### Bambu gate after Mac unlock, 2026-09-23

- With the Mac unlocked and the Companion already running, both persisted display
  targets remained enabled: the ScryBar endpoint and the Meguru endpoint.
- Meguru then reported `receivedNowPlaying=true` and `receivedBambu=true` from the
  same Companion process. This is the first live confirmation that the shared
  Companion fanout reaches both feeds while keeping ScryBar active.
- ScryBar's Bambu endpoint is POST-only, so a read-only GET correctly returned 404;
  Meguru's capability response is the read-side receipt for the Bambu POST.
- No camera, light, pause, resume or print command was issued. Camera remains a
  read-only acceptance gate requiring an active view and a live printer stream.


### Earlier source checkpoint, 2026-10-19 (before 0.3.9)

The multi-display implementation and this contract are now saved in Git alongside
separate Bambu, firmware-idle, icon-proposal, package-cleanup and documentation
commits. Sources remain 0.3.8 (43); the retained 0.3.7 (42) DMG packages an earlier
candidate. No 0.3.8 DMG, push, tag, installation or release is part of this checkpoint.
Six obsolete tracked DMGs are removed from the current tree and remain in history.
Untracked `output/` is excluded and untouched.

All 38 Swift tests pass using the native engine outside the sandbox; all three
host C++ tests pass. Initial signing/pasteboard failures and the final successful
run are recorded in `CHANGELOG.md`. No implementation/test fixes were made. The
2026-09-23 and 2026-10-14 device/installation reports above are historical evidence;
no physical device, current installed app, camera/light or track-change acceptance
was rechecked for this checkpoint. Keep those gates separate from local tests.

### Privacy/TV package and unreleased cover URL, 2026-10-19

- `e68add4`: SourcePolicy, PRIVACY/STUDIO TV settings and HTTPS push, version
  0.3.9 (44). The implementation session reports 44/44 Swift tests passed,
  including six `SourcePolicyTests` (three permitted music apps; blocked sources;
  extra-switch precedence; categories; provider aliases; unset preference default).
- `da6bf78`: `companion/mac/ScryBarCompanion-0.3.9.dmg`; owner-reported installation
  in `/Applications`. Public release state and physical display/TV acceptance
  are separate from that local package and report.
- `092b279d`: resolved public cover URL for web delivery, not in that DMG.
  Next build is planned as 0.3.10; project metadata still reads 0.3.9 (44) pending
  the bump. Do not represent a build of current sources as the original 0.3.9 DMG.
- This documentation task does not rerun Swift tests, install/build a package,
  contact the TV, flash firmware or perform physical acceptance checks.
