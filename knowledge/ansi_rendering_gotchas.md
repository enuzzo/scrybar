# ANSI/BBS Art Rendering — Gotchas & Lessons Learned

Comprehensive recipe for rendering ANSI (.ANS) art correctly on embedded displays.
Built from iterative testing on ScryBar (ESP32-S3, 172px wide AXS15231B) and
confirmed against the [icy_tools](https://github.com/mkrueger/icy_tools) reference
implementation (Rust).

---

## 1. Color Mapping: ANSI SGR to CGA Palette

**The trap:** ANSI SGR codes 30-37 do NOT map linearly to CGA palette indices.

```
ANSI:  Black=0  Red=1  Green=2  Yellow=3  Blue=4  Magenta=5  Cyan=6  White=7
CGA:   Black=0  Blue=1 Green=2  Cyan=3    Red=4   Magenta=5  Brown=6 LGray=7
```

**Lookup table (ANSI index → CGA index):**
```cpp
static const uint8_t kAnsiToCga[8] = {0, 4, 2, 6, 1, 5, 3, 7};
```

This matches icy_tools `ANSI_COLOR_OFFSETS`. Without it, red appears as blue and
vice versa — the most common ANSI rendering bug.

**Bright colors (SGR 90-97 / 100-107):**
```cpp
fg = kAnsiToCga[v - 90] + 8;   // bright foreground
bg = kAnsiToCga[v - 100] + 8;  // bright background
```

**Bold (SGR 1) and Blink (SGR 5):**
Both map to "bright" in CGA terms: `fg | 8`. Blink was never implemented on most
BBS terminals; it was universally interpreted as bright background or bright
foreground. We map both to bright foreground (bold = true → fgResolved = fg | 8).

---

## 2. Deferred Wrap (Pending Wrap / DOS Autowrap)

**The trap:** DOS terminals do NOT wrap immediately when the cursor reaches the last
column. They set a "pending wrap" flag. The wrap executes only when the *next
printable character* arrives.

**Why it matters:** Most BBS art draws character 80 (0-indexed col 79) followed by
CR+LF. Without deferred wrap:
- Col 79 char → immediate wrap → row advances
- CR+LF → row advances again
- Result: every row is double-spaced, art is completely broken

**Correct implementation:**
```cpp
// After drawing a character:
++st.col;
if (st.col >= wrapCols) {
    st.pendingWrap = true;   // do NOT advance row yet
}

// Before drawing the next character:
if (st.pendingWrap) {
    st.col = 0;
    ++st.row;
    st.pendingWrap = false;
}

// CR clears pending wrap without advancing row:
if (c == '\r') { st.col = 0; st.pendingWrap = false; }

// LF advances row and clears pending wrap:
if (c == '\n') { ++st.row; st.col = 0; st.pendingWrap = false; }
```

**CSI cursor movement (H, f, A, B, C, D) also clears pendingWrap.**

This is the single most impactful fix for ANSI rendering. Confirmed matching
icy_tools `screen.rs` behavior.

---

## 3. SAUCE Record Parsing

The last 128 bytes of an .ANS file may contain a SAUCE record (Standard Architecture
for Universal Comment Extensions).

**Signature:** bytes start with ASCII `SAUCE00` at offset `len - 128`.

**Key fields:**
- Width (cols): offset 96-97, little-endian uint16
- Height (rows): offset 98-99, little-endian uint16
- Title: offset 7, 35 chars
- Author: offset 42, 20 chars

**Gotcha: SAUCE height is often wrong.** Many files report height=25 (one screen)
but contain 50-200 rows of content. Always allocate a generous row buffer
(e.g. min 2000 rows) regardless of SAUCE height.

**Gotcha: SAUCE width can exceed display.** Some art uses width=180 or other
non-standard values. Use SAUCE width for parser wrap logic, but clip to display
width during flush.

**SUB character (0x1A):** SAUCE uses Ctrl-Z as end-of-file marker. Stop parsing
displayable content at the first 0x1A byte.

---

## 4. CP437 Font (IBM VGA 8x16)

BBS art assumes Code Page 437 encoding. Characters 0x80-0xFF are block/line-drawing
glyphs critical for the art:

- 0xB0-0xB2: light/medium/dark shade blocks
- 0xDB-0xDF: full block, lower/upper half blocks
- 0xC0-0xDA: box-drawing single lines
- 0xB3-0xBF: box-drawing double lines

**Source:** Extract the 8x16 bitmap from any IBM VGA ROM dump or use the widely
available `cp437_8x16.h` arrays.

**Downsampling:** If the display can't fit 8x16 at 80 columns (e.g. 172px wide →
fontW=2, fontH=4), use area-average downsampling from the 8x16 master bitmap.
Each destination pixel averages a tile of source pixels.

---

## 5. CSI Sequence Parsing Edge Cases

### Private Parameter Prefix
Sequences like `ESC[?7h` (enable auto-wrap) have a `?` prefix before the numeric
parameters. Must skip `?`, `<`, `=`, `>` before parsing numbers.

