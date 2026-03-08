# Wiki Language + Random Article + Remove Napoletano — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add independent Wikipedia language selector, replace Wikinews with Random Article (with thumbnail), and remove Napoletano language.

**Architecture:** New NVS-backed `g_wikiLang` global (separate from `g_wordClockLang`). Wiki feed URLs parameterized by `g_wikiLang`. Slot 2 replaced with REST API JSON fetch for random articles. JPEG thumbnail decoded in PSRAM via `esp_jpeg_decode`, displayed via `lv_img` widget in feed deck.

**Tech Stack:** ESP32-S3, LVGL 8.x, ArduinoJson, esp_jpeg (ESP-IDF built-in), WiFiClientSecure.

---

### Task 1: Remove Napoletano (nap) — All References

**Files:**
- Modify: `scrybar.ino` (lines referenced below)
- Modify: `src/ui_strings.h` (lines 187-205)

**Step 1: Delete nap function definitions in scrybar.ino**

Remove these functions entirely:
- `wordHourNap()` — line 9161
- `minutePastNap()` — line 9179
- `minuteMancoNap()` — line 9190
- `composeWordClockSentenceNap()` — line 9201
- `weatherCodeShortNap()` — line 4733
- `weatherCodeUiLabelNap()` — line 4744
- `formatDateNap()` — line 10624
- `kSaverQuotesNap[]` — line 7788

**Step 2: Remove nap from all 6 dispatchers**

Delete the `nap` branch in each:
- `weatherCodeUiLabel()` — line 4992
- `weatherCodeShort()` — line 5009
- `getRandomSaverQuotes()` — line 7843
- `composeWordClockSentence()` — line 9497
- `getUiStringsForLang()` — line 9513
- `formatDate()` — line 10695

**Step 3: Remove nap from language arrays**

Remove `"nap"` from:
- `kAllowed[]` — lines 2975, 3858, 12501 (3 instances)
- `kLangsFun[]` — line 3381 (`{"nap", "Napoletano"}`)

**Step 4: Remove kUiLang_nap from src/ui_strings.h**

Delete the `kUiLang_nap` struct instance (lines 187-205).

**Step 5: Compile and verify**

```bash
arduino-cli compile --clean --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi .
```

Expected: compiles OK, flash usage slightly lower.

**Step 6: Flash and verify via serial**

Upload and check `[FW]` boot line. Set lang to `nap` via serial to confirm it's rejected.

---

### Task 2: Add `g_wikiLang` Global + NVS Persistence

**Files:**
- Modify: `scrybar.ino`

**Step 1: Add global variable near `g_wordClockLang` (line ~660)**

```cpp
static char g_wikiLang[8] = "en";
```

**Step 2: Add NVS load in `loadRuntimeNetConfigFromNvs()` (after line ~3012)**

```cpp
{
  char wl[8] = "en";
  prefs.getString("wiki_lang", wl, sizeof(wl));
  const char* kWikiLangs[] = {"en","it","fr","de","es","pt","la","eo",nullptr};
  bool valid = false;
  for (const char **p = kWikiLangs; *p; ++p) { if (strcmp(wl, *p) == 0) { valid = true; break; } }
  if (valid) strncpy(g_wikiLang, wl, sizeof(g_wikiLang) - 1);
}
```

**Step 3: Add NVS save in `saveRuntimeNetConfigToNvs()` (after line 3058)**

```cpp
const size_t nWikiLang = prefs.putString("wiki_lang", g_wikiLang);
```

Update the `Serial.printf` format + args to include `wiki_lang=%u`.

**Step 4: Add to `applyRuntimeConfigFromRequest()` (near line ~3858)**

After `wc_lang` handling, add `wiki_lang` parameter extraction:

