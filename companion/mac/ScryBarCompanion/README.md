# ScryBar Companion

macOS companion app in Swift for feeding `Now Playing` metadata to ScryBar.

## Current scope

- SwiftUI macOS app
- LAN discovery via `_scrybar._tcp`
- Manual `host/IP + port` fallback
- `System` provider via macOS `MediaRemote.framework` for the active system-wide Now Playing session
- `Mock` provider for UI/pipeline testing
- `Music.app` provider via AppleScript for first real-source integration
- JSON payload preview
- `POST /api/now-playing` transport contract wired to the firmware

## Open in Xcode

```bash
cd companion/mac/ScryBarCompanion
open ScryBarCompanion.xcodeproj
```

The repo also keeps the XcodeGen spec in `project.yml`, so if the project file ever
needs regeneration:

```bash
cd companion/mac/ScryBarCompanion
xcodegen generate
```

Or build from Terminal:

```bash
cd companion/mac/ScryBarCompanion
swift build
```

## Payload contract

The companion currently sends this JSON shape:

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
  "updatedAt": "2026-03-18T16:00:00Z"
}
```

The `System` provider fills that payload from the same macOS Now Playing layer that feeds Control Center / menu bar media controls, including:

- title
- artist
- album
- playback state
- app/source name
- artwork bytes staged to a temporary local file URL for debugging

## Notes

- The firmware now advertises `_scrybar._tcp` over Bonjour/mDNS and exposes `GET/POST /api/now-playing` on port `8080`.
- Typical discovery result on the current device looks like `ScryBar DB1C` at `scrybar-db1c.local:8080`.
- `System` uses a private Apple framework (`MediaRemote.framework`), which is acceptable for this local GitHub-side companion but may break across macOS updates.
- Source/app naming is best-effort: some system sessions expose `displayName` / `bundleIdentifier`, some expose only the track metadata.
- Manual target entry is the intended fallback when discovery does not find the bar immediately.
- `Music.app` access may trigger macOS Automation permission prompts the first time it runs.
