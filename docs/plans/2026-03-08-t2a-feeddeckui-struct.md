# T2-A: FeedDeckUi Struct Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Consolidate 43 parallel AUX/Wiki globals into two `FeedDeckUi` struct instances, eliminating ~120 duplicate lines and making future deck additions trivial.

**Architecture:** Define `FeedDeckUi` struct with all common widget pointers + state. Replace standalone `g_lvglAux*`/`g_lvglWiki*` globals with `g_auxDeck` and `g_wikiDeck`. Merge the two update functions into `lvglUpdateFeedDeck(FeedDeckUi &d, RssState &c, bool isWiki, bool force)`. Simplify 27 AUX/Wiki/dispatch functions to ~9 using `activeFeedDeck()`. Loop over decks in theme/font functions. Refactor init into `lvglInitFeedDeck`.

**Tech Stack:** Arduino C++, LVGL 8.x, ESP32-S3. No unit tests — verification is `arduino-cli compile`. Compile after every task.

**Build command (run from repo root):**
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi \
  . 2>&1 | tail -5
```
Expected output on success: lines ending with `(70%) of program storage space` and `(48%) of dynamic memory`.

---

## Baseline (before this plan)

From audit of `scrybar.ino` post-r160:

| Symbol block | Lines |
|---|---|
| AUX globals (lines 825–851) | 21 symbols |
| Wiki globals (lines 826, 853–876) | 20 symbols |
| AUX-specific functions | 9 functions (~88 lines) |
| Wiki-specific functions | 9 functions (~70 lines) |
| Feed dispatch helpers | 9 functions (~39 lines) |
| `lvglUpdateAuxRss` | ~196 lines |
| `lvglUpdateWikiDeck` | ~178 lines |
| `lvglApplyThemeStyles` AUX block | ~80 lines |
| `lvglApplyThemeStyles` Wiki block | ~80 lines |
| `lvglApplyThemeFonts` AUX block | ~10 lines |
| `lvglApplyThemeFonts` Wiki block | ~10 lines |
| `initLvglUi` AUX deck init | ~220 lines |
| `initLvglUi` Wiki deck init | ~200 lines |

---

## Global rename mapping

Every `g_lvglAux*` → `g_auxDeck.*` and `g_lvglWiki*` → `g_wikiDeck.*` using this table:

| Old AUX global | Struct member | Old Wiki global |
|---|---|---|
| `g_lvglAuxCard` | `.card` | `g_lvglWikiCard` |
| `g_lvglAuxHeader` | `.header` | `g_lvglWikiHeader` |
| `g_lvglAuxHeaderFill` | `.headerFill` | `g_lvglWikiHeaderFill` |
| `g_lvglAuxTitle` | `.title` | `g_lvglWikiTitle` |
| `g_lvglAuxFeedIcon` | `.feedIcon` | *(nullptr for Wiki)* |
| `g_lvglAuxStatus` | `.status` | `g_lvglWikiStatus` |
| `g_lvglAuxQrBtn` | `.qrBtn` | `g_lvglWikiQrBtn` |
| `g_lvglAuxQrBtnText` | `.qrBtnText` | `g_lvglWikiQrBtnText` |
| `g_lvglAuxRefreshBtn` | `.refreshBtn` | `g_lvglWikiRefreshBtn` |
| `g_lvglAuxRefreshBtnText` | `.refreshBtnText` | `g_lvglWikiRefreshBtnText` |
| `g_lvglAuxNextFeedBtn` | `.nextFeedBtn` | `g_lvglWikiNextFeedBtn` |
| `g_lvglAuxNextFeedBtnText` | `.nextFeedBtnText` | `g_lvglWikiNextFeedBtnText` |
| `g_lvglAuxSourceBadge` | `.sourceBadge` | `g_lvglWikiSourceBadge` |
| `g_lvglAuxSourceBadgeText` | `.sourceBadgeText` | `g_lvglWikiSourceBadgeText` |
| `g_lvglAuxSourceSite` | `.sourceSite` | `g_lvglWikiSourceSite` |
| `g_lvglAuxNews` | `.news` | `g_lvglWikiNews` |
| `g_lvglAuxMeta` | `.meta` | `g_lvglWikiMeta` |
| `g_lvglAuxQrOverlay` | `.qrOverlay` | `g_lvglWikiQrOverlay` |
| `g_lvglAuxQrHint` | `.qrHint` | `g_lvglWikiQrHint` |
| `g_lvglAuxQr` | `.qr` | `g_lvglWikiQr` |
| `g_lvglAuxLastItemShown` | `.lastItemShown` | `g_lvglWikiLastItemShown` |
| `g_lvglAuxLastQrPayload` | `.lastQrPayload` | `g_lvglWikiLastQrPayload` |
| `g_lvglAuxQrModalOpen` | `.qrModalOpen` | `g_lvglWikiQrModalOpen` |

**Not in struct** (standalone globals kept as-is):
- `g_lvglAuxRoot` — page root, used by navigation
- `g_lvglWikiRoot` — page root, used by navigation

---

## Task 1: Define FeedDeckUi struct and instances

**File:** `scrybar.ino`

**Step 1: Insert struct definition**

Find the AUX globals block (around line 825: `static lv_obj_t *g_lvglAuxRoot = nullptr;`).
Insert the struct definition BEFORE the existing globals block:

```cpp
// ── FeedDeckUi — shared widget/state bundle for AUX and WIKI decks ──────────
struct FeedDeckUi {
  lv_obj_t *card         = nullptr;
  lv_obj_t *header       = nullptr;
  lv_obj_t *headerFill   = nullptr;
  lv_obj_t *title        = nullptr;
  lv_obj_t *status       = nullptr;
  lv_obj_t *feedIcon     = nullptr;  // AUX only; nullptr for Wiki
  lv_obj_t *qrBtn        = nullptr;
  lv_obj_t *qrBtnText    = nullptr;
  lv_obj_t *refreshBtn   = nullptr;
  lv_obj_t *refreshBtnText = nullptr;
  lv_obj_t *nextFeedBtn  = nullptr;
  lv_obj_t *nextFeedBtnText = nullptr;
  lv_obj_t *sourceBadge  = nullptr;
  lv_obj_t *sourceBadgeText = nullptr;
  lv_obj_t *sourceSite   = nullptr;
  lv_obj_t *news         = nullptr;
  lv_obj_t *meta         = nullptr;
  lv_obj_t *qrOverlay    = nullptr;
  lv_obj_t *qrHint       = nullptr;
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  lv_obj_t *qr           = nullptr;
#endif
  int16_t   lastItemShown  = -1;
  char      lastQrPayload[280] = {0};
  bool      qrModalOpen    = false;
};
static FeedDeckUi g_auxDeck;
static FeedDeckUi g_wikiDeck;
```

**Step 2: Compile to verify struct is valid**

Run build command. Expected: exit 0. The old globals still exist — no renames yet.

**Step 3: Commit checkpoint**

```bash
git add scrybar.ino
git commit -m "refactor(t2a-1): add FeedDeckUi struct + g_auxDeck/g_wikiDeck instances"
```

---

## Task 2: Migrate AUX globals → g_auxDeck members

**File:** `scrybar.ino`

**Step 1: Rename all AUX widget references**

Using the mapping table, do a global find-and-replace in `scrybar.ino`:

| Replace | With |
|---|---|
| `g_lvglAuxCard` | `g_auxDeck.card` |
| `g_lvglAuxHeader\b` (not HeaderFill) | `g_auxDeck.header` |
| `g_lvglAuxHeaderFill` | `g_auxDeck.headerFill` |
| `g_lvglAuxTitle` | `g_auxDeck.title` |
| `g_lvglAuxFeedIcon` | `g_auxDeck.feedIcon` |
| `g_lvglAuxStatus` | `g_auxDeck.status` |
| `g_lvglAuxQrBtnText` | `g_auxDeck.qrBtnText` |
| `g_lvglAuxQrBtn\b` | `g_auxDeck.qrBtn` |
| `g_lvglAuxRefreshBtnText` | `g_auxDeck.refreshBtnText` |
| `g_lvglAuxRefreshBtn\b` | `g_auxDeck.refreshBtn` |
| `g_lvglAuxNextFeedBtnText` | `g_auxDeck.nextFeedBtnText` |
| `g_lvglAuxNextFeedBtn\b` | `g_auxDeck.nextFeedBtn` |
| `g_lvglAuxSourceBadgeText` | `g_auxDeck.sourceBadgeText` |
| `g_lvglAuxSourceBadge\b` | `g_auxDeck.sourceBadge` |
| `g_lvglAuxSourceSite` | `g_auxDeck.sourceSite` |
| `g_lvglAuxNews` | `g_auxDeck.news` |
| `g_lvglAuxMeta` | `g_auxDeck.meta` |
| `g_lvglAuxQrOverlay` | `g_auxDeck.qrOverlay` |
| `g_lvglAuxQrHint` | `g_auxDeck.qrHint` |
| `g_lvglAuxQr\b` | `g_auxDeck.qr` |
| `g_lvglAuxLastItemShown` | `g_auxDeck.lastItemShown` |
| `g_lvglAuxLastQrPayload` | `g_auxDeck.lastQrPayload` |
| `g_lvglAuxQrModalOpen` | `g_auxDeck.qrModalOpen` |

**IMPORTANT:** Use word-boundary replacements to avoid partial matches.
Do these in longest-name-first order to avoid partial substitution (e.g. do `QrBtnText` before `QrBtn`).

**Step 2: Remove the old standalone AUX global declarations**

Delete lines ~825–851 (the block of `static lv_obj_t *g_lvglAux*` declarations).
Do NOT remove `g_lvglAuxRoot` — keep it as a standalone global.

**Step 3: Compile to verify no regressions**

Expected: exit 0, same flash/RAM percentages.

**Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "refactor(t2a-2): migrate AUX globals into g_auxDeck struct members"
```

