# Now Playing Companion Gotchas

Stable notes for the macOS companion that feeds ScryBar `Now Playing`.

## Current Architecture

- Firmware advertises `_scrybar._tcp` over Bonjour/mDNS.
- Firmware exposes `GET/POST /api/now-playing` on the web config port.
- macOS companion discovers the bar automatically, with manual `host/IP + port` fallback.
- Default companion provider is the macOS system-wide now-playing feed via private `MediaRemote.framework`.

## MediaRemote Symbols Validated

These symbols were confirmed present and usable on a current macOS system:

- `MRMediaRemoteGetNowPlayingInfo`
- `MRMediaRemoteGetNowPlayingApplicationPlaybackState`
- `MRMediaRemoteGetNowPlayingClient`
- `MRMediaRemoteRegisterForNowPlayingNotifications`
- `MRMediaRemoteUnregisterForNowPlayingNotifications`
- `MRMediaRemoteSendCommand`

Useful metadata keys observed in the now-playing dictionary:

- `kMRMediaRemoteNowPlayingInfoTitle`
- `kMRMediaRemoteNowPlayingInfoArtist`
- `kMRMediaRemoteNowPlayingInfoAlbum`
- `kMRMediaRemoteNowPlayingInfoDuration`
- `kMRMediaRemoteNowPlayingInfoElapsedTime`
- `kMRMediaRemoteNowPlayingInfoPlaybackRate`
- `kMRMediaRemoteNowPlayingInfoArtworkData`
- `kMRMediaRemoteNowPlayingInfoArtworkMIMEType`
- `kMRMediaRemoteNowPlayingInfoArtworkIdentifier`

## Gotchas

- `MRMediaRemoteGetNowPlayingInfo` callback should be dispatched on a background queue in the companion; blocking the main queue while waiting for the callback is unreliable.
- `duration` can arrive as either numeric or string-like payload, so the bridge should coerce both.
- `client` metadata is best-effort. Some sessions expose `displayName` and `bundleIdentifier`; some expose neither even when the track metadata is valid.
- Artwork bytes are available directly from the system feed. Current companion code stages them to a temporary local file URL for debugging, not yet for firmware transport.
- `MediaRemote.framework` is private Apple API. Good fit for a local GitHub-side companion, bad fit for App Store assumptions.

## Firmware Interaction Notes

- Firmware keeps the last live payload on screen even after it goes stale.
- Firmware `IN SYNC` badge depends on recent payload refresh; stale payloads remain visible but should flip out of sync after the configured TTL.
- Current live payload path updates title, artist, source, playback timing, and sync state; live artwork transport is still pending.

## Next Session Focus

- Ship artwork from companion to firmware instead of leaving it local-only.
- Use the same `MediaRemote` layer for `back / pause / next`.
- Improve best-effort source/app labeling when `MRMediaRemoteGetNowPlayingClient` is incomplete.
