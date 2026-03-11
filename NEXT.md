# Next Session — 2026-03-11

## Where We Left Off

- **r182** is live and stable on device.
- DOOM is integrated as a real extra view, not a placeholder spike:
  - donor: `ducalex/retro-go` -> `prboom-go` only
  - live centered 4:3 framebuffer on the `640x172` display
  - title screen first, `FIRE` starts the core
  - IMU active only in DOOM
  - stable neutral capture after settle window
  - swipe exit, left `USE`, right `FIRE`, center tap recenter
- Partition now uses checked-in `partitions.csv` with `PartitionScheme=custom`.
- DOOM/RSS/WIKI coexistence is stable only with `DB_CHUNK_ROWS=32`.
- Public deep note: `knowledge/doom_integration_gotchas.md`

## What To Do Next

### 1. DOOM gameplay tuning (priority)

- Validate real in-hand control feel during gameplay, not just neutral-at-rest.
- Decide final IMU mapping strength and deadbands for:
  - forward/back
  - turn left/right
- Optionally add small HUD hit/press animations or iconography.

### 2. Wikipedia / RSS polish

- Revisit Random Article thumbnail support only if it can be reintroduced without
  destabilizing heap/TLS behavior.

### 3. Backlog

See `memory/backlog.md` for other ideas (Radio Metadata, Crypto ticker, Snake, Pomodoro, etc.).