---

## Task 3: Migrate Wiki globals → g_wikiDeck members

**File:** `scrybar.ino`

Same process as Task 2, but for Wiki:

| Replace | With |
|---|---|
| `g_lvglWikiCard` | `g_wikiDeck.card` |
| `g_lvglWikiHeader\b` | `g_wikiDeck.header` |
| `g_lvglWikiHeaderFill` | `g_wikiDeck.headerFill` |
| `g_lvglWikiTitle` | `g_wikiDeck.title` |
| `g_lvglWikiStatus` | `g_wikiDeck.status` |
| `g_lvglWikiQrBtnText` | `g_wikiDeck.qrBtnText` |
| `g_lvglWikiQrBtn\b` | `g_wikiDeck.qrBtn` |
| `g_lvglWikiRefreshBtnText` | `g_wikiDeck.refreshBtnText` |
| `g_lvglWikiRefreshBtn\b` | `g_wikiDeck.refreshBtn` |
| `g_lvglWikiNextFeedBtnText` | `g_wikiDeck.nextFeedBtnText` |
| `g_lvglWikiNextFeedBtn\b` | `g_wikiDeck.nextFeedBtn` |
| `g_lvglWikiSourceBadgeText` | `g_wikiDeck.sourceBadgeText` |
| `g_lvglWikiSourceBadge\b` | `g_wikiDeck.sourceBadge` |
| `g_lvglWikiSourceSite` | `g_wikiDeck.sourceSite` |
| `g_lvglWikiNews` | `g_wikiDeck.news` |
| `g_lvglWikiMeta` | `g_wikiDeck.meta` |
| `g_lvglWikiQrOverlay` | `g_wikiDeck.qrOverlay` |
| `g_lvglWikiQrHint` | `g_wikiDeck.qrHint` |
| `g_lvglWikiQr\b` | `g_wikiDeck.qr` |
| `g_lvglWikiLastItemShown` | `g_wikiDeck.lastItemShown` |
| `g_lvglWikiLastQrPayload` | `g_wikiDeck.lastQrPayload` |
| `g_lvglWikiQrModalOpen` | `g_wikiDeck.qrModalOpen` |

