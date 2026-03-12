# ANSI Art Viewer — Archived

Removed from active firmware in r183. Feature was fun but low replay value
(27 files loop in ~2 minutes) and added complexity for little user benefit.

## Contents

- `src_ansi/` — C headers: parser (`ansi_parser.h`), font (`font_ibmvga8x16.h`), file registry (`ansi_files.h`)
- `data_ansi/` — 27 raw `.ans` files (940 KB total), extracted from embedded hex headers
- `extract_ansi_raw.py` — Script to extract raw .ans from hex-encoded C headers
- `provision_ansi.py` — Script to build FAT image and flash to device
- `ansi_rendering_gotchas.md` — Technical notes on CGA palette, CP437, SAUCE parsing
- `2026-03-08-ansi-view.md` — Original implementation plan

## To restore

1. Copy `src_ansi/` back to `src/ansi/`
2. Copy `data_ansi/` back to `data/ansi/`
3. Re-add `#include` directives, `UI_PAGE_ANSI` enum, globals, and functions to `scrybar.ino`
4. Re-add `UI_VIEW_FLAG_ANSI` to view mask system
5. Re-add web UI checkbox and serial VIEWANSI command
6. See git history for the exact code that was removed
