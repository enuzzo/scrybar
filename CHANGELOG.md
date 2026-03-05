# Changelog

## 2026-03-05

### Firmware DB-M0-r151
- Restored AUX/RSS behavior without replacing user RSS feeds.
- Added dedicated `WIKI` view (`VIEW3` / `VIEWWIKI`) separate from AUX/RSS.
- Wiki view now follows AUX interaction model:
  - `SKIP` -> next article
  - `NXT` -> next feed family
  - `QR` only on demand
- Added Wiki content enrichment and media UX:
  - article summary + title rendering
  - right-side remote thumbnail when available
  - feed-family badge/label for clearer context (`Featured`, `On this day`, `Wikinews`)
- Navigation updates:
  - `VIEWFIRST` / `VIEWLAST`
  - physical `BOOT` short press mapped to jump `HOME`
  - `PWR` short press keeps screensaver behavior

## 2026-02-13

### Firmware DB-M0-r132
- GPT `ASK` flow now forces response variability with per-request random seed in prompt.
- Updated GPT prompt profile for less repetitive/truncated answers:
  - chat `temperature` increased to `0.8`,
  - default ask now requests random number + random quote in Italian.
- Increased audio recording window from `4s` to `20s` for mic-to-GPT tests.
- Reduced visible truncation in GPT panel:
  - enlarged answer buffer (`g_gptPanelText`) and question buffer (`g_gptLastQuestion`),
  - kept higher TTS char cap for longer spoken replies.

### Firmware DB-M0-r131
- Added `GPT` as a new LVGL page (`VIEW3` / `VIEWGPT`) with dedicated header/body placeholder panel.
- Extended swipe navigation and page drag/slide logic from 3 pages to 4 pages (`INFO -> HOME -> AUX -> GPT`).
- Updated serial help output to include the new GPT view commands.

### Firmware DB-M0-r130
- Screensaver idle timeout is now power-aware:
  - `60 min` on USB-C,
  - `30 min` on battery.
- `SAVERSTAT` now reports the active idle timeout in ms.

### Firmware DB-M0-r129
- Screensaver anti-retention update:
  - field/grass line now scrolls slowly,
  - tiny tree marker drifts with the field,
  - footer date/time now applies periodic micro-jitter offsets.

### Firmware DB-M0-r128
- Fixed right-facing moving cow eye spacing to `-e e -`.
- Updated thought visibility timing to `15s ON / 10s OFF` (25s cycle).

### Firmware DB-M0-r127
- Tightened screensaver thought wrapping boundary:
  - max width reduced to ~56% screen,
  - explicit wrap set to 28 chars / line (max 3 lines).

### Firmware DB-M0-r126
- Added explicit quote word-wrap (max 34 chars per line, max 3 lines) to prevent right-edge overflow in screensaver thoughts.

### Firmware DB-M0-r125
- Thought text max width reduced to ~65% of screen to force more frequent 2-3 line wrapping.

### Firmware DB-M0-r124
- Thought text width reduced to ~72% of screen to favor 2-3 line wrapping with small monospace font.

### Firmware DB-M0-r123
- Screensaver thought width cap changed from fixed `90px` to responsive `90%` of screen width for readability.
- Kept text-only thought style plus dashed underline (no balloon box).

### Firmware DB-M0-r122
- Screensaver thought style simplified:
  - removed balloon box, now text-only plus dashed underline,
  - forced max thought width to 90px.

### Firmware DB-M0-r121
- Balloon wrap tuning: reduced maximum bubble width to improve line balance and avoid right-edge glyph artifacts.

### Firmware DB-M0-r120
- Screensaver balloon text flow improved: removed forced line breaks from quotes for cleaner natural wrapping.
- Replaced quote set with the new 40 philosophical/cynical lines.
- Reduced balloon text spacing to near-zero extra leading for denser, cleaner rendering.

### Firmware DB-M0-r119
- Screensaver balloon now auto-sizes to quote length (no oversized fixed box).
- Cow thought text refreshed with 40 more sarcastic lines.
- Right-facing cow eye alignment tuned (`-e e`) for consistent gaze.
- Balloon readability slightly increased with modest mono spacing bump (+~20% perceived size).
- Stars shifted to a warmer/yellow tint for softer night mood.