```cpp
if (server.hasArg("wiki_lang")) {
  const String wl = server.arg("wiki_lang");
  const char* kWikiLangs[] = {"en","it","fr","de","es","pt","la","eo",nullptr};
  bool valid = false;
  for (const char **p = kWikiLangs; *p; ++p) { if (strcmp(wl.c_str(), *p) == 0) { valid = true; break; } }
  if (valid) {
    strncpy(g_wikiLang, wl.c_str(), sizeof(g_wikiLang) - 1);
    g_wikiLang[sizeof(g_wikiLang) - 1] = '\0';
    Serial.printf("[CFG] wiki_lang='%s'\n", g_wikiLang);
  }
}
```

**Step 5: Add `WIKILANG` serial command (near other serial commands ~line 12501)**

```cpp
if (line == "WIKILANG") {
  Serial.printf("[WIKI] lang='%s'\n", g_wikiLang);
}
```

**Step 6: Add to boot log in `[CFG][NVS] loaded` printf**

Add `wiki_lang='%s'` and `g_wikiLang` to the existing log line.

**Step 7: Compile and verify**

---

### Task 3: Web UI — Wikipedia Language Dropdown

**Files:**
- Modify: `scrybar.ino` — `buildWebConfigPage()` (after System Language section, ~line 3416)

**Step 1: Add Wikipedia Language dropdown section**

Insert after the System Language `</div>` (line 3415):

```cpp
{
  struct { const char *code; const char *label; } kWikiLangs[] = {
    {"en", "English"},
    {"it", "Italiano"},
    {"fr", "Fran\xC3\xA7" "ais"},
    {"de", "Deutsch"},
    {"es", "Espa\xC3\xB1" "ol"},
    {"pt", "Portugu\xC3\xAA" "s"},
    {"la", "Latina"},
    {"eo", "Esperanto"},
  };
  html += F("<div class='sec'><h2><i class='fa-solid fa-book-open'></i>Wikipedia Language</h2>"
            "<div class='key'>WIKI LANGUAGE</div><select name='wiki_lang'>");
  for (unsigned i = 0; i < sizeof(kWikiLangs)/sizeof(kWikiLangs[0]); ++i) {
    html += F("<option value='");
    html += kWikiLangs[i].code;
    html += '\'';
    if (strcmp(g_wikiLang, kWikiLangs[i].code) == 0) html += F(" selected");
    html += '>';
    html += kWikiLangs[i].label;
    html += F("</option>");
  }
  html += F("</select><p class='hint'><i class='fa-solid fa-circle-info'></i> "
            "Language for Wikipedia feeds (Featured, On This Day, Random). Independent from the system language.</p></div>");
}
```

**Step 2: Compile, flash, verify dropdown appears in web UI at http://<IP>:8080**

---

### Task 4: Parameterize Wiki Feed URLs by `g_wikiLang`

**Files:**
- Modify: `scrybar.ino` — `updateWikiFromFeed()` (lines 6159-6295)
- Modify: `kWikiFeedUrl[]` and `kWikiFeedName[]` (lines 631-640)

**Step 1: Remove static `kWikiFeedUrl[]` array (lines 637-641)**

The URLs are now dynamic. Keep `kWikiFeedName[]` but update slot 2 name.

```cpp
static const char *kWikiFeedName[WIKI_FEED_SLOT_COUNT] = {
    "Wiki Featured",
    "Wiki OnThisDay",
    "Wiki Random",
};
```

**Step 2: Build URLs dynamically in `updateWikiFromFeed()`**

At the top of the function (after lastAttemptMs check), build all 3 URLs:

