# ScryBar Companion

macOS menu-bar app (Swift/SwiftUI) that feeds **Now Playing**, **Mac Stats**, and credential-free **Bambu Lab print status** to enabled ScryBar and Meguru displays, according to each display's capabilities.
An optional **STUDIO TV** destination receives permitted Now Playing metadata over HTTPS.

> **Mandatory studio privacy rule — 2026-10-19:** Now Playing may come only from
> **Apple Music** (`com.apple.Music`), **Spotify** (`com.spotify.client`) or
> **TIDAL** (`com.tidal.desktop`). VLC, IINA, QuickTime, TV, browsers (Safari,
> Chrome, Firefox, Arc and others), YouTube, YouTube Music and unknown apps must
> never expose their content on a display, in a log or through a cover-art search.
> YouTube Music counts as a browser, not an allowed music app. Colleagues and
> clients can see the studio displays: never reveal what somebody is watching.
> Keep **PRIVACY → Music apps only** enabled in this setup.

---

## Installation

Current package: **v0.3.15 (53)** for macOS 14+, Apple Silicon and Intel,
prepared on **2026-10-23**. [Download the DMG](https://github.com/enuzzo/scrybar/releases/download/companion-v0.3.15/ScryBarCompanion-0.3.15.dmg).
One app serves both displays. This version reads the player’s native album cover
from a bundled, read-only MediaRemote stream, removing the dependency on a
matching catalog search for the standard cover. It also restores unchanged
ScryBar covers after display restarts and reloads the Companion preview when
pixels arrive after metadata. Meguru receives 512 px only when genuine source
detail is available; a 150 px thumbnail is never enlarged and labeled HD.
Centered network rates, music-only privacy and the optional studio TV push remain.
The package is ad-hoc signed, not Developer ID signed or notarized.
See the [release and verification record](../../../docs/releases/companion-0.3.15.md)
for publication, installation and hardware-test status.

1. Download [ScryBarCompanion-0.3.15.dmg](https://github.com/enuzzo/scrybar/releases/download/companion-v0.3.15/ScryBarCompanion-0.3.15.dmg). Older public 0.3.5 builds predate the music-only privacy protection.
2. Close camera viewers and quit the previous Companion normally, allowing any pending light-off cleanup to finish.
3. Open the DMG and drag **ScryBarCompanion.app** into **Applications**, replacing the previous version if present.
4. Launch the app. For the first-launch Gatekeeper prompt, follow the instructions below. Allow Local Network access and any Keychain prompt for your saved printer.
5. Enable your displays in Settings. The new desk network view needs **ScryBar r309** or Meguru firmware with **Mac Stats** support; their separate installation status is in the [release record](../../../docs/releases/companion-0.3.10.md). Existing firmware keeps its supported feeds. Printer credentials stay in the Companion's Keychain.

## Multiple displays

- Settings lists ScryBar and Meguru displays with independent enable switches,
  connection feedback and errors. Selection survives restarts and address changes.
- Bonjour browses `_scrybar._tcp` and `_meguru._tcp` together. The first display of
  each product is enabled automatically; additional displays require selection.
  The previous manual target is migrated and manual addresses remain available.
- Each destination negotiates feeds through `GET /api/companion`. A slow or
  unavailable display does not hold up the others; stale metadata is not queued.
- Meguru receives separate RGB565 covers: 150 px by default, or 512 px when it
  advertises the resolution and binary-body budget and a genuine ≥512 px source
  is available. Native 150 px artwork is delivered without waiting for optional
  high-resolution enrichment; candidates must match the edition metadata and
  native pixels. ScryBar retains 150 px JSON
  artwork. Mock metadata carries `isDemo:true`.
- Successful artwork uploads refresh every 15 seconds per display, using a
  monotonic clock. This restores RAM-only covers after a display reboot even if
  the track and endpoint did not change; ordinary one-second metadata polls
  omit the image bytes. Invalid or incomplete pixels do not consume the first
  valid artwork delivery.
- Bambu camera viewers share one printer stream with independent leases and
  frame delivery. Light commands are deduplicated across polling/restarts and
  expire; printer telemetry confirms the actual lamp state.

See the [multi-display protocol and evidence history](../../../docs/companion-multidevice-v1.md)
for limits, legacy fallback and outstanding physical checks.

## Bambu camera

- Enable LAN access on a supported A1/P1 printer. In the Companion's Bambu section, select the printer and save its LAN access code. Wait for live printer status.
- Open **Camera** in the Companion, or tap **CAM** in ScryBar's Bambu header. Bambu Studio is not required; keep the Companion running and both devices on the reachable LAN.
- On ScryBar, **Light: ON** (yellow) and **Light: OFF** (gray) report the lamp state. **Turn on** / **Turn off** is the action; **Back** closes the camera. Opening the camera does not turn the light on automatically.
- On the Mac, Start/Stop controls that viewer; close the camera window to leave it. If another viewer remains open, the shared stream continues.
- Closing the last viewer stops the stream and requests light-off. If the printer is unreachable, the Companion persists and retries cleanup; a normal app quit waits for confirmation. Forced termination, lost power or an unreachable printer can delay cleanup, so check the physical lamp in those cases.
- This release supports the A1/P1 LAN JPEG camera, including its native low frame rate. X1/H2 RTSPS cameras are not supported. Camera images are not saved.

## Features

| Feature | Notes |
|---|---|
| **Now Playing** | Read-only native MediaRemote stream with source and track identity checks; JXA metadata fallback if the bridge is unavailable; Apple Music, Spotify and TIDAL allowed by default |
| **Music.app provider** | Direct AppleScript integration for richer metadata |
| **Privacy** | Music-only source policy before artwork lookup and again before delivery; blocked content becomes an empty placeholder |
| **Studio TV** | Optional HTTPS Now Playing push with a Keychain-stored key; independent receiver allowlist |
| **Mac Stats** | CPU/GPU usage %, CPU/GPU temperature, RAM and disk usage |
| **Network** | Live download/upload in a compact menu item; click for up to five ranked apps and separate rates |
| **Bambu Lab monitor** | SSDP discovery plus direct LAN MQTT/TLS status with Keychain-protected access codes |
| **A1/P1 camera** | Shared Mac/display stream and light-off cleanup; Meguru in the local candidate |
| **Multiple printers** | Select a discovered printer; credentials remain separate for each serial number |
| **Setup Help** | Short ASD-STE100-style connection and troubleshooting instructions in the app |
| **Print event sounds** | Distinct macOS sounds for start, pause/fault, and completion |
| **LAN discovery** | Discover ScryBar and Meguru via `_scrybar._tcp` / `_meguru._tcp` |
| **Manual target** | Persisted `host/IP + port` entries alongside discovered displays |
| **Auto-send** | Pushes Now Playing and Mac Stats at approximately one-second intervals |

Network totals measure current traffic on physical Ethernet/Wi-Fi interfaces,
not the maximum capacity of your internet connection. Per-app TCP/UDP accounting
can differ from these totals, especially with VPNs. Only the leading app's name
and rates go to the displays; the fuller ranking remains on the Mac. No domains,
destination addresses or browser content are collected for this feature.
Settings → **Network rates in the menu bar** controls the additional indicator;
the original music item remains available. Unknown samples show a dash; measured
idle traffic is zero. ScryBar needs r309 for the compact network layout; Meguru
must advertise `mac-stats`. Older firmware keeps its existing supported feeds.
See the [network contract](../../../docs/mac-stats-live-v1.md).

## Settings — PRIVACY

| Control | Default | Behavior |
|---|---|---|
| **Music apps only** | **On** | Only the three music bundle IDs above pass; overrides both extra switches |
| **Also browsers and YouTube (any site)** | Off | Available only with music-only off; includes YouTube Music and browser web apps |
| **Also video players (VLC, IINA, QuickTime, TV…)** | Off | Available only with music-only off |

`SourcePolicy.swift` classifies sources as `music`, `browser`, `videoPlayer` or
`other`. Preferences persist in `sourcePolicy.musicOnly`,
`sourcePolicy.allowBrowsers` and `sourcePolicy.allowVideoPlayers`. Music-only
defaults to on in every build when the preference is absent; saved choices persist.
**Turning music-only off also permits unknown/other apps, even with both extras
off. These opt-out settings do not satisfy the mandatory studio rule.**

The bundled MediaRemote reader checks source policy before decoding or retaining
artwork. `SystemNowPlayingProvider` checks the bundle ID before artwork lookup,
caching or logging, including before any iTunes Search request. The stream uses
full source/track/edition identity, retains transiently absent pixels only for
that exact identity and immediately clears blocked sources. While the stream
starts, it does not launch a second blocking JXA probe or substitute old TIDAL
cache metadata. The initial frame may briefly be pending; it is never invented. `AppModel.refreshPayload`
checks every provider again before delivery. A blocked source replaces the current
payload with an empty placeholder and shows **Hidden for privacy**. Only the
hidden category is remembered, never its title. Future providers must apply the
same early protection before external lookups, not rely only on the delivery check.

## Settings — STUDIO TV

This optional third destination (`YakamozPush.swift`) is **off by default** and
sends only Now Playing, independently of ScryBar/Meguru capability negotiation.

1. Keep **PRIVACY → Music apps only** on and enable **Auto-send**.
2. Set **Push URL** to `https://<HOST>/api/musica_push` (HTTPS is required).
3. Enter `<API_KEY>` in **Push key**, then select **Save key**.
4. Enable **Send now playing to the studio TV** and check its connection status.

`yakamoz.url` and `yakamoz.enabled` live in UserDefaults. The key lives only in
Keychain service `com.netmilk.ScryBarCompanion.yakamoz`, account `push-key`, and
is sent in the `X-Yakamoz-Key` header. Never commit a real endpoint or key.
Updates follow track/playback/artwork changes, with a 10-second heartbeat while
a nonempty permitted track is present. When nothing permitted is available,
`{"bundleId":"none"}` clears the TV's previous track. The TV server has its own
music-app allowlist as a second defence; the documented receiver expires silent
feeds after 30 seconds. This task does not verify live TV receipt.

In the unreleased `092b279d` change, internal `NowPlayingPayload.webArtworkURL`
retains the public cover actually used (the matching TIDAL edition or iTunes
fallback). TV push prefers it over `artworkURL`. It is **not** an added field in
`NowPlayingWirePayload`; the ScryBar/Meguru wire contract is unchanged.

---

## Prerequisites

### macmon — required for CPU/GPU temperatures on Apple Silicon

```bash
brew install macmon
```

[`macmon`](https://github.com/vladkens/macmon) is a community CLI tool that reads M1–M4 temperatures
through the same private IOKit power-management channel Apple uses internally — **no `sudo` required**.

Without macmon, the Mac Stats page on ScryBar will still show CPU/GPU usage % (from `host_processor_info`
and `IOAccelerator`) but temperature fields will display `—`.

The companion app shows a live indicator in its popover:

- **Green "Live"** dot → data is fresh (< 15 s old)
- **Amber "Stale"** dot → last sample is outdated
- If temps are `—` and macmon is not found, the popover shows:
  > *Install macmon for temperatures: `brew install macmon`*

---

## Build from source

### Quick build (Terminal)

```bash
cd companion/mac/ScryBarCompanion
xcodegen generate        # regenerate .xcodeproj from project.yml
swift build -c release
```

The binary lands at `.build/release/ScryBarCompanion`. To launch it directly:

```bash
.build/release/ScryBarCompanion &
```

### Build a proper .app bundle (Xcode)

```bash
cd companion/mac/ScryBarCompanion
xcodegen generate
open ScryBarCompanion.xcodeproj
```

Then **Product → Archive → Distribute App → Custom → Mac Application**.

A release maintainer can regenerate a universal app and DMG from the repository
root when the Xcode toolchain is available:

```bash
./script/build_companion_dmg.sh
```

The script reads the version from `project.yml` (currently **0.3.14 (50)**) and
writes the universal distributable to `dist/ScryBarCompanion-<VERSION>.dmg` at the
repository root. Keep only the current DMG there and the current installed app
at `/Applications/ScryBarCompanion.app`. The script does not export another app
bundle into the repository. Older app bundles and DMGs are preserved as ZIP files
under `archive/companion/2026-10-23-cleanup/`; archive superseded packages before
moving them out of the available builds.

Both this script and `script/build_and_run.sh` default to separate temporary
derived-data directories outside Dropbox, with a `.noindex` suffix. Set
`SCRYBAR_DERIVED_DATA` to override the relevant script's location.
On the Xcode 27.1 machine used for the 0.3.10 package, the SDK stat-cache process waited in
its event loop; `SCRYBAR_SDK_STAT_CACHE_ENABLE=NO` disables only that build cache
for this script invocation, without changing global Xcode settings.

### Tests (no signing)

From `companion/mac/ScryBarCompanion`, run `swift test`. With the Swift 6.4
toolchain used for this checkpoint, `--build-system native` avoids the default
engine's test-bundle signing step (the native engine is deprecated). The package
has an executable target and a test target, with no external package dependencies.
The 2026-10-19 checkpoint runs the suite with isolated build/cache directories
under the repository's ignored `build/.codex-tmp/` area; it does not launch the app
or connect to a physical printer. See the [changelog](../../../CHANGELOG.md) for
that run's result. The earlier 0.3.8 checkpoint passed 38 tests when the named
test pasteboard could access macOS services; its sandboxed run failed at that
service boundary. The 0.3.9 implementation session reports **44/44 tests passed**,
including six `SourcePolicyTests`. The current package's test and native smoke
results are in the [release record](../../../docs/releases/companion-0.3.10.md).
Device/UI acceptance remains separate.

---

## Running the app

### First launch — Gatekeeper

The app is ad-hoc signed (no Apple Developer ID), so macOS Gatekeeper will block it on first run.

To open it:

> **System Settings → Privacy & Security → "ScryBarCompanion was blocked…" → Open Anyway**

This is a one-time step.

### Required permissions

| Permission | Why |
|---|---|
| **Local Network** | mDNS/Bonjour discovery of ScryBar and Meguru |
| **Automation (Music.app)** | Only prompted when Music provider is selected |

---

## Payload contracts

### `POST /api/now-playing`

Only permitted sources reach this contract. Blocked sources send the empty
placeholder instead; the rejected title/artwork must never be substituted into it.

```json
{
  "title": "Track title",
  "artist": "Artist name",
  "album": "Album name",
  "source": "Spotify",
  "appName": "com.spotify.client",
  "durationSec": 412,
  "elapsedSec": 103,
  "isPlaying": true,
  "inSync": true,
  "artworkURL": null,
  "updatedAt": "2026-04-22T12:00:00Z"
}
```

### `POST /api/mac-stats`

```json
{
  "cpuTempC":    47.5,
  "gpuTempC":    48.0,
  "cpuUsagePct": 12.3,
  "gpuUsagePct": 6.0,
  "ramUsedGB":   14.2,
  "ramTotalGB":  16.0,
  "diskUsedGB":  412.0,
  "diskTotalGB": 994.0,
  "updatedAt":   "2026-04-22T12:00:00Z"
}
```

`cpuTempC` / `gpuTempC` are `null` when macmon is unavailable.
`cpuUsagePct` is `null` on the very first 5-second cycle (baseline needs one prior sample).

### `POST /api/bambu`

The companion connects directly to the printer on LAN port 8883 and forwards only the current print snapshot to ScryBar. Printer credentials never enter this payload. See [`../../../docs/bambu-lan-integration.md`](../../../docs/bambu-lan-integration.md) for configuration, fields, sounds, security notes, and the full example.

---

## How Mac Stats are collected

| Metric | Source | Notes |
|---|---|---|
| CPU temperature | `macmon pipe -s 1 -i 1000` | Subprocess; parses `temp.cpu_temp_avg` JSON |
| GPU temperature | `macmon pipe -s 1 -i 1000` | Same subprocess; `temp.gpu_temp_avg` |
| CPU usage % | `host_processor_info(PROCESSOR_CPU_LOAD_INFO)` | Mach API tick delta between polls |
| GPU usage % | `IOAccelerator PerformanceStatistics["Device Utilization %"]` | IOKit iterator; no sudo |
| RAM used / total | `host_statistics64(HOST_VM_INFO64)` + `hw.memsize` sysctl | |
| Disk used / total | `FileManager.default.attributesOfFileSystem(forPath: "/")` | |
| Network download / upload | Public `IFMIB_IFDATA` 64-bit interface counters | Active physical Ethernet/Wi-Fi; actual elapsed time |
| Per-app network rates | One persistent system `nettop`, process summaries only | 1 Hz; helpers aggregated by containing app; no domains |

Sampling runs on a `Task.detached(priority: .background)` at approximately 1 Hz
to avoid blocking the UI thread. Network deltas use a continuous monotonic clock
that includes sleep; resets and long gaps require a new baseline.

---

## Versioning

Bump `CURRENT_PROJECT_VERSION` in `project.yml` with every published build.

---

## Known issues / caveats

- `MediaRemote.framework` is private; behavior may change across macOS updates.
- Source/app naming is best-effort — some sessions expose `displayName`, others only metadata.
- macmon is a third-party tool. If Homebrew paths differ (e.g. Rosetta Intel prefix), macmon detection may fail; set `MACMON_PATH` env override (planned).
- SMC key-based temperature reading (legacy fallback for Intel Macs) returns `0` on M3 Ultra / M4 — macmon is the only supported path on those chips.

### Estimated print finish

During an active print, the Bambu card shows **Estimated finish** with the local day and time, calculated from the latest telemetry timestamp and remaining minutes. This is an estimate and changes as printer telemetry changes. It is hidden when disconnected, paused, blocked, finished, or when no positive remaining time is available.