Remove old Wiki global declarations (~lines 853–876). Keep `g_lvglWikiRoot`.

Compile. Expected: exit 0.

```bash
git add scrybar.ino
git commit -m "refactor(t2a-3): migrate Wiki globals into g_wikiDeck struct members"
```

---

## Task 4: Simplify deck-specific and dispatch functions

**File:** `scrybar.ino`

After Tasks 2–3, the 27 functions (9 AUX + 9 Wiki + 9 dispatch) all use struct members. Now collapse them.

**Step 1: Add `activeFeedDeck()` helper**

Insert near the dispatch helpers block (around line 6600):

```cpp
// Returns the active feed deck based on current UI page.
static FeedDeckUi &activeFeedDeck() {
  return (g_uiPageMode == UI_PAGE_WIKI) ? g_wikiDeck : g_auxDeck;
}
```

**Step 2: Replace the 9 AUX + 9 Wiki functions with unified deck-param versions**

Replace the bodies of all 18 AUX/Wiki specific functions. The new unified versions take `FeedDeckUi &d`:

```cpp
static bool lvglDeckQrButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.qrBtn, x, y, 8);
}
static bool lvglDeckRefreshButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.refreshBtn, x, y, 8);
}
static bool lvglDeckNextFeedButtonContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPointExpanded(d.nextFeedBtn, x, y, 8);
}
static bool lvglDeckNewsContainsPoint(FeedDeckUi &d, int16_t x, int16_t y) {
  return lvglContainsPoint(d.news, x, y);
}
static void lvglSetDeckQrButtonPressed(FeedDeckUi &d, bool pressed) {
  const uint32_t bg  = pressed ? activeUiTheme().lvgl.auxBtnPressedBg  : activeUiTheme().lvgl.auxBtnBg;
  const uint32_t txt = pressed ? activeUiTheme().lvgl.auxBtnPressedText : activeUiTheme().lvgl.auxBtnText;
  if (d.qrBtn)     lv_obj_set_style_bg_color(d.qrBtn,     lv_color_hex(bg),  LV_PART_MAIN);
  if (d.qrBtnText) lv_obj_set_style_text_color(d.qrBtnText, lv_color_hex(txt), 0);
}
static void lvglSetDeckRefreshButtonPressed(FeedDeckUi &d, bool pressed) {
  const uint32_t bg  = pressed ? activeUiTheme().lvgl.auxBtnPressedBg  : activeUiTheme().lvgl.auxBtnBg;
  const uint32_t txt = pressed ? activeUiTheme().lvgl.auxBtnPressedText : activeUiTheme().lvgl.auxBtnText;
  if (d.refreshBtn)     lv_obj_set_style_bg_color(d.refreshBtn,     lv_color_hex(bg),  LV_PART_MAIN);
  if (d.refreshBtnText) lv_obj_set_style_text_color(d.refreshBtnText, lv_color_hex(txt), 0);
}
static void lvglSetDeckNextFeedButtonPressed(FeedDeckUi &d, bool pressed) {
  const uint32_t bg  = pressed ? activeUiTheme().lvgl.auxBtnPressedBg  : activeUiTheme().lvgl.auxBtnBg;
  const uint32_t txt = pressed ? activeUiTheme().lvgl.auxBtnPressedText : activeUiTheme().lvgl.auxBtnText;
  if (d.nextFeedBtn)     lv_obj_set_style_bg_color(d.nextFeedBtn,     lv_color_hex(bg),  LV_PART_MAIN);
  if (d.nextFeedBtnText) lv_obj_set_style_text_color(d.nextFeedBtnText, lv_color_hex(txt), 0);
}
static void lvglSetDeckQrModalOpen(FeedDeckUi &d, bool open) {
  d.qrModalOpen = open;
  if (!open) { d.lastItemShown = -1; d.lastQrPayload[0] = '\0'; }
  if (d.qrBtnText) lv_label_set_text(d.qrBtnText, open ? "X" : "QR");
  if (d.qrOverlay) {
    if (open) {
      lv_obj_clear_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(d.qrOverlay);
    } else {
      lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
    }
  }
}
```