### Firmware DB-M0-r118
- Screensaver cow updated using user-provided ASCII files for both directions.
- Cow raised slightly and spacing reset to avoid stretched look.
- Balloon typography tuned to mid-size mono style with:
  - thicker border (`2px`),
  - increased inner padding,
  - moderate line spacing.

### Firmware DB-M0-r117
- Screensaver cow tuning:
  - reduced perceived size by lifting/compacting placement and tighter monospace spacing,
  - adjusted right-facing eye row alignment.
- Balloon text switched to larger monospace font with increased line spacing.

### Firmware DB-M0-r116
- Screensaver cow size reduced and repositioned for better proportion on panel.
- Balloon text font switched to monospaced glyphs.
- Cow mirrored sprite refined to stabilize eye alignment in both directions.

### Firmware DB-M0-r115
- Screensaver stars slowed down further for softer twinkle (independent timing remains async).
- Balloon upgraded to comic style:
  - white dashed border,
  - dark translucent fill,
  - tail marker pointing downward toward the cow.
- Balloon visibility cycle changed to:
  - hidden for 15s,
  - visible for 10s,
  - repeat.
- Expanded philosophical quote pool to ~40 Italian lines with longer wording.

### Firmware DB-M0-r113
- Fixed screensaver footer placement: date/time is now anchored reliably to bottom-right after text updates.

### Firmware DB-M0-r112
- Screensaver typography/layout refinement:
  - switched cow rendering to true mono bitmap font path (`unscii_16`) to reduce stretched look,
  - cow color set to white,
  - grass simplified to a single row.
- Removed bottom hint text and added compact date/time footer at bottom-right.
- Star glyph rendering remains sparse and asynchronous with mono-safe glyphs.

### Firmware DB-M0-r111
- Screensaver star glyph compatibility fix:
  - switched twinkle glyphs from `*` to a safe set (`.`, `:`, `o`) to avoid missing-glyph rendering on the current mono font.

### Firmware DB-M0-r110
- Screensaver star bootstrap tweak:
  - ensures at least one lit star per sky row at saver start for better vertical distribution,
  - keeps independent timing and gentle fade behavior.

### Firmware DB-M0-r109
- Screensaver star tuning:
  - keeps stars asynchronous and sparse (max 2 per row),
  - initializes a small random subset already lit to avoid empty-sky first frames,
  - preserves slow independent fade cadence.

### Firmware DB-M0-r108
- Screensaver stars redesigned for a softer look:
  - independent per-star fade timing (no global blinking),
  - max 2 stars per row, sparse and random placement,
  - star field extended vertically to stop about 3 text rows above the field.
- Fixed footer hint text readability (`touch to start`).
- Added cow speech balloon with rotating short philosophical quotes in Italian (refresh every ~10s).

### Firmware DB-M0-r107
- Screensaver cow sprite updated to the new ASCII art supplied by user.
- Added automatic sprite mirroring when movement direction changes, so the cow faces the walk direction.

### Firmware DB-M0-r106
- Screensaver behavior refinement:
  - fixed wake logic to avoid touch just “refreshing” saver,
  - added wake guard to prevent immediate re-entry right after wake.
- New minimal ASCII scene:
  - wider star sky (multi-row), random sparse twinkle with pseudo-fade levels,
  - full-width field with grass + small tree detail,
  - cow movement changed from constant float to horizontal short-step bursts with random pause (1..6s).

### Firmware DB-M0-r105
- Stabilized screensaver wake detection:
  - added stricter raw-touch filters (reject phantom/out-of-panel points),
  - added short raw-touch debounce before wake.
- Keeps touch wake responsive while avoiding immediate accidental saver exit.

### Firmware DB-M0-r104
- Screensaver refinement for testing and style:
  - idle timeout reduced to 10s (`SCREENSAVER_IDLE_MS=10000`) for quick validation.
  - minimal ASCII scene added (star sky + field + moving cow) using monospaced font.
  - touch wake path hardened with raw touch-presence fallback while saver is active.

### Firmware DB-M0-r103
- Simplified energy saver to a binary policy only:
  - `USB-C`: backlight 100%
  - `BATTERY`: backlight reduced (`ENERGY_BACKLIGHT_ON_BATTERY`, default 72%)
- Removed energy-driven weather/RSS refresh scaling (refresh behavior unchanged).
- Added anti-retention screensaver:
  - auto-start after inactivity (`SCREENSAVER_IDLE_MS`, default 90s),
  - moving pixel-style cow across variable heights,
  - wake on touch (and shake when IMU loop is enabled).