```cpp
// Slot 0: Featured
char wikiFeaturedUrl[160];
snprintf(wikiFeaturedUrl, sizeof(wikiFeaturedUrl),
         "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=featured&feedformat=rss",
         g_wikiLang);

// Slot 1: On This Day (date-explicit, existing logic)
char wikiOnThisDayUrl[160] = {0};
{
  struct tm tNow;
  if (getLocalTime(&tNow, 50)) {
    snprintf(wikiOnThisDayUrl, sizeof(wikiOnThisDayUrl),
             "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=onthisday"
             "&feedformat=rss&month=%02d&day=%02d",
             g_wikiLang, tNow.tm_mon + 1, tNow.tm_mday);
  } else {
    snprintf(wikiOnThisDayUrl, sizeof(wikiOnThisDayUrl),
             "https://%s.wikipedia.org/w/api.php?action=featuredfeed&feed=onthisday&feedformat=rss",
             g_wikiLang);
  }
}

// Slot 2: Random — handled separately (Task 5)
```

Update the loop to use `wikiFeaturedUrl` for slot 0, `wikiOnThisDayUrl` for slot 1, and skip slot 2 in the RSS loop (handled by new random fetch).

**Step 3: Update any remaining hardcoded `en.wikipedia.org` references**

Check `resolveWikipediaArticleUrlFromDescription()`, `rssFetchWikipediaSummaryMeta()`, `rssExtractWikipediaArticleRef()` — these may need `g_wikiLang` substitution too.

**Step 4: Compile, flash, verify**

Set `wiki_lang=fr` via web UI, check serial log shows French Wikipedia URLs.

---

### Task 5: Replace Wikinews (Slot 2) with Wikipedia Random Article

**Files:**
- Modify: `scrybar.ino` — `updateWikiFromFeed()`, new helper function

**Step 1: Add `fetchWikiRandomArticle()` helper**

```cpp
// Fetches a random Wikipedia article via REST API.
// Returns true if item was populated.
static bool fetchWikiRandomArticle(RssItem &item) {
  char url[128];
  snprintf(url, sizeof(url),
           "https://%s.wikipedia.org/api/rest_v1/page/random/summary",
           g_wikiLang);

  HTTPClient http;
  http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
  http.setTimeout(RSS_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure tls;
  tls.setInsecure();
  if (!http.begin(tls, url)) return false;
  http.addHeader("Accept", "application/json");

  pumpWebUiDuringIo();
  const int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }

  pumpWebUiDuringIo();
  String payload = http.getString();
  http.end();
  if (payload.length() == 0) return false;

  // Parse JSON
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, payload)) return false;

  const char *title   = doc["title"];
  const char *extract = doc["extract"];
  const char *pageUrl = doc["content_urls"]["desktop"]["page"];

  if (!title || !extract || !pageUrl) return false;

  copyStringSafe(item.title, sizeof(item.title), title);
  copyStringSafe(item.summary, sizeof(item.summary), extract);
  copyStringSafe(item.link, sizeof(item.link), pageUrl);

  // Store thumbnail URL in pubDate field (repurposed, 32 chars is tight)
  // Better: store in shortLink field (96 chars) with a prefix
  const char *thumbUrl = doc["thumbnail"]["source"];
  if (thumbUrl) {
    // Store for later thumbnail fetch
    copyStringSafe(item.shortLink, sizeof(item.shortLink), thumbUrl);
    item.shortReady = false;  // signals "has thumb URL, not yet fetched"
    item.shortTried = false;
  }

  item.feedSlot = 2;
  item.pubDate[0] = '\0';
  normalizeRssText(item.title);
  normalizeRssText(item.summary);

  return true;
}
```

**Step 2: Integrate into `updateWikiFromFeed()`**

Change the loop to only iterate slots 0-1 for RSS, then handle slot 2 separately:

```cpp
// After the RSS loop for slots 0-1:
// Slot 2: Random article via REST API
if (count < RSS_MAX_ITEMS) {
  RssItem randomItem;
  memset(&randomItem, 0, sizeof(randomItem));
  if (fetchWikiRandomArticle(randomItem)) {
    g_rssParseBuf[count] = randomItem;
    ++count;
    ++feedsWithItems;
  }
  ++feedsTried;
}
```

**Step 3: Update `wikiFeedUiName()`, `wikiFeedUiBadge()`, `wikiFeedUiColorHex()`**