**Step 3: Rewrite the 9 dispatch helpers using activeFeedDeck()**

```cpp
static bool lvglFeedQrButtonContainsPoint(int16_t x, int16_t y)      { return lvglDeckQrButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedRefreshButtonContainsPoint(int16_t x, int16_t y)  { return lvglDeckRefreshButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedNextFeedButtonContainsPoint(int16_t x, int16_t y) { return lvglDeckNextFeedButtonContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedNewsContainsPoint(int16_t x, int16_t y)           { return lvglDeckNewsContainsPoint(activeFeedDeck(), x, y); }
static bool lvglFeedQrModalIsOpen()                                    { return activeFeedDeck().qrModalOpen; }
static void lvglSetFeedQrModalOpen(bool open)                          { lvglSetDeckQrModalOpen(activeFeedDeck(), open); }
static void lvglSetFeedQrButtonPressed(bool pressed)                   { lvglSetDeckQrButtonPressed(activeFeedDeck(), pressed); }
static void lvglSetFeedRefreshButtonPressed(bool pressed)              { lvglSetDeckRefreshButtonPressed(activeFeedDeck(), pressed); }
static void lvglSetFeedNextFeedButtonPressed(bool pressed)             { lvglSetDeckNextFeedButtonPressed(activeFeedDeck(), pressed); }
```

**Step 4: Update setUiPage to use lvglSetDeckQrModalOpen directly**

Find the `setUiPage` calls to `lvglSetAuxQrModalOpen(false)` / `lvglSetWikiQrModalOpen(false)` and replace:

```cpp
lvglSetDeckQrModalOpen(g_auxDeck, false);
lvglSetDeckQrModalOpen(g_wikiDeck, false);
```

**Step 5: Remove the 18 now-dead AUX/Wiki specific functions**

Delete all 9 `lvglAux*` and 9 `lvglWiki*` function bodies (they are no longer called anywhere). Also remove their forward declarations.

The `lvglAuxHeroContainsPoint` stub (always returns false) — keep it for now as it has a call site at touch handler.

**Step 6: Update forward declarations**

Add forward decls for all new `lvglDeck*` functions in the `#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI` block.

**Step 7: Compile**

Expected: exit 0.

```bash
git add scrybar.ino
git commit -m "refactor(t2a-4): collapse 27 deck functions into 9 lvglDeck* + activeFeedDeck()"
```

---

## Task 5: Merge lvglUpdateAuxRss + lvglUpdateWikiDeck → lvglUpdateFeedDeck

**File:** `scrybar.ino`

**Step 1: Identify the differences between the two functions**