- Added serial helper commands: `SAVERON`, `SAVEROFF`, `SAVERSTAT`.

### Firmware DB-M0-r102
- INFO stats tuning:
  - removed duplicate `ip` line from `net` card (IP remains in header).
  - restored visual breathing space under `[net]` and `[system]`.
- Energy saving first integration (battery-aware):
  - automatic backlight policy by power profile (`AC`, `BATT`, `ECO`, `ULTRA`).
  - weather/RSS refresh and retry intervals now scale up on battery to reduce consumption.
  - runtime profile switch logs on serial (`[ENERGY] ...`).

### Firmware DB-M0-r101
- Fixed LVGL recolor parsing on battery visual line in INFO stats.
- Battery bars now use recolor-safe glyphs (`=`/`.`) to avoid showing raw hex color codes.

### Firmware DB-M0-r100
- Added at-a-glance battery visual in INFO stats: colored level bars (`[-----]` to `[#####]`) plus charging marker (`+CHG`).
- Battery line now communicates level immediately without reading percentages first.

### Firmware DB-M0-r099
- INFO panel readability pass: switched stats body to a larger, cleaner sans font for easier at-a-glance reading.
- Added real-time `src` line under power status to show current power source (`USB-C` or `BATTERY`).
- Improved plug/unplug responsiveness for power source detection with a faster slope hint while keeping stable charging trend logic.
- Tightened stats text layout (removed extra blank rows) to keep all fields visible.

### Firmware DB-M0-r098
- INFO panel now shows Wi-Fi SSID and power status (`CHARGING`/`BATTERY` + percentage).
- Wi-Fi reconnection is now non-blocking and cyclic: each configured SSID is tried for 10s, then the firmware moves to the next one continuously until handshake.
- Boot flow no longer depends on immediate Wi-Fi success: LVGL UI starts even offline (no more indefinite logo wait when network is unavailable).
- Added deferred NTP sync in loop once Wi-Fi becomes available.

## 2026-02-12

### Firmware DB-M0-r097
- Set canonical physical orientation to USB-C/power on the left (speaker top, mic bottom).
- Added `DISPLAY_FLIP_180` runtime config flag and enabled it by default.
- Applied 180deg panel flip on ESP LCD backend and aligned touch mapping accordingly.

### Firmware DB-M0-r096
- Tuned Tron Grid for mobile: brighter lines/glow, higher grid plane, and stronger horizon visibility.
- Simplified visual hierarchy by removing outer wrapper chrome (`hero`/`panel`) while keeping inner cards.
- Increased UI translucency so animated background remains visible without hurting readability.

### Firmware DB-M0-r095
- Integrated animated Tron Grid background into Web UI (`fx-grid` layer with plane/glow/horizon/scanline/vignette).
- Added reduced-motion support for accessibility (`prefers-reduced-motion`).
- Applied glass-style transparency treatment to key Web UI sections.

### Firmware DB-M0-r094
- Fixed regression where `ScryBar Stats` labels could disappear (black cards with no text).
- Stabilized INFO stats typography using visible monospace fallback (`LV_FONT_UNSCII_8`) with recolor tags enabled.
- Kept colored section labels (`[net]` cyan, `[system]` magenta) and compact `key: value` formatting.
- Confirmed clean compile/upload workflow with explicit build path and no stale cache artifacts.

### Firmware DB-M0-r093
- Tried intermediate visual scaling for monospace text (later reverted in r094 due rendering side effects).

### Firmware DB-M0-r092
- Increased stats monospace readability and added extra spacing under section headers.

### Firmware DB-M0-r091
- Introduced monospace style for INFO stats blocks.
- Colored section tokens for technical readability (`net/system`).
- Tightened separator spacing around `:` in stats lines.

### Firmware DB-M0-r090
- Unified INFO header typography with other views (title + endpoint).
- Darkened INFO header background and set content background to full black.

### Firmware DB-M0-r089
- RSS AUX controls stabilized:
  - `NXT` jumps to next feed block.
  - `SKIP` advances to next item.
- Added stronger touch hit areas and visual press feedback parity across `NXT/SKIP/QR`.
- Added fixed-top status message for web UI and improved `/reload` behavior using current form state.
- Added WebUI QR in INFO system card and compacted DNS display to single line.