```cpp
case 2: return "Random";     // was "Wikinews"
case 2: return "WR";         // was "WN"
case 2: return 0xD4A017;     // gold, was purple
```

**Step 4: Compile, flash, verify**

Check serial: `[WIKI] feeds=3/3 items=N` with a random article title visible. Swipe to WIKI page, verify Random articles appear with "WR" badge.

---

### Task 6: Thumbnail Display — JPEG Decode + LVGL Widget

**Files:**
- Modify: `scrybar.ino` — `FeedDeckUi`, `lvglInitFeedDeck()`, `lvglUpdateFeedDeck()`

**Step 1: Add thumbnail state to FeedDeckUi struct (line ~829)**

```cpp
lv_obj_t *thumbImg     = nullptr;  // Wiki thumbnail (lv_img)
uint16_t *thumbBuf     = nullptr;  // PSRAM: decoded RGB565
int       thumbBufW    = 0;
int       thumbBufH    = 0;
lv_img_dsc_t thumbDsc  = {};       // LVGL image descriptor
```

**Step 2: Add `lv_img` widget in `lvglInitFeedDeck()` for wiki decks**

After the news label setup (~line 11164), add:

```cpp
if (isWiki) {
  d.thumbImg = lv_img_create(d.card);
  lv_obj_add_flag(d.thumbImg, LV_OBJ_FLAG_HIDDEN);
  // Position: right side, below source badge, above buttons
  lv_obj_set_pos(d.thumbImg, cardW - 90, sourceRowY + sourceBadgeSize + 4);
}
```

Also narrow `d.news` width when thumbnail is visible (done in update step).

**Step 3: Add `wikiTryFetchThumb()` helper**

Fetch JPEG from URL stored in `shortLink`, decode to RGB565 in PSRAM:

```cpp
#include "esp_jpeg_dec.h"  // ESP-IDF JPEG decoder

static bool wikiTryFetchThumb(FeedDeckUi &d, const char *thumbUrl) {
  if (!thumbUrl || !thumbUrl[0]) return false;

  // Fetch JPEG
  HTTPClient http;
  http.setConnectTimeout(RSS_HTTP_TIMEOUT_MS);
  http.setTimeout(RSS_HTTP_TIMEOUT_MS);
  WiFiClientSecure tls;
  tls.setInsecure();
  if (!http.begin(tls, thumbUrl)) return false;
  const int code = http.GET();
  if (code != HTTP_CODE_OK) { http.end(); return false; }
  int jpegLen = http.getSize();
  if (jpegLen <= 0 || jpegLen > RSS_THUMB_MAX_BYTES) { http.end(); return false; }
  uint8_t *jpegBuf = (uint8_t *)ps_malloc(jpegLen);
  if (!jpegBuf) { http.end(); return false; }
  WiFiClient *stream = http.getStreamPtr();
  int read = stream->readBytes(jpegBuf, jpegLen);
  http.end();
  if (read != jpegLen) { free(jpegBuf); return false; }

  // Decode JPEG header to get dimensions
  // Use ESP-IDF jpeg_dec API
  jpeg_dec_config_t config = { .output_type = JPEG_RAW_TYPE_RGB565_BE };
  jpeg_dec_handle_t handle = NULL;
  if (jpeg_dec_open(&config, &handle) != ESP_OK) { free(jpegBuf); return false; }

  jpeg_dec_header_info_t hdr = {};
  if (jpeg_dec_parse_header(handle, jpegBuf, read, &hdr) != ESP_OK) {
    jpeg_dec_close(handle); free(jpegBuf); return false;
  }

  // Scale down to fit ~80x80 max
  const int maxDim = 80;
  int outW = hdr.width, outH = hdr.height;
  // Use JPEG decoder's built-in downscaling (1/2, 1/4, 1/8)
  // Pick smallest scale that keeps both dims >= maxDim
  // (detailed scaling logic here)

  // Allocate output buffer
  if (d.thumbBuf) { free(d.thumbBuf); d.thumbBuf = nullptr; }
  d.thumbBuf = (uint16_t *)ps_malloc(outW * outH * 2);
  if (!d.thumbBuf) { jpeg_dec_close(handle); free(jpegBuf); return false; }

  jpeg_dec_io_t io = {};
  io.inbuf = jpegBuf;
  io.inbuf_len = read;
  io.outbuf = (uint8_t *)d.thumbBuf;

  esp_err_t err = jpeg_dec_process(handle, &io);
  jpeg_dec_close(handle);
  free(jpegBuf);
  if (err != ESP_OK) { free(d.thumbBuf); d.thumbBuf = nullptr; return false; }

  d.thumbBufW = outW;
  d.thumbBufH = outH;

  // Setup LVGL image descriptor
  d.thumbDsc.header.always_zero = 0;
  d.thumbDsc.header.w = outW;
  d.thumbDsc.header.h = outH;
  d.thumbDsc.header.cf = LV_IMG_CF_TRUE_COLOR;
  d.thumbDsc.data_size = outW * outH * 2;
  d.thumbDsc.data = (const uint8_t *)d.thumbBuf;

  return true;
}
```

