# Next Session — 2026-03-09

## Where We Left Off

- **r178** is live and stable on device.
- Partition migrated to `noota_app15M_16MB` — flash at 18% (~2.9MB used / 16.6MB available).
- Napoletano (`nap`) removed entirely from all code paths.
- Wikipedia viewer now has:
  - Independent language selector (`wiki_lang` in NVS, 8 real languages)
  - Random Article (slot 2) via REST API `/api/rest_v1/page/random/summary`
  - OTD bug fixed (parse all items, keep last N)

## What To Do Next

### 1. Wiki Random Article Thumbnail (priority)

The Random Article API returns a `thumbnail.source` URL. The infrastructure to display it is partially there (`extractJsonStringField` parses the URL, `thumbUrl` is populated in `RssItem`). What's missing:

- **JPEG decoder**: enable `esp_jpeg_decode` (ESP-IDF built-in, no library needed). Flash space is no longer a constraint.
- **Display pipeline**: decode JPEG into RGB565 buffer, blit to LVGL image widget in the WIKI card.
- **Sizing**: Wikipedia thumbnails are typically 320px wide. Scale down to fit the right side of the 640x172 card (~120x120 or similar).
- Look at how the old thumbnail infrastructure worked before r158 simplification (git log/diff around that commit for reference).

### 2. Backlog

See `memory/backlog.md` for other ideas (DOOM, Crypto ticker, Snake, Pomodoro, etc.).