After Tasks 2–3 the two functions are nearly identical. The only real differences:
- `isWiki=false` → `rssTryShortenUrl`, fallback URL `"https://ansa.it"`, `includeTitleInBody=true`, title `"ScryBar RSS"`
- `isWiki=true` → `wikiTryShortenUrl`, fallback URL `"https://en.wikipedia.org"`, `includeTitleInBody=false`, title `"ScryBar Wiki"`, wiki-specific slot badge/color/sourceLine logic
- `isWiki=false` → RSS source host resolution (`rssResolveSourceHost`, `buildRssSiteShortName`, ANSA check)
- `isWiki=true` → Wiki slot badge (`wikiFeedUiBadge`, `wikiFeedUiColorHex`, `wikiFeedUiName`)

**Step 2: Write merged function**

Replace both functions with a single `lvglUpdateFeedDeck`:

```cpp
static void lvglUpdateFeedDeck(FeedDeckUi &d, RssState &content, bool isWiki, bool force) {
  if (!d.news) return;
#if TEST_WIFI && RSS_ENABLED
  const uint32_t now = millis();
  bool qrOpen = d.qrModalOpen;
  if (g_wifiConnected && content.valid && content.itemCount > 1 && content.lastRotateMs != 0 &&
      !qrOpen && (now - content.lastRotateMs) >= RSS_ROTATE_MS) {
    content.currentIndex = (uint8_t)((content.currentIndex + 1) % content.itemCount);
    content.lastRotateMs = now;
    force = true;
  }
#endif

  char title3[260];
  char whenLine[64];   // AUX only — used in sourceWithWhen; unused for Wiki
  char status[32];
  char meta[96];
  char siteShort[16];
  char siteBadge[4];
  char sourceLine[96];
  char siteHost[96];
  uint32_t siteColorHex = 0x2B468E;
  uint32_t siteTextHex  = 0xFFFFFF;
  bool sourceLineSet    = false;
  title3[0] = whenLine[0] = status[0] = meta[0] = sourceLine[0] = '\0';
  strncpy(siteShort, isWiki ? "WIKI" : "RSS",  sizeof(siteShort) - 1); siteShort[sizeof(siteShort)-1] = '\0';
  strncpy(siteBadge, isWiki ? "W"    : "R",    sizeof(siteBadge) - 1); siteBadge[sizeof(siteBadge)-1] = '\0';
  strncpy(siteHost,  isWiki ? "wiki" : "rss",  sizeof(siteHost)  - 1); siteHost[sizeof(siteHost)-1]   = '\0';
  int16_t showIndex = -1;
  if (d.title) lv_label_set_text(d.title, isWiki ? "ScryBar Wiki" : "ScryBar RSS");

#if TEST_WIFI && RSS_ENABLED
  if (!g_wifiConnected) {
    strncpy(title3, activeUiStrings()->rssOffline, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "OFF");
    snprintf(meta, sizeof(meta), "Ultimo fetch: %s", content.lastFetchMs ? content.fetchedAt : "--/-- --:--");
  } else if (content.valid && content.itemCount > 0) {
    showIndex = (int16_t)(content.currentIndex % content.itemCount);
    buildRssStoryBlock(content.items[showIndex], d.news, title3, sizeof(title3), !isWiki);
    if (!isWiki) buildRssWhenLabel(content.items[showIndex].pubDate, whenLine, sizeof(whenLine));
    snprintf(status, sizeof(status), "%u/%u", (unsigned)(showIndex + 1), (unsigned)content.itemCount);
    uint32_t rotateLeftSec = 0;
    if (content.lastRotateMs != 0) {
      const uint32_t elapsed = now - content.lastRotateMs;
      rotateLeftSec = (elapsed >= RSS_ROTATE_MS) ? 0 : (uint32_t)(((RSS_ROTATE_MS - elapsed) + 999UL) / 1000UL);
    }
    if (isWiki) {
      const uint8_t wikiSlot = content.items[showIndex].feedSlot;
      extractRssHost(content.items[showIndex].link, siteHost, sizeof(siteHost));
      copyStringSafe(siteBadge, sizeof(siteBadge), wikiFeedUiBadge(wikiSlot));
      siteColorHex = wikiFeedUiColorHex(wikiSlot);
      const char *feedName = wikiFeedUiName(wikiSlot);
      snprintf(sourceLine, sizeof(sourceLine), "Wikipedia | %s", feedName);
      if (qrOpen) {
        snprintf(meta, sizeof(meta), "%s | QR", feedName);
      } else {
        char titleHead[72];
        copyStringSafe(titleHead, sizeof(titleHead), content.items[showIndex].title[0] ? content.items[showIndex].title : "-");
        sanitizeAsciiBuffer(titleHead, sizeof(titleHead));
        snprintf(meta, sizeof(meta), "%s | %s", feedName, titleHead);
      }
      sourceLineSet = true;
    } else {
      rssResolveSourceHost(content.items[showIndex], siteHost, sizeof(siteHost));
      buildRssSiteShortName(siteHost, siteShort, sizeof(siteShort));
      buildRssSiteBadge(siteShort, siteBadge, sizeof(siteBadge));
      siteColorHex = rssSiteColorHexFromHost(siteHost);
      if (strcmp(siteShort, "ANSA") == 0) { siteColorHex = 0xFFFFFF; siteTextHex = 0x1B3C86; }
      if (qrOpen) snprintf(meta, sizeof(meta), "Fetch %s | QR", content.fetchedAt);
      else        snprintf(meta, sizeof(meta), "Fetch %s | %lus", content.fetchedAt, (unsigned long)rotateLeftSec);
    }
  } else if (content.lastHttpCode != 0) {
    strncpy(title3, activeUiStrings()->rssFeedError, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "ERR %d", content.lastHttpCode);
    snprintf(meta, sizeof(meta), "Fetch %s", content.lastFetchMs ? content.fetchedAt : "--/-- --:--");
  } else {
    strncpy(title3, activeUiStrings()->rssSyncing, sizeof(title3) - 1); title3[sizeof(title3)-1] = '\0';
    if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
    snprintf(status, sizeof(status), "SYNC");
    snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
  }
#elif TEST_WIFI
  strncpy(title3, activeUiStrings()->rssDisabled, sizeof(title3)-1); title3[sizeof(title3)-1] = '\0';
  if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
  snprintf(status, sizeof(status), g_wifiConnected ? "WiFi" : "OFF");
  snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
#else
  const char *naMsg = isWiki ? "Wiki non disponibile\n(TEST_WIFI=0)." : "RSS non disponibile\n(TEST_WIFI=0).";
  strncpy(title3, naMsg, sizeof(title3)-1); title3[sizeof(title3)-1] = '\0';
  if (!isWiki) { strncpy(whenLine, "--/-- --:--", sizeof(whenLine)-1); whenLine[sizeof(whenLine)-1] = '\0'; }
  snprintf(status, sizeof(status), "N/A");
  snprintf(meta, sizeof(meta), "Fetch --/-- --:--");
#endif

  if (!sourceLineSet) snprintf(sourceLine, sizeof(sourceLine), "%s", siteShort);

  sanitizeAsciiBuffer(title3, sizeof(title3));
  sanitizeAsciiBuffer(meta, sizeof(meta));
  sanitizeAsciiBuffer(sourceLine, sizeof(sourceLine));

  char sourceWithWhen[140];
  copyStringSafe(sourceWithWhen, sizeof(sourceWithWhen), sourceLine);
  if (!isWiki) {
    sanitizeAsciiBuffer(whenLine, sizeof(whenLine));
    if (whenLine[0] && strcmp(whenLine, "--/-- --:--") != 0) {
      strncat(sourceWithWhen, " - ", sizeof(sourceWithWhen) - strlen(sourceWithWhen) - 1);
      strncat(sourceWithWhen, whenLine, sizeof(sourceWithWhen) - strlen(sourceWithWhen) - 1);
    }
  }

  lv_label_set_text(d.news, title3);
  lvglForceLabelVisible(d.news);
  if (d.sourceBadge)     lv_obj_clear_flag(d.sourceBadge, LV_OBJ_FLAG_HIDDEN);
  if (d.sourceSite)    { lv_label_set_text(d.sourceSite, sourceWithWhen); lvglForceLabelVisible(d.sourceSite); }
  if (d.sourceBadgeText) {
    lv_label_set_text(d.sourceBadgeText, siteBadge);
    lv_obj_set_style_text_color(d.sourceBadgeText, lv_color_hex(siteTextHex), 0);
    lvglForceLabelVisible(d.sourceBadgeText);
  }
  if (d.sourceBadge)   lv_obj_set_style_bg_color(d.sourceBadge, lv_color_hex(siteColorHex), LV_PART_MAIN);
  if (d.status)        { lv_label_set_text(d.status, status); lvglForceLabelVisible(d.status); }
  if (d.meta)          { lv_label_set_text(d.meta, meta); lvglForceLabelVisible(d.meta); }

#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
#if TEST_WIFI && RSS_ENABLED
  if (d.qrOverlay) {
    if (qrOpen) {
      lv_obj_clear_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(d.qrOverlay);
      if (d.qrHint) lv_obj_clear_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
      if (d.qr) {
        const char *url = isWiki ? "https://en.wikipedia.org" : "https://ansa.it";
        bool qrReady = true;
        int16_t qrIndex = -1;
        if (showIndex >= 0 && showIndex < (int16_t)content.itemCount) {
          RssItem &item = content.items[showIndex];
          if (item.link[0]) url = item.link;
          if (!item.shortReady) {
            if (isWiki) (void)wikiTryShortenUrl((uint8_t)showIndex);
            else        (void)rssTryShortenUrl((uint8_t)showIndex);
          }
          if (item.shortReady && item.shortLink[0]) url = item.shortLink;
          qrIndex = showIndex;
        }
        if (qrReady) {
          if (force || d.lastItemShown != qrIndex ||
              strncmp(d.lastQrPayload, url, sizeof(d.lastQrPayload) - 1) != 0) {
            lv_qrcode_update(d.qr, url, strlen(url));
            d.lastItemShown = qrIndex;
            strncpy(d.lastQrPayload, url, sizeof(d.lastQrPayload) - 1);
            d.lastQrPayload[sizeof(d.lastQrPayload) - 1] = '\0';
          }
        }
      }
    } else {
      lv_obj_add_flag(d.qrOverlay, LV_OBJ_FLAG_HIDDEN);
      if (d.qrHint) lv_obj_add_flag(d.qrHint, LV_OBJ_FLAG_HIDDEN);
    }
  }
#endif
#endif
}
```