**Step 4: Show thumbnail in `lvglUpdateFeedDeck()` (~line 10257)**

When displaying a wiki item with feedSlot==2 and thumbUrl available:

```cpp
// In the wiki branch of lvglUpdateFeedDeck, after setting news text:
if (isWiki && d.thumbImg) {
  const RssItem &item = content.items[showIndex];
  if (item.feedSlot == 2 && item.shortLink[0] && !item.shortTried) {
    // Mark tried to avoid re-fetching
    content.items[showIndex].shortTried = true;
    if (wikiTryFetchThumb(d, item.shortLink)) {
      lv_img_set_src(d.thumbImg, &d.thumbDsc);
      lv_obj_clear_flag(d.thumbImg, LV_OBJ_FLAG_HIDDEN);
      // Narrow news label to leave room for thumbnail
      lv_obj_set_width(d.news, leftPaneW - d.thumbBufW - 12);
    } else {
      lv_obj_add_flag(d.thumbImg, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_width(d.news, leftPaneW);
    }
  } else if (item.feedSlot != 2) {
    lv_obj_add_flag(d.thumbImg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(d.news, leftPaneW);
  }
}
```

**Step 5: Compile, flash, verify**

Verify on device: swipe to WIKI, navigate to Random article, thumbnail should appear on the right. Check PSRAM usage via serial heartbeat.

---

### Task 7: Integration + Cleanup + Bump Build Tag

**Step 1: Increment `FW_BUILD_TAG` to `r178` in `config.h`**

**Step 2: Full compile + flash**

**Step 3: Verify all features via serial + device:**
- System Language dropdown: nap absent
- Wikipedia Language dropdown: present, changes wiki feeds
- Featured/On This Day: in selected wiki language
- Random article: appears with "WR" badge, correct language
- Thumbnail: displayed for random articles
- Serial: `[WIKI] feeds=3/3 items=N`

**Step 4: Update memory files**
- `memory/MEMORY.md`: update language count 14→13, note wiki_lang feature
- `knowledge/project_knowledge.md`: add wiki_lang NVS key, update feed slot table

---

## Task Dependencies

```
Task 1 (remove nap) ──────────────┐
Task 2 (g_wikiLang + NVS) ────────┼──> Task 4 (parameterize URLs)
Task 3 (web UI dropdown) ─────────┘         │
                                            ├──> Task 5 (random article)
                                            │         │
                                            │         └──> Task 6 (thumbnail)
                                            │                      │
                                            └──────────────────────┴──> Task 7 (integration)
```

Tasks 1, 2, 3 are independent and can run in parallel.
Tasks 4-6 are sequential.
Task 7 is the final integration pass.