### Intermediate Bytes
Bytes 0x20-0x2F between parameters and the final byte are intermediate bytes.
Consume them silently and keep looking for the final byte (0x40-0x7E).

### Non-CSI Escape Sequences
Not all ESC sequences start with `[`. Common ones:
- `ESC(B` — select ASCII charset (2 bytes after ESC)
- `ESC)0` — select line-drawing charset (2 bytes after ESC)
- `ESC M` — reverse line feed (1 byte after ESC)

If you only handle `ESC[...`, the `(` and `B` will be drawn as visible characters,
corrupting the display.

### SGR Reset
`ESC[m` and `ESC[0m` both mean full reset: fg=7, bg=0, bold=false.

---

## 6. Buffer Width vs Display Width

**The trap:** Some art uses CSI cursor positioning (`ESC[row;colH`) to place
characters past column 80. If the render buffer is exactly display-width, these
characters are silently clipped, and subsequent text appears shifted.

**Example:** misfit-blocked.ans positions 'B' at column 86. At fontW=2, that's
pixel 172 — exactly at the buffer edge (172 + 2 > 172), so it's not drawn.
Result: "LOCKTRONICS" instead of "BLOCKTRONICS".

**Fix:** Allocate a render buffer wider than the display:
```cpp
const int renderW = max(DB_NATIVE_W, (artCols + 16) * fontW);
```
During flush, copy only the first `DB_NATIVE_W` pixels per row. The extra columns
keep parser state correct even if those pixels are never shown.

---

## 7. Overlay Corruption (DMA Buffer vs PSRAM)

**The trap:** If you draw a title overlay directly into the PSRAM source buffer,
it permanently modifies the rendered art. Every frame re-dims and re-draws text,
progressively burning the overlay into the art data.

**Fix:** Apply overlays in the DMA staging buffer during flush, never in the
source PSRAM buffer. The DMA buffer is rebuilt from PSRAM each frame, so the
overlay is transient.

---

## 8. macOS Filesystem Case Sensitivity

macOS HFS+/APFS (default) is case-insensitive. `FILE.ANS` and `file.ans` are the
same file. An md5-based duplicate finder will incorrectly report both as duplicates
because they ARE the same file.

When normalizing filenames:
1. Check if source and target are the same inode before deleting
2. Use a two-step rename: `FILE.ANS` → `tmp_file.ans` → `file.ans`
3. Or use `git mv` which handles this correctly

---

## 9. Embedded File Strategy (Flash-Constrained)

For embedding .ANS files in firmware flash (e.g. 3MB app partition):
- Convert each .ANS to a C byte array header (`xxd -i` or custom script)
- Use PROGMEM where applicable (ESP32 handles this via flash-mapping)
- Track a registry array for iteration: `{data, len, filename}`

**Future: deflate compression** (miniz, included in ESP-IDF) can achieve 3:1-5:1
ratio. Workflow: compressed PROGMEM → decompress to PSRAM at load → render → free.

---

## 10. AXS15231B Touch Controller — Scan Rate vs Poll Rate

**The trap:** The AXS15231B is an integrated display+touch controller. The I2C touch
read command (`0xb5 0xab 0xa5 0x5a ...`) clears the touch data buffer on read. If the
firmware polls faster than the controller's internal scan rate (~60Hz), most reads
return "no touch" even while a finger is physically on the screen.

**Symptom:** Touch detected for only 1 frame (2-3ms), then gone. Swipe gestures
appear as zero-displacement taps (dx=0 dy=0 dur=2ms). Particularly visible in ANSI
portrait mode where the main loop runs fast (~500Hz) between 40ms display flushes.

**Fix:** Add a miss counter. Require N consecutive "no touch" frames before declaring
a true release:
```cpp
static uint8_t g_touchMissCount = 0;

// In touch handler — on detection:
if (touched) { g_touchMissCount = 0; /* normal touch-down logic */ return; }

// On miss — bridge gaps between controller scans:
if (g_touchDown) {
    if (++g_touchMissCount < 12) return;  // ~12 frames ≈ 25-50ms hold-off
    g_touchMissCount = 0;
}
// proceed with release logic...
```

This preserves the touch across scan gaps, allowing swipe displacement to accumulate
over the full gesture duration. The hold-off of 12 frames provides enough bridging
without adding noticeable latency to tap detection.

**Also read twice per loop:** `readTouchLogicalPoint` (swipe handler) and
`isAnyTouchPresentRaw` (screensaver) both send the same I2C command. Each read
independently consumes touch data. Keep this in mind if adding more touch consumers.

---

## 11. Reference Implementation

**[icy_tools](https://github.com/mkrueger/icy_tools)** by Mike Krueger is the
authoritative reference for ANSI art rendering. Key files:
- `crates/icy_parser_core/src/ansi/mod.rs` — CSI parser, color handling
- `crates/icy_engine/src/screen.rs` — cursor movement, wrap behavior
- `crates/icy_engine/src/parser_sink.rs` — character output, state management

Color mapping confirmed: `ANSI_COLOR_OFFSETS: [0, 4, 2, 6, 1, 5, 3, 7]` — matches
our `kAnsiToCga` exactly.
