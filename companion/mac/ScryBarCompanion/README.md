# ScryBar Companion

macOS menu-bar app (Swift/SwiftUI) that feeds **Now Playing** metadata and **Mac Stats** to your ScryBar device.

---

## Features

| Feature | Notes |
|---|---|
| **Now Playing** | System-wide playback via `MediaRemote.framework` (same source as Control Center) |
| **Music.app provider** | Direct AppleScript integration for richer metadata |
| **Mac Stats** | CPU/GPU usage %, CPU/GPU temperature, RAM and disk usage |
| **LAN discovery** | Auto-discover ScryBar via `_scrybar._tcp` Bonjour/mDNS |
| **Manual target** | Fallback `host/IP + port` entry |
| **Auto-send** | Pushes updates every second (Now Playing) and every 5 s (Mac Stats) |

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

A prebuilt ad-hoc signed `.app` is kept in `companion/mac/dist/` for convenience.

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
| **Local Network** | mDNS/Bonjour discovery of ScryBar |
| **Automation (Music.app)** | Only prompted when Music provider is selected |

---

## Payload contracts

### `POST /api/now-playing`

```json
{
  "title": "Track title",
  "artist": "Artist name",
  "album": "Album name",
  "source": "Spotify",
  "appName": "Spotify.app",
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

---

## How Mac Stats are collected

| Metric | Source | Notes |
|---|---|---|
| CPU temperature | `macmon pipe -s 1 -i 200` | Subprocess; parses `temp.cpu_temp_avg` JSON |
| GPU temperature | `macmon pipe -s 1 -i 200` | Same subprocess; `temp.gpu_temp_avg` |
| CPU usage % | `host_processor_info(PROCESSOR_CPU_LOAD_INFO)` | Mach API tick delta between polls |
| GPU usage % | `IOAccelerator PerformanceStatistics["Device Utilization %"]` | IOKit iterator; no sudo |
| RAM used / total | `host_statistics64(HOST_VM_INFO64)` + `hw.memsize` sysctl | |
| Disk used / total | `FileManager.default.attributesOfFileSystem(forPath: "/")` | |

Sampling runs on a `Task.detached(priority: .background)` every 5 seconds to avoid blocking the UI thread.

---

## Versioning

Bump `CURRENT_PROJECT_VERSION` in `project.yml` with every published build.

---

## Known issues / caveats

- `MediaRemote.framework` is private; behavior may change across macOS updates.
- Source/app naming is best-effort — some sessions expose `displayName`, others only metadata.
- macmon is a third-party tool. If Homebrew paths differ (e.g. Rosetta Intel prefix), macmon detection may fail; set `MACMON_PATH` env override (planned).
- SMC key-based temperature reading (legacy fallback for Intel Macs) returns `0` on M3 Ultra / M4 — macmon is the only supported path on those chips.