**Step 3: Replace old functions with thin wrappers (temporary, removed in Task 7)**

```cpp
static void lvglUpdateAuxRss(bool force)   { lvglUpdateFeedDeck(g_auxDeck,  g_rss,  false, force); }
static void lvglUpdateWikiDeck(bool force)  { lvglUpdateFeedDeck(g_wikiDeck, g_wiki, true,  force); }
```

These wrappers let us keep call sites in `updateLvglUi` unchanged for now.

**Step 4: Compile**

Expected: exit 0.

```bash
git add scrybar.ino
git commit -m "refactor(t2a-5): merge lvglUpdateAuxRss + lvglUpdateWikiDeck into lvglUpdateFeedDeck"
```

---

## Task 6: Deduplicate lvglApplyThemeStyles and lvglApplyThemeFonts

**File:** `scrybar.ino`

**Step 1: Replace AUX + Wiki theme blocks with a loop**

Find the AUX theming block in `lvglApplyThemeStyles` and the identical Wiki block immediately after. Replace both with:

```cpp
// ── Feed Deck theming (AUX + WIKI) ──────────────────────────────────────────
FeedDeckUi *feedDecks[] = {&g_auxDeck, &g_wikiDeck};
for (FeedDeckUi *d : feedDecks) {
  if (d->card) {
    lv_obj_set_style_bg_color(d->card, lv_color_hex(t.auxCardBg), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(d->card, lv_color_hex(t.auxCardGrad), LV_PART_MAIN);
    lv_obj_set_style_radius(d->card, 0, LV_PART_MAIN);
  }
  if (d->header) {
    lv_obj_set_style_bg_color(d->header, lv_color_hex(t.auxHeaderBg), LV_PART_MAIN);
    // ... all header styling ...
  }
  // ... all other widget styling mirroring current AUX block ...
  // QR recreation (same logic for both)
#if defined(LV_USE_QRCODE) && LV_USE_QRCODE
  if (d->qrOverlay) {
    // recreate QR with updated colors — identical logic
  }
#endif
}
```

