# Claude Handoff: TIDAL Now Playing Bug In macOS Companion

Date: 2026-03-19
Project: `scrybar`
Area: `companion/mac/ScryBarCompanion`
Current visible app version/build: `v0.1.1 (2)`

## Objective

Fix the macOS companion so that `Provider = System` correctly shows the currently playing TIDAL track in the UI and produces a correct now-playing payload.

The user-visible failure is:

- TIDAL is actively playing a song.
- The companion UI still shows placeholder values like `—`, empty album, `Paused`, and `No system now-playing session is currently exposed by macOS.`
- The specific real track the user expects to see is:
  - Title: `Nanai`
  - Artist: `Mala Rodríguez`
  - Album: `Malamarismo`

Important constraint from the user:

- Leave the current project state as-is except for the bug fix.
- Do not refactor gratuitously.
- Keep the version badge behavior. It was added intentionally so we can verify the correct build is running.

## Current State Of The Code

The relevant files are:

- `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/Providers.swift`
- `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/AppModel.swift`
- `companion/mac/ScryBarCompanion/Sources/ScryBarCompanion/NowPlayingDashboardView.swift`
- `companion/mac/ScryBarCompanion/project.yml`

The current provider wiring in `AppModel.swift` is:

- `System` provider is a fallback chain:
  1. `SystemNowPlayingProvider()`
  2. `TidalNowPlayingProvider()`

This means:

- We first try `MediaRemote`.
- If `MediaRemote` returns nothing useful, we try the custom local TIDAL parser.

The current `TidalNowPlayingProvider` has already been partially upgraded to use multiple candidate sources:

- fresh `Local Storage` playerState
- `IndexedDB` playback events
- `connectqueue` cache pages and queue position cache

This logic is in `Providers.swift` inside `TidalNowPlayingProvider`.

The current UI also shows a version/build badge at the top-left of the dashboard header:

- File: `NowPlayingDashboardView.swift`
- It reads from bundle keys:
  - `CFBundleShortVersionString`
  - `CFBundleVersion`

The current app version/build is defined in `project.yml`:

- `MARKETING_VERSION: "0.1.1"`
- `CURRENT_PROJECT_VERSION: 2`

## What Has Already Been Verified

## 1. MediaRemote is not reliable here for TIDAL

The app originally relied on `MediaRemote` for `Provider = System`.

Observed behavior on this machine:

- TIDAL is playing.
- The companion still gets no valid now-playing metadata from `MediaRemote`.
- Therefore the UI falls through to the custom TIDAL fallback.

So the bug is not just presentation. The underlying data source is failing or incomplete for TIDAL on this Mac.

## 2. The old TIDAL fallback was wrong

Originally, the custom TIDAL fallback did this:

1. Read the newest `playerState` from:
   - `~/Library/Application Support/TIDAL/Local Storage/leveldb`
2. Use that `mediaId` as the current track
3. Decorate it with metadata from cached HTTP responses

This is wrong on this machine because `Local Storage` is stale.

## 3. Terminal verification shows stale Local Storage

This command was used:

```sh
strings -a ~/Library/Application\ Support/TIDAL/Local\ Storage/leveldb/* | \
rg -n '_TIDAL_[0-9]+playerState|currentlyPlaying|mediaId|setTime'
```

What it showed:

- The visible `playerState` records are stale.
- One of the main stale tracks found there was `493754886` corresponding to an older track we had seen before (`Abbrusc`).

So `Local Storage` cannot be treated as authoritative.

## 4. IndexedDB contains newer TIDAL playback evidence

This command was used:

```sh
strings -a ~/Library/Application\ Support/TIDAL/IndexedDB/https_desktop.tidal.com_0.indexeddb.leveldb/000681.log | \
rg -n -C 2 'playback_session|actualProductId|sessionProductId|sourceId|Nanai|Malamarismo|Mala Rodr'
```

Important findings:

- There are recent `2026-03-19` playback events.
- Around local time `11:21:14`, `11:21:19`, and `11:21:24`, TIDAL emitted playback-related events.
- Recent IDs found there include:
  - `493754886`
  - `289120234`