**NOTE:** Before writing this, carefully read the existing AUX and Wiki blocks to confirm they are truly identical (same token names). If any token differs between AUX and Wiki, add an inline conditional:
```cpp
const uint32_t headerBg = (d == &g_wikiDeck) ? t.wikiHeaderBg : t.auxHeaderBg;
```
Based on the audit, both use identical `t.auxX` tokens — so no conditionals should be needed.

**Step 2: Replace AUX + Wiki font blocks in lvglApplyThemeFonts with a loop**

```cpp
FeedDeckUi *feedDecks[] = {&g_auxDeck, &g_wikiDeck};
for (FeedDeckUi *d : feedDecks) {
  if (d->nextFeedBtnText) lv_obj_set_style_text_font(d->nextFeedBtnText, lvglFontTiny(), 0);
  if (d->refreshBtnText)  lv_obj_set_style_text_font(d->refreshBtnText,  lvglFontTiny(), 0);
  if (d->qrBtnText)       lv_obj_set_style_text_font(d->qrBtnText,       lvglFontTiny(), 0);
  if (d->title)           lv_obj_set_style_text_font(d->title,           lvglFontSmall(), 0);
  if (d->status)          lv_obj_set_style_text_font(d->status,          lvglFontTiny(), 0);
  if (d->sourceBadgeText) lv_obj_set_style_text_font(d->sourceBadgeText, lvglFontTiny(), 0);
  if (d->sourceSite)      lv_obj_set_style_text_font(d->sourceSite,      lvglFontMeta(), 0);
  if (d->news)            lv_obj_set_style_text_font(d->news,            lvglFontRssNews(), 0);
  if (d->meta)            lv_obj_set_style_text_font(d->meta,            lvglFontSmall(), 0);
  if (d->qrHint)          lv_obj_set_style_text_font(d->qrHint,         lvglFontTiny(), 0);
}
```

**Step 3: Compile**

Expected: exit 0, ~20 fewer lines in each function.

```bash
git add scrybar.ino
git commit -m "refactor(t2a-6): loop over feedDecks in theme/font functions, remove ~120 dup lines"
```

---

## Task 7: Refactor initLvglUi → lvglInitFeedDeck

**File:** `scrybar.ino`

This is the largest task. The AUX init block (~220 lines) and Wiki init block (~200 lines) are nearly identical. Extract to a shared function.

**Step 1: Write lvglInitFeedDeck**

```cpp
// Initialises all LVGL widgets for one feed deck (AUX or WIKI).
// root:   the lv_obj_t page created by the nav system for this deck
// d:      the FeedDeckUi struct to populate
// isWiki: false=AUX/RSS, true=WIKI — controls default text and minor layout tweaks
static void lvglInitFeedDeck(FeedDeckUi &d, lv_obj_t *root, bool isWiki) {
  // ... full ~200-line implementation ...
  // Parameter-driven differences:
  //   title text: isWiki ? "ScryBar Wiki" : "ScryBar RSS"
  //   NXT btn text: "NXT" (same both)
  //   SKIP btn text: "SKIP" (same both)
  //   QR btn text: "QR" (same both)
  //   feedIcon: only created if !isWiki (d.feedIcon stays nullptr for Wiki)
}
```

**Step 2: Replace init blocks in initLvglUi**

Find where `g_lvglAuxRoot = lv_obj_create(scr)` and `g_lvglWikiRoot = lv_obj_create(scr)` are (lines ~11665–11677). After creating each root, replace the full init block with:

```cpp
g_lvglAuxRoot  = lv_obj_create(scr);
// ... nav setup for AuxRoot (scroll, size, etc.) ...
lvglInitFeedDeck(g_auxDeck, g_lvglAuxRoot, false);

g_lvglWikiRoot = lv_obj_create(scr);
// ... nav setup for WikiRoot ...
lvglInitFeedDeck(g_wikiDeck, g_lvglWikiRoot, true);
```

**Step 3: Remove the old thin wrappers lvglUpdateAuxRss / lvglUpdateWikiDeck**

Update their call sites in `updateLvglUi` directly:

```cpp
if (g_uiPageMode == UI_PAGE_AUX)  { lvglUpdateFeedDeck(g_auxDeck,  g_rss,  false, force); return; }
if (g_uiPageMode == UI_PAGE_WIKI) { lvglUpdateFeedDeck(g_wikiDeck, g_wiki, true,  force); return; }
```

**Step 4: Remove the wrapper function bodies**

Delete the now-unused `lvglUpdateAuxRss` and `lvglUpdateWikiDeck` wrappers. Update forward declarations.

**Step 5: Compile**

Expected: exit 0.

```bash
git add scrybar.ino
git commit -m "refactor(t2a-7): extract lvglInitFeedDeck, call twice in initLvglUi, remove update wrappers"
```

---

## Task 8: Final cleanup and bumps

**File:** `scrybar.ino`, `config.h`

**Step 1: Remove dead sourceWhenW / sourceWhenX variables**

The audit found dead code at lines ~12032, ~12035 (remnants of removed When widget):
```cpp
int16_t sourceWhenW = 0;             // delete
const int16_t sourceWhenX = ...;    // delete
```
Verify with grep that these variables have no remaining references after the When widget removal.

**Step 2: Bump build tag in config.h**

```cpp
#define FW_BUILD_TAG    "DB-M0-r161"
#define FW_RELEASE_DATE "2026-03-08"
```

**Step 3: Final compile**

Expected: exit 0, 70% flash, 48% RAM (possibly slightly lower after dead code removal).

**Step 4: Update SNOW.md T2-A status and metrics**

**Step 5: Final commit**

```bash
git add scrybar.ino config.h .codex/SNOW.md
git commit -m "refactor(snow-t2a): FeedDeckUi struct complete — 43 globals → 2 struct instances"
```

---

## Expected final metrics

| Metric | Before T2-A | After T2-A |
|---|---|---|
| AUX+Wiki globals | 43 | 2 structs (23 members each) |
| Deck-specific functions | 27 | ~9 lvglDeck* + activeFeedDeck() |
| lvglApplyThemeStyles AUX+Wiki | ~160 lines | ~80 lines (1 loop) |
| lvglApplyThemeFonts AUX+Wiki | ~20 lines | ~12 lines (1 loop) |
| lvglUpdateAuxRss + lvglUpdateWikiDeck | ~374 lines | ~200 lines (1 merged fn) |
| initLvglUi AUX+Wiki init | ~420 lines | ~220 lines + fn call |
| Net line reduction | — | ~350 lines |

---

## Execution order

Tasks must be executed sequentially — each compiles before the next begins.
Recommended checkpoints for review: after Tasks 2, 4, 5, 7.