- A key event observed:
  - `playback_session`
  - `actualProductId = 289120234`
  - `sourceId = 22ffb200-cb07-4069-8d86-fc1435583480`
  - timestamp around `1773915684608`

This proves that fresher playback activity exists outside `Local Storage`.

## 5. Cache data confirms the user’s expected track metadata

This command was used:

```sh
find ~/Library/Application\ Support/TIDAL/Cache/Cache_Data -type f ! -name index-dir -maxdepth 1 -print0 | \
xargs -0 strings -a | \
rg -n -C 2 '22ffb200-cb07-4069-8d86-fc1435583480|54181791|Nanai|Malamarismo|Mala Rodr'
```

Important findings:

- There is a playlist payload for:
  - `https://desktop.tidal.com/v1/playlists/22ffb200-cb07-4069-8d86-fc1435583480/items?...`
- Inside that payload:
  - track `54181791`
  - title `Nanai`
  - artist `Mala Rodríguez`
  - album `Malamarismo`
  - cover `493c3fc8-7232-436a-b0a9-9b02f0889e4e`

This is the exact track the user showed in the screenshot.

## 6. Queue cache also exists, but its current-position state is stale too

Commands used:

```sh
find ~/Library/Application\ Support/TIDAL/Cache/Cache_Data -type f ! -name index-dir -maxdepth 1 -print0 | \
xargs -0 strings -a | \
rg -n -C 2 'active":"true"|position":"[0-9]+"|22ffb200-cb07-4069-8d86-fc1435583480|54181791|Nanai|Malamarismo'
```

Important findings:

- Queue payloads exist for:
  - `https://connectqueue.tidal.com/v1/queues/d0d9984f-8582-4a29-b7af-c2f42ca01daa/items?...`
- That queue points to source playlist:
  - `22ffb200-cb07-4069-8d86-fc1435583480`
- It contains:
  - `289120234` at `original_order = 0`
  - `54181791` (`Nanai`) at `original_order = 10`
- There is also a queue state payload:
  - `https://connectqueue.tidal.com/v1/queues/d0d9984f-8582-4a29-b7af-c2f42ca01daa?...`
  - `properties.position = "0"`

This means:

- Queue metadata exists.
- But the cached queue position for that queue still says `0`, which would resolve to `Sombrilla`, not `Nanai`.
- So queue position alone is also stale in cache.

## 7. Session Storage did not yield anything useful

Commands used:

```sh
strings -a ~/Library/Application\ Support/TIDAL/Session\ Storage/* | \
rg -n -C 2 'Nanai|Malamarismo|Mala Rodr|currentlyPlaying|mediaId|playerState|playback|queue|position|sessionProductId|actualProductId'
```

Result:

- no useful hits

So Session Storage is not currently a practical source for the fix.

## 8. Accessibility fallback via System Events is currently blocked

Commands attempted:

```sh
osascript -e 'tell application "System Events" to (name of every process whose background only is false)'
osascript -e 'tell application "System Events" to tell process "TIDAL" to get name of every window'
```

Result:

- both fail with `-10827`

So UI scripting is not currently available as a dependable fallback in this environment.

## What Was Changed Already

These changes are already in the tree and should be preserved unless you intentionally improve them:

## 1. Visible build badge

Added to `NowPlayingDashboardView.swift` so the user can verify the running build.

Expectation:

- Every future significant change should continue bumping build/version in `project.yml`.
- The UI should keep showing the current build at top-left.

## 2. TIDAL provider upgraded to candidate-based ranking

Current intent of `TidalNowPlayingProvider`:

- do not trust stale `Local Storage`
- collect candidates from multiple sources
- rank by confidence and recency
- resolve the winner to track metadata from cached HTTP responses

Current candidate sources:

- `freshPlayerStates()`
- `indexedDBPlaybackCandidates()`
- `queuePlaybackCandidates()`

This is a meaningful improvement, but it is not proven to solve the real-world `Nanai` case yet.

## The Actual Remaining Problem

Even after moving away from the naive `Local Storage -> metadata` pipeline, we still do not have a guaranteed “live truth” source for the exact current TIDAL track.

We now know:

- `MediaRemote` is not dependable here.
- `Local Storage` is stale.
- `Queue position` in cache can also be stale.
- `IndexedDB` has fresher playback events, but the freshest ID we observed in those logs during this investigation was `289120234`, not `54181791`.
- The exact track `Nanai` absolutely exists in cached playlist metadata.

So the remaining task is:

- identify a reliable source of the true current TIDAL track on this machine
- or, if impossible, build the best practical heuristic so that the companion chooses the correct live track more often instead of falling back to stale data or placeholder state

## What Claude Should Fix

Please solve this bug:

When TIDAL is currently playing a track, `Provider = System` in the macOS companion should show the correct track metadata, including title, artist, and album, instead of placeholder values.

Specifically, in the reproduction we care about, the companion should show:

- `Nanai`
- `Mala Rodríguez`
- `Malamarismo`

and not:

- `—`
- empty album
- `Paused`
- `No system now-playing session is currently exposed by macOS.`

## Strong Suggestions For The Fix

## 1. Start from the current `TidalNowPlayingProvider`

Do not throw it away blindly.

There is already meaningful work in place:

- candidate ranking
- multi-source lookup
- queue parsing
- IndexedDB parsing

Improve it instead of restarting from zero.

## 2. Verify the current behavior end-to-end

Please test which candidate actually wins at runtime and why.

Most likely questions to answer:

- Does `SystemNowPlayingProvider` still return `nil` for TIDAL?
- Which candidate wins inside `TidalNowPlayingProvider` for the current reproduction?
- Why does that winner not become `54181791 / Nanai`?
- Is there another local TIDAL store not yet being parsed that contains the truly current item?

## 3. If necessary, add instrumentation temporarily

It may help to log:

- candidate source
- media ID
- timestamp
- confidence
- whether metadata resolved

The key debugging need is to see:

- what candidate list is being built
- which candidate is selected
- why the selected candidate still disagrees with the actual player UI

## 4. Most promising directions

These appear most promising based on current evidence:

- Improve `IndexedDB` parsing beyond the current regexes.
  - There may be additional event types or structures that identify the actual active track more accurately.
- Search more of the TIDAL local data model for a fresher active queue/item state.
  - Current queue payloads have stale `position`.
  - There may be another cached response containing the active item or current index.
- If a better live local source does not exist, implement a more practical heuristic.
  - Example: prefer the newest high-confidence candidate that also has compatible metadata and recency.
  - But do not ship a heuristic that is obviously brittle without documenting its tradeoffs.

## 5. Preserve the UI behavior

Please keep:

- the dark UI redesign
- the version/build badge in the header

The problem to solve is the metadata bug, not the layout.

## Build / Verification Commands

These worked at the time of handoff:

```sh
cd companion/mac/ScryBarCompanion
xcodegen generate
swift build
xcodebuild -project ScryBarCompanion.xcodeproj -scheme ScryBarCompanion -configuration Debug -derivedDataPath /tmp/ScryBarCompanionDerivedData build
```

## Recommended Acceptance Criteria

Consider the bug fixed only if:

1. Launching the companion with TIDAL actively playing shows real metadata instead of placeholders.
2. The specific reproduction with `Nanai / Mala Rodríguez / Malamarismo` works when that track is actively playing.
3. `Provider = System` remains the path the user uses. Do not require them to manually switch to a TIDAL-specific provider.
4. The app still builds successfully.
5. The version badge remains visible and is bumped again if you change the app.

## Notes For Claude

- Please do not assume `MediaRemote` is enough. It is not enough here.
- Please do not assume `Local Storage playerState` is live. It is stale here.
- Please reason from the actual local files and timestamps on this Mac.
- If you introduce a heuristic because there is no true live source, document that clearly in the code or in a short note.

## Short Summary

The bug is real. The app is not merely hiding data.

The machine has:

- stale `Local Storage`
- useful but incomplete `IndexedDB`
- useful but stale queue position cache
- correct playlist metadata containing `Nanai / Malamarismo`
- no trustworthy live source identified yet

Your job is to bridge that last gap and make `Provider = System` show the real TIDAL track reliably.
