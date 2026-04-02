# Screensaver "Pasture Simulator" Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Evolve the ASCII cow screensaver from a flat animation into a living micro-world with day/night cycle, random events, cow state machine, and hacker-flavored thoughts.

**Architecture:** All changes are in `scrybar.ino`. The `ScreensaverState` struct gets ~36 new bytes of state. No new LVGL objects — events borrow existing `starObj` labels via a bitmask. Cow behavior is a 5-state machine with time-of-day-weighted transitions. Thoughts are categorized (philosophy/hacker/meta/weather/easter_egg) with per-language arrays.

**Tech Stack:** C++ (Arduino/ESP32), LVGL 8.4, FreeRTOS

**Spec:** `docs/superpowers/specs/2026-04-02-screensaver-pasture-simulator-design.md`

---

## File Map

| File | Responsibility |
|------|---------------|
| `scrybar.ino:1276-1316` | `ScreensaverState` struct — add new fields |
| `scrybar.ino:9906-9928` | RNG + cow art sprites — add chew/sleep variants |
| `scrybar.ino:10029-10055` | Field line builder — unchanged |
| `scrybar.ino:10057-10087` | Footer update — unchanged |
| `scrybar.ino:10089-10135` | Idle target + star init — add borrow mask check |
| `scrybar.ino:10137-10265` | Quote arrays — add hacker/meta/weather/easter arrays |
| `scrybar.ino:10275-10316` | Balloon text setter — add category selection |
| `scrybar.ino:10317-10332` | Balloon update — unchanged |
| `scrybar.ino:10334-10374` | Star update — add borrow mask skip |
| `scrybar.ino:10376-10396` | Cow respawn — integrate state machine |
| `scrybar.ino:10398-10434` | Activate/deactivate — add new state resets |
| `scrybar.ino:10436-10510` | Main loop — add sky phase, events, cow state machine |
| `config.h:21-24` | Screensaver constants — no changes |

All tasks modify only `scrybar.ino`. No new files.

---

### Task 1: Extend ScreensaverState struct + enums

**Files:**
- Modify: `scrybar.ino:1276-1316`

- [ ] **Step 1: Add enums before the struct**

Insert after line 1279 (`static constexpr uint8_t kSaverStarsPerRow = 2;`):

```cpp
enum CowState : uint8_t {
  COW_GRAZE    = 0,
  COW_IDLE     = 1,
  COW_SLEEP    = 2,
  COW_RUN      = 3,
  COW_STARE_UP = 4,
};

enum SaverEvent : uint8_t {
  SAVER_EVENT_NONE          = 0,
  SAVER_EVENT_SHOOTING_STAR = 1,
  SAVER_EVENT_UFO           = 2,
  SAVER_EVENT_SATELLITE     = 3,
  SAVER_EVENT_RAIN          = 4,
  SAVER_EVENT_MATRIX_GLITCH = 5,
};

enum SkyPhase : uint8_t {
  SKY_NIGHT = 0,
  SKY_DAWN  = 1,
  SKY_DAY   = 2,
  SKY_DUSK  = 3,
};

enum ThoughtCategory : uint8_t {
  THOUGHT_PHILOSOPHY = 0,
  THOUGHT_HACKER     = 1,
  THOUGHT_META       = 2,
  THOUGHT_WEATHER    = 3,
  THOUGHT_EASTER_EGG = 4,
};
```

- [ ] **Step 2: Add new fields to ScreensaverState**

Add after `char fieldBuf[256] = {0};` (line 1314), before the closing `};`:

```cpp
  // --- Pasture Simulator additions ---
  // Group uint32_t together for alignment
  uint32_t skyNextMs = 0;
  uint32_t cowChewNextMs = 0;
  uint32_t cowStateNextMs = 0;
  uint32_t eventEndMs = 0;
  uint32_t eventCooldownMs = 0;
  uint32_t starBorrowedMask = 0;
  // Group smaller types
  int16_t  eventX = 0;
  uint8_t  skyPhase = 0;       // SkyPhase enum
  uint8_t  cowState = 0;       // CowState enum
  uint8_t  cowChewFrame = 0;
  uint8_t  eventActive = 0;    // SaverEvent enum
  int8_t   eventDir = 1;
  uint8_t  thoughtCategory = 0; // ThoughtCategory enum
  uint8_t  cloudOffset = 0;    // cloud drift position (day only)
  uint8_t  cowPrevState = 0;   // state before STARE_UP, for restoration
  uint32_t cloudNextMs = 0;    // next cloud update time
```

- [ ] **Step 3: Compile to verify struct is valid**

Run:
```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  /Users/enuzzo/Library/CloudStorage/Dropbox/Mitnick/scrybar
```
Expected: compiles successfully.

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add enums and new state fields for Pasture Simulator"
```

---

### Task 2: Cow art variants (chew + sleep)

**Files:**
- Modify: `scrybar.ino:9911-9928` (lvglScreenSaverSetCowArt)

- [ ] **Step 1: Add chew and sleep sprite variants**

Replace the entire `lvglScreenSaverSetCowArt` function with:

```cpp
static void lvglScreenSaverSetCowArt(int8_t dir) {
  if (!g_saver.cow) return;
  // Sleep sprite — lying down, 4 lines
  if (g_saver.cowState == COW_SLEEP) {
    static const char *kCowSleep =
        "      Z z z\n"
        "   __(o_o)__\n"
        "  /__|__|__\\~\n"
        "  ~~~~~~~~~~";
    lv_label_set_text(g_saver.cow, kCowSleep);
    return;
  }
  static const char *kCowRight =
      " _(__)_        V\n"
      "'-e e -'__,--.__)\n"
      "(o_o)        )\n"
      "   \\. /___.  |\n"
      "   ||| _)/_)/\n"
      "  //_(/_(/_(";
  static const char *kCowRightChew =
      " _(__)_        V\n"
      "'-e e -'__,--.__)\n"
      "(o-o)        )\n"
      "   \\. /___.  |\n"
      "   ||| _)/_)/\n"
      "  //_(/_(/_(";
  static const char *kCowLeft =
      "V        _(__)_ \n"
      "(__.--,__'-e e -'\n"
      "  (        (o_o) \n"
      "  |  .___\\ ./    \n"
      "   \\(_\\(_ |||    \n"
      "    )_\\)_\\)_\\\\";
  static const char *kCowLeftChew =
      "V        _(__)_ \n"
      "(__.--,__'-e e -'\n"
      "  (        (o-o) \n"
      "  |  .___\\ ./    \n"
      "   \\(_\\(_ |||    \n"
      "    )_\\)_\\)_\\\\";
  const bool chew = (g_saver.cowChewFrame == 1 && g_saver.cowState == COW_GRAZE);
  if (dir >= 0) {
    lv_label_set_text(g_saver.cow, chew ? kCowLeftChew : kCowLeft);
  } else {
    lv_label_set_text(g_saver.cow, chew ? kCowRightChew : kCowRight);
  }
}
```

- [ ] **Step 2: Compile**

Same compile command as Task 1 Step 3. Expected: compiles successfully.

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add chew and sleep cow art variants"
```

---

### Task 3: Day/night cycle (sky phase)

**Files:**
- Modify: `scrybar.ino` — add new function `lvglScreenSaverUpdateSkyPhase` near line ~10090

- [ ] **Step 1: Add helper to force-end events incompatible with current phase**

Insert after `lvglScreenSaverInitStars()` (around line 10135), before the quote arrays:

```cpp
static void lvglScreenSaverEndEvent() {
  if (g_saver.eventActive == SAVER_EVENT_NONE) return;
  // Release borrowed star labels
  for (uint8_t r = 0; r < kSaverSkyRowsMax; ++r) {
    for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
      const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & bit) {
        if (g_saver.starObj[r][s]) {
          lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
          lv_label_set_text(g_saver.starObj[r][s], ".");
        }
      }
    }
  }
  g_saver.starBorrowedMask = 0;
  g_saver.eventActive = SAVER_EVENT_NONE;
  g_saver.eventEndMs = 0;
  g_saver.eventCooldownMs = millis() + 180000UL; // 3min cooldown
  Serial.println("[SCRNSVR] event ended (forced)");
}
```

- [ ] **Step 2: Add sky phase update function**

Insert immediately after the function above:

```cpp
static void lvglScreenSaverUpdateSkyPhase(uint32_t nowMs) {
  if (nowMs < g_saver.skyNextMs) return;
  g_saver.skyNextMs = nowMs + 60000UL; // check every 60s

  uint8_t newPhase = SKY_NIGHT;
  if (g_clock.ntpSynced) {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 20)) {
      const uint8_t h = (uint8_t)tmNow.tm_hour;
      if (h >= 5 && h < 7)       newPhase = SKY_DAWN;
      else if (h >= 7 && h < 19) newPhase = SKY_DAY;
      else if (h >= 19 && h < 21) newPhase = SKY_DUSK;
      else                        newPhase = SKY_NIGHT;
    }
  }

  if (newPhase == g_saver.skyPhase) return;
  const uint8_t oldPhase = g_saver.skyPhase;
  g_saver.skyPhase = newPhase;
  Serial.printf("[SCRNSVR] skyPhase=%u\n", newPhase);

  // Phase transition safety: force-end incompatible events
  if (g_saver.eventActive != SAVER_EVENT_NONE) {
    const uint8_t ev = g_saver.eventActive;
    // Rain is day-only
    if (ev == SAVER_EVENT_RAIN && newPhase != SKY_DAY) lvglScreenSaverEndEvent();
    // Shooting star & satellite are night/dusk only
    if ((ev == SAVER_EVENT_SHOOTING_STAR || ev == SAVER_EVENT_SATELLITE) &&
        (newPhase == SKY_DAY || newPhase == SKY_DAWN)) lvglScreenSaverEndEvent();
  }

  // Update sky label for dawn/dusk gradient or clear for night
  if (g_saver.sky) {
    if (newPhase == SKY_NIGHT) {
      lv_label_set_text(g_saver.sky, "");
      lv_obj_add_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
    } else if (newPhase == SKY_DAWN) {
      lv_obj_clear_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
      // gradient text set by cloud update function
    } else if (newPhase == SKY_DAY) {
      lv_obj_clear_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
      g_saver.cloudOffset = 0;
    } else { // SKY_DUSK
      lv_obj_clear_flag(g_saver.sky, LV_OBJ_FLAG_HIDDEN);
    }
  }
}
```

- [ ] **Step 3: Add cloud/gradient update for the sky label**

Insert after the sky phase function:

```cpp
static void lvglScreenSaverUpdateClouds(uint32_t nowMs) {
  if (!g_saver.sky) return;
  // Only update every 2 seconds
  if (nowMs < g_saver.cloudNextMs) return;
  g_saver.cloudNextMs = nowMs + 2000UL;

  char buf[128];
  memset(buf, 0, sizeof(buf));

  if (g_saver.skyPhase == SKY_DAY) {
    // Drifting cloud
    const char *cloud = "_.--\"\"--._";
    const uint8_t cloudLen = 10;
    uint8_t spaces = g_saver.cloudOffset;
    uint8_t i = 0;
    // Leading newlines to position cloud vertically
    buf[i++] = '\n';
    for (uint8_t s = 0; s < spaces && i < 100; ++s) buf[i++] = ' ';
    for (uint8_t c = 0; c < cloudLen && i < 115; ++c) buf[i++] = cloud[c];
    buf[i] = '\0';
    g_saver.cloudOffset = (uint8_t)((g_saver.cloudOffset + 1) % 60);
    lv_label_set_text(g_saver.sky, buf);
  } else if (g_saver.skyPhase == SKY_DAWN) {
    // Rising gradient at bottom of sky area
    snprintf(buf, sizeof(buf), "\n\n\n\n\n          ░░▒▒▓▓▓▓▒▒░░");
    lv_label_set_text(g_saver.sky, buf);
  } else if (g_saver.skyPhase == SKY_DUSK) {
    snprintf(buf, sizeof(buf), "\n\n\n\n\n          ▓▓▒▒░░░░▒▒▓▓");
    lv_label_set_text(g_saver.sky, buf);
  }
  // SKY_NIGHT: sky label hidden, no update needed
}
```

- [ ] **Step 4: Compile**

Same compile command. Expected: compiles successfully.

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add day/night cycle with sky phases and clouds"
```

---

### Task 4: Star update — borrow mask skip

**Files:**
- Modify: `scrybar.ino:10334-10374` (lvglScreenSaverUpdateStars)

- [ ] **Step 1: Add borrow mask check to star update loop**

In `lvglScreenSaverUpdateStars()`, add a skip check at the start of the inner loop body. Find the line:

```cpp
      if (nowMs < g_saver.starNextMs[r][s]) continue;
```

Insert **before** it (inside the inner for loop, as the first statement):

```cpp
      // Skip slots borrowed by active events
      const uint32_t starBit = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & starBit) continue;
```

Also in the second loop (the rendering loop at ~line 10357), add the same check before `lv_obj_t *star = g_saver.starObj[r][s];`:

```cpp
      const uint32_t starBit2 = 1UL << (r * kSaverStarsPerRow + s);
      if (g_saver.starBorrowedMask & starBit2) continue;
```

Additionally, add sky-phase-aware visibility: during `SKY_DAY`, hide all non-borrowed stars. After the rendering loop, add:

```cpp
  // During day, hide all stars (not borrowed by events)
  if (g_saver.skyPhase == SKY_DAY) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starObj[r][s]) lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
  // During dawn, cap max star brightness (stars fading out)
  if (g_saver.skyPhase == SKY_DAWN) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starLevel[r][s] > 2) g_saver.starLevel[r][s] = 2;
      }
    }
  }
  // During dusk, cap at brightness 2 (stars just appearing)
  if (g_saver.skyPhase == SKY_DUSK) {
    for (uint8_t r = 0; r < g_saver.rows; ++r) {
      for (uint8_t s = 0; s < kSaverStarsPerRow; ++s) {
        const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
        if (g_saver.starBorrowedMask & bit) continue;
        if (g_saver.starLevel[r][s] > 2) g_saver.starLevel[r][s] = 2;
        // Only show ~40% of stars during dusk (hide rest)
        if (s == 1 && (r % 3) != 0 && g_saver.starObj[r][s]) {
          lv_obj_add_flag(g_saver.starObj[r][s], LV_OBJ_FLAG_HIDDEN);
        }
      }
    }
  }
```

- [ ] **Step 2: Compile**

Same compile command. Expected: compiles successfully.

- [ ] **Step 3: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add star borrow mask + day phase star hiding"
```

---

### Task 5: Random event system

**Files:**
- Modify: `scrybar.ino` — add new function `lvglScreenSaverUpdateEvent` near the sky phase code

- [ ] **Step 1: Add the event borrowing helpers**

Insert after `lvglScreenSaverUpdateClouds`:

```cpp
static void lvglScreenSaverBorrowStar(uint8_t r, uint8_t s, const char *text,
                                       int16_t x, int16_t y, uint32_t color) {
  if (r >= kSaverSkyRowsMax || s >= kSaverStarsPerRow) return;
  const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
  g_saver.starBorrowedMask |= bit;
  lv_obj_t *obj = g_saver.starObj[r][s];
  if (!obj) return;
  lv_label_set_text(obj, text);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void lvglScreenSaverReleaseStar(uint8_t r, uint8_t s) {
  if (r >= kSaverSkyRowsMax || s >= kSaverStarsPerRow) return;
  const uint32_t bit = 1UL << (r * kSaverStarsPerRow + s);
  g_saver.starBorrowedMask &= ~bit;
  lv_obj_t *obj = g_saver.starObj[r][s];
  if (!obj) return;
  lv_label_set_text(obj, ".");
  lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  // Star animation will naturally re-acquire this slot
}
```

- [ ] **Step 2: Add the main event update function**

```cpp
static void lvglScreenSaverUpdateEvent(uint32_t nowMs) {
  // If event active, update it
  if (g_saver.eventActive != SAVER_EVENT_NONE) {
    if (nowMs >= g_saver.eventEndMs) {
      lvglScreenSaverEndEvent();
      return;
    }
    const int16_t cW = canvasWidth();
    const int16_t cH = canvasHeight();
    switch (g_saver.eventActive) {
      case SAVER_EVENT_SHOOTING_STAR: {
        g_saver.eventX += 40 * g_saver.eventDir;
        const uint32_t elapsed = nowMs - (g_saver.eventEndMs - 1500UL);
        int16_t y = (int16_t)(8 + (elapsed / 55) * 4);
        if (y > cH - 40) y = (int16_t)(cH - 40);
        if (g_saver.eventX > cW || g_saver.eventX < -40) {
          lvglScreenSaverEndEvent();
          return;
        }
        // Use last row, slot 0
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, "--*",
            g_saver.eventX, (y < cH - 40) ? y : cH - 40,
            activeUiTheme().lvgl.saverStarHigh);
        break;
      }
      case SAVER_EVENT_UFO: {
        g_saver.eventX += 3 * g_saver.eventDir;
        if (g_saver.eventX > cW) g_saver.eventX = -30;
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, "<==>",
            g_saver.eventX, 6, activeUiTheme().lvgl.saverStarHigh);
        break;
      }
      case SAVER_EVENT_SATELLITE: {
        g_saver.eventX += 2 * g_saver.eventDir;
        if (g_saver.eventX > cW || g_saver.eventX < -10) {
          lvglScreenSaverEndEvent();
          return;
        }
        lvglScreenSaverBorrowStar(g_saver.rows - 1, 0, ".",
            g_saver.eventX, 10, activeUiTheme().lvgl.saverStarMid);
        break;
      }
      case SAVER_EVENT_RAIN: {
        // Animate 4 rain drops using last 2 rows (4 slots)
        for (uint8_t i = 0; i < 4; ++i) {
          uint8_t rr = (uint8_t)(g_saver.rows - 2 + (i / 2));
          uint8_t ss = (uint8_t)(i % 2);
          if (rr >= kSaverSkyRowsMax) rr = (uint8_t)(kSaverSkyRowsMax - 1);
          lv_obj_t *obj = g_saver.starObj[rr][ss];
          if (!obj) continue;
          lv_coord_t cy = lv_obj_get_y(obj);
          cy += 6;
          if (cy > cH - 30) {
            cy = 4;
            lv_obj_set_x(obj, (lv_coord_t)(8 + (lvglScreenSaverRandNext() % (uint32_t)(cW - 20))));
          }
          const char *drop = ((lvglScreenSaverRandNext() & 1) == 0) ? "|" : "'";
          lvglScreenSaverBorrowStar(rr, ss, drop,
              lv_obj_get_x(obj), cy, activeUiTheme().lvgl.saverStarMid);
        }
        break;
      }
      case SAVER_EVENT_MATRIX_GLITCH: {
        // 2 labels stacked, cycling hex chars
        for (uint8_t i = 0; i < 2; ++i) {
          uint8_t rr = (uint8_t)(g_saver.rows - 1);
          uint8_t ss = (uint8_t)(i % kSaverStarsPerRow);
          char ch[2] = { (char)('0' + (lvglScreenSaverRandNext() % 16)), '\0' };
          if (ch[0] > '9') ch[0] = (char)('A' + (ch[0] - '9' - 1));
          uint32_t color = lvglThemeIsCathodeRay() ? 0xFFAA00u : 0x00FF00u;
          int16_t y = (int16_t)(20 + i * 14);
          lvglScreenSaverBorrowStar(rr, ss, ch,
              g_saver.eventX, y, color);
        }
        break;
      }
      default: break;
    }
    return;
  }

  // No event active — check cooldown and maybe start one
  if (nowMs < g_saver.eventCooldownMs) return;

  // Roll for new event (weighted by sky phase)
  const uint32_t roll = lvglScreenSaverRandNext() % 1000;

  // Base probabilities per step (55ms). Tuned so events feel 5-60 min apart.
  // At 55ms/step, ~1090 steps/minute. P=1/1000 per step ≈ ~1 event/min.
  // We want much rarer, so use higher denominators.
  bool canNight = (g_saver.skyPhase == SKY_NIGHT || g_saver.skyPhase == SKY_DUSK);
  bool canDay = (g_saver.skyPhase == SKY_DAY);

  // Per-step probability: shooting star ~1 per 10min = 1/(10*1090) ≈ 1/10900
  // Simplified: roll < threshold out of big space
  // Use a simpler scheme: every cooldown expiry, roll once for which event
  uint32_t baseWait = 300000UL + (lvglScreenSaverRandNext() % 300000UL); // 5-10 min

  if (canNight && roll < 300) {
    // Shooting star (30%)
    g_saver.eventActive = SAVER_EVENT_SHOOTING_STAR;
    g_saver.eventEndMs = nowMs + 1500UL;
    g_saver.eventX = (g_saver.eventDir > 0) ? -30 : canvasWidth();
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -30 : canvasWidth();
    // Cow reacts (save previous state for restoration)
    if (g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=shooting_star");
  } else if (canNight && roll < 350) {
    // Satellite (5%)
    g_saver.eventActive = SAVER_EVENT_SATELLITE;
    g_saver.eventEndMs = nowMs + 20000UL;
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -10 : canvasWidth();
    if ((lvglScreenSaverRandNext() % 100) < 30 && g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=satellite");
  } else if (roll < 380) {
    // UFO (3%, any phase)
    g_saver.eventActive = SAVER_EVENT_UFO;
    g_saver.eventEndMs = nowMs + 8000UL;
    g_saver.eventDir = ((lvglScreenSaverRandNext() & 1) == 0) ? 1 : -1;
    g_saver.eventX = (g_saver.eventDir > 0) ? -30 : canvasWidth();
    if (g_saver.cowState != COW_SLEEP) {
      g_saver.cowPrevState = g_saver.cowState;
      g_saver.cowState = COW_STARE_UP;
      g_saver.cowStateNextMs = g_saver.eventEndMs;
    }
    Serial.println("[SCRNSVR] event=ufo");
  } else if (canDay && roll < 420) {
    // Rain (4%, day only)
    g_saver.eventActive = SAVER_EVENT_RAIN;
    g_saver.eventEndMs = nowMs + 120000UL + (lvglScreenSaverRandNext() % 60000UL); // 2-3 min
    g_saver.eventX = 0; // not used for rain
    // Initialize 4 rain drop positions
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t rr = (uint8_t)(g_saver.rows - 2 + (i / 2));
      uint8_t ss = (uint8_t)(i % 2);
      if (rr >= kSaverSkyRowsMax) rr = (uint8_t)(kSaverSkyRowsMax - 1);
      int16_t rx = (int16_t)(8 + (lvglScreenSaverRandNext() % (uint32_t)(canvasWidth() - 20)));
      int16_t ry = (int16_t)(4 + (lvglScreenSaverRandNext() % 60));
      lvglScreenSaverBorrowStar(rr, ss, "|", rx, ry,
          activeUiTheme().lvgl.saverStarMid);
    }
    // Cow walks toward tree
    g_saver.cowState = COW_IDLE;
    g_saver.cowStateNextMs = g_saver.eventEndMs;
    Serial.println("[SCRNSVR] event=rain");
  } else if (roll < 435) {
    // Matrix glitch (1.5%, any phase)
    g_saver.eventActive = SAVER_EVENT_MATRIX_GLITCH;
    g_saver.eventEndMs = nowMs + 3000UL;
    g_saver.eventX = (int16_t)(40 + (lvglScreenSaverRandNext() % (uint32_t)(canvasWidth() - 80)));
    Serial.println("[SCRNSVR] event=matrix_glitch");
  } else {
    // No event this round — set next check
    g_saver.eventCooldownMs = nowMs + baseWait;
  }
}
```

- [ ] **Step 3: Compile**

Same compile command. Expected: compiles successfully.

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add random event system (shooting star, UFO, satellite, rain, matrix glitch)"
```

---

### Task 6: Cow state machine

**Files:**
- Modify: `scrybar.ino:10376-10510` (cow respawn + main loop cow movement)

- [ ] **Step 1: Add cow state transition function**

Insert before `lvglScreenSaverRespawnCow`:

```cpp
static void lvglScreenSaverTransitionCow(uint32_t nowMs) {
  // Exiting STARE_UP: restore previous state
  if (g_saver.cowState == COW_STARE_UP) {
    uint8_t prev = g_saver.cowPrevState;
    // Don't restore SLEEP during daytime
    if (prev == COW_SLEEP && g_saver.skyPhase != SKY_NIGHT) prev = COW_GRAZE;
    g_saver.cowState = prev;
    g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 5000UL);
    lvglScreenSaverSetCowArt(g_saver.cowDir);
    Serial.printf("[SCRNSVR] cowState=%u (restored from STARE_UP)\n", prev);
    return;
  }

  const uint32_t roll = lvglScreenSaverRandNext() % 100;
  uint8_t newState;

  if (g_saver.skyPhase == SKY_NIGHT) {
    if (roll < 60)      newState = COW_SLEEP;
    else if (roll < 80) newState = COW_IDLE;
    else if (roll < 95) newState = COW_GRAZE;
    else                newState = COW_RUN;
  } else {
    if (roll < 50)      newState = COW_GRAZE;
    else if (roll < 80) newState = COW_IDLE;
    else                newState = COW_RUN;
  }

  g_saver.cowState = newState;
  lvglScreenSaverSetCowArt(g_saver.cowDir);

  switch (newState) {
    case COW_GRAZE:
      g_saver.cowStateNextMs = nowMs + 8000UL + (lvglScreenSaverRandNext() % 12000UL); // 8-20s
      g_saver.cowChewNextMs = nowMs + 400UL;
      break;
    case COW_IDLE:
      g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 5000UL); // 3-8s
      g_saver.cowStepsLeft = 0;
      break;
    case COW_SLEEP:
      g_saver.cowStateNextMs = nowMs + 30000UL + (lvglScreenSaverRandNext() % 90000UL); // 30-120s
      g_saver.cowStepsLeft = 0;
      break;
    case COW_RUN:
      g_saver.cowStateNextMs = nowMs + 3000UL + (lvglScreenSaverRandNext() % 2000UL); // 3-5s
      g_saver.cowStepsLeft = 20; // lots of steps
      break;
    default: break;
  }
  Serial.printf("[SCRNSVR] cowState=%u\n", newState);
}
```

- [ ] **Step 2: Refactor cow movement in handleScreenSaverLoop**

Replace the cow movement block in `handleScreenSaverLoop` (the section from `if (nowMs >= g_saver.cowNextMoveMs)` through `lv_obj_set_pos(g_saver.cow, g_saver.x, g_saver.y)`) with:

```cpp
  // Cow state machine
  if (nowMs >= g_saver.cowStateNextMs && g_saver.cowState != COW_STARE_UP) {
    lvglScreenSaverTransitionCow(nowMs);
  }

  // Chew animation (only during GRAZE)
  if (g_saver.cowState == COW_GRAZE && nowMs >= g_saver.cowChewNextMs) {
    g_saver.cowChewFrame = (uint8_t)(1 - g_saver.cowChewFrame);
    g_saver.cowChewNextMs = nowMs + 400UL;
    lvglScreenSaverSetCowArt(g_saver.cowDir);
  }

  // Movement (GRAZE and RUN move, others don't)
  if ((g_saver.cowState == COW_GRAZE || g_saver.cowState == COW_RUN) &&
      nowMs >= g_saver.cowNextMoveMs) {
    const uint8_t stepPx = (g_saver.cowState == COW_RUN) ? 18 : 6;
    const uint32_t stepMs = (g_saver.cowState == COW_RUN) ? 120UL : 180UL;
    bool dirChanged = false;
    if (g_saver.cowStepsLeft == 0) {
      g_saver.cowStepsLeft = (uint8_t)(2U + (lvglScreenSaverRandNext() % 5U));
      if ((lvglScreenSaverRandNext() % 5U) == 0U) {
        g_saver.cowDir = -g_saver.cowDir;
        dirChanged = true;
      }
    }
    const int16_t minX = 8;
    const int16_t maxX = canvasWidth() - 250;
    int16_t nx = (int16_t)(g_saver.x + (g_saver.cowDir * stepPx));
    if (nx < minX) {
      nx = minX;
      g_saver.cowDir = 1;
      dirChanged = true;
    } else if (nx > maxX) {
      nx = maxX;
      g_saver.cowDir = -1;
      dirChanged = true;
    }
    if (dirChanged) lvglScreenSaverSetCowArt(g_saver.cowDir);
    g_saver.x = nx;
    if (g_saver.cowStepsLeft > 0) --g_saver.cowStepsLeft;
    g_saver.cowNextMoveMs = nowMs + ((g_saver.cowStepsLeft > 0) ? stepMs :
        (1000UL + (lvglScreenSaverRandNext() % 5000UL)));
  }

  // IDLE: occasional head turn
  if (g_saver.cowState == COW_IDLE && (lvglScreenSaverRandNext() % 200) == 0) {
    g_saver.cowDir = -g_saver.cowDir;
    lvglScreenSaverSetCowArt(g_saver.cowDir);
  }

  if (g_saver.cow) {
    lv_obj_set_pos(g_saver.cow, g_saver.x, g_saver.y);
  }
```

- [ ] **Step 3: Compile**

Same compile command. Expected: compiles successfully.

- [ ] **Step 4: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add cow state machine (graze/idle/sleep/run/stare_up)"
```

---

### Task 7: Expanded thought categories + new quotes

**Files:**
- Modify: `scrybar.ino:10137-10316` (quote arrays + balloon text setter)

- [ ] **Step 1: Add hacker, meta, weather, and easter egg quote arrays**

Insert after the last existing quote array (`kSaverQuotesBellazio`, ~line 10246), before `lvglScreenSaverQuotePackForLang`:

```cpp
// --- Hacker quotes (shared across niche languages that fall back to English) ---
static const char *const kSaverQuotesHackerEn[] = {
    "sudo rm -rf /grass",
    "my udder has 256 bits of entropy",
    "is this the matrix or just good pasture",
    "segfault in rumination module",
    "404: meaning not found",
    "I run on grass, not JavaScript",
    "localhost:8080/pasture",
    "have you tried turning the fence off and on",
};
static const char *const kSaverQuotesHackerIt[] = {
    "sudo rm -rf /erba",
    "la mia mammella ha 256 bit di entropia",
    "segfault nel modulo ruminazione",
    "404: senso non trovato",
    "funziono a erba, non a JavaScript",
    "hai provato a spegnere e riaccendere il recinto",
};
static const char *const kSaverQuotesHackerFr[] = {
    "sudo rm -rf /herbe",
    "mon pis a 256 bits d'entropie",
    "segfault dans le module rumination",
    "404: sens introuvable",
    "je tourne a l'herbe, pas au JavaScript",
};
static const char *const kSaverQuotesHackerDe[] = {
    "sudo rm -rf /gras",
    "mein Euter hat 256 Bit Entropie",
    "Segfault im Wiederkaumodul",
    "404: Sinn nicht gefunden",
    "ich laufe auf Gras, nicht JavaScript",
};
static const char *const kSaverQuotesHackerEs[] = {
    "sudo rm -rf /hierba",
    "mi ubre tiene 256 bits de entropia",
    "segfault en modulo de ruminacion",
    "404: sentido no encontrado",
    "funciono con hierba, no con JavaScript",
};
static const char *const kSaverQuotesHackerPt[] = {
    "sudo rm -rf /erva",
    "meu ubre tem 256 bits de entropia",
    "segfault no modulo de ruminacao",
    "404: sentido nao encontrado",
    "funciono com erva, nao com JavaScript",
};
static const char *const kSaverQuotesHackerLa[] = {
    "sudo rm -rf /herba",
    "uber meum CCLVI bits entropiae habet",
    "segfault in modulo ruminationis",
    "CDIV: sensus non inventus",
};

// --- Meta quotes (English only — niche langs fall back) ---
static const char *const kSaverQuotesMetaEn[] = {
    "I've been walking back and forth for hours",
    "someone is watching me on a tiny screen",
    "am I screensaver or screensavee",
    "this field is exactly 640 pixels wide",
    "the tree never moves. suspicious.",
};
static const char *const kSaverQuotesMetaIt[] = {
    "cammino avanti e indietro da ore",
    "qualcuno mi guarda su uno schermino",
    "sono io lo screensaver o lo screensavato",
    "questo campo e' largo esattamente 640 pixel",
    "l'albero non si muove mai. sospetto.",
};

// --- Weather quotes (English; selected by skyPhase) ---
static const char *const kSaverQuotesWeatherNightEn[] = {
    "nice stars tonight",
    "is that a satellite or a pixel",
    "3 AM and still no answers",
};
static const char *const kSaverQuotesWeatherDawnEn[] = {
    "another sunrise. still a cow.",
    "the gradient is beautiful today",
};
static const char *const kSaverQuotesWeatherDayEn[] = {
    "that cloud looks like a TCP packet",
    "solar powered contemplation",
};
static const char *const kSaverQuotesWeatherDuskEn[] = {
    "golden hour. still chewing.",
    "sunset commits are the best",
};
static const char *const kSaverQuotesWeatherRainEn[] = {
    "rain again. at least I'm not a server.",
    "cloud computing, literally",
};
static const char *const kSaverQuotesWeatherUfoEn[] = {
    "I saw something. nobody will believe me.",
};

// --- Easter egg quotes (shared across all languages) ---
static const char *const kSaverQuotesEasterEn[] = {
    "01101101 01101111 01101111",
    "mooooo",
    "< this space intentionally left blank >",
};
```

- [ ] **Step 2: Modify `lvglScreenSaverQuotePackForLang` to accept category**

Replace the function signature and body to dispatch by category:

```cpp
static void lvglScreenSaverQuotePackForLang(const char *const **items, uint8_t *count,
                                             uint8_t category) {
  if (!items || !count) return;

  // Easter egg — same for all languages
  if (category == THOUGHT_EASTER_EGG) {
    *items = kSaverQuotesEasterEn;
    *count = (uint8_t)(sizeof(kSaverQuotesEasterEn) / sizeof(kSaverQuotesEasterEn[0]));
    return;
  }

  // Weather — English only, selected by skyPhase/event
  if (category == THOUGHT_WEATHER) {
    if (g_saver.eventActive == SAVER_EVENT_RAIN) {
      *items = kSaverQuotesWeatherRainEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherRainEn) / sizeof(kSaverQuotesWeatherRainEn[0]));
    } else if (g_saver.eventActive == SAVER_EVENT_UFO) {
      *items = kSaverQuotesWeatherUfoEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherUfoEn) / sizeof(kSaverQuotesWeatherUfoEn[0]));
    } else if (g_saver.skyPhase == SKY_DAWN) {
      *items = kSaverQuotesWeatherDawnEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherDawnEn) / sizeof(kSaverQuotesWeatherDawnEn[0]));
    } else if (g_saver.skyPhase == SKY_DAY) {
      *items = kSaverQuotesWeatherDayEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherDayEn) / sizeof(kSaverQuotesWeatherDayEn[0]));
    } else if (g_saver.skyPhase == SKY_DUSK) {
      *items = kSaverQuotesWeatherDuskEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherDuskEn) / sizeof(kSaverQuotesWeatherDuskEn[0]));
    } else {
      *items = kSaverQuotesWeatherNightEn;
      *count = (uint8_t)(sizeof(kSaverQuotesWeatherNightEn) / sizeof(kSaverQuotesWeatherNightEn[0]));
    }
    return;
  }

  // Meta — IT and EN have translations, others fall back to EN
  if (category == THOUGHT_META) {
    if (strcmp(g_wordClockLang, "it") == 0) {
      *items = kSaverQuotesMetaIt;
      *count = (uint8_t)(sizeof(kSaverQuotesMetaIt) / sizeof(kSaverQuotesMetaIt[0]));
    } else {
      *items = kSaverQuotesMetaEn;
      *count = (uint8_t)(sizeof(kSaverQuotesMetaEn) / sizeof(kSaverQuotesMetaEn[0]));
    }
    return;
  }

  // Hacker — core 7 languages have translations, niche fall back to EN
  if (category == THOUGHT_HACKER) {
    *items = kSaverQuotesHackerEn;
    *count = (uint8_t)(sizeof(kSaverQuotesHackerEn) / sizeof(kSaverQuotesHackerEn[0]));
    if (strcmp(g_wordClockLang, "it") == 0) { *items = kSaverQuotesHackerIt; *count = (uint8_t)(sizeof(kSaverQuotesHackerIt) / sizeof(kSaverQuotesHackerIt[0])); }
    else if (strcmp(g_wordClockLang, "fr") == 0) { *items = kSaverQuotesHackerFr; *count = (uint8_t)(sizeof(kSaverQuotesHackerFr) / sizeof(kSaverQuotesHackerFr[0])); }
    else if (strcmp(g_wordClockLang, "de") == 0) { *items = kSaverQuotesHackerDe; *count = (uint8_t)(sizeof(kSaverQuotesHackerDe) / sizeof(kSaverQuotesHackerDe[0])); }
    else if (strcmp(g_wordClockLang, "es") == 0) { *items = kSaverQuotesHackerEs; *count = (uint8_t)(sizeof(kSaverQuotesHackerEs) / sizeof(kSaverQuotesHackerEs[0])); }
    else if (strcmp(g_wordClockLang, "pt") == 0) { *items = kSaverQuotesHackerPt; *count = (uint8_t)(sizeof(kSaverQuotesHackerPt) / sizeof(kSaverQuotesHackerPt[0])); }
    else if (strcmp(g_wordClockLang, "la") == 0) { *items = kSaverQuotesHackerLa; *count = (uint8_t)(sizeof(kSaverQuotesHackerLa) / sizeof(kSaverQuotesHackerLa[0])); }
    return;
  }

  // Philosophy (category 0) — existing arrays, unchanged
  *items = kSaverQuotesIt;
  *count = (uint8_t)(sizeof(kSaverQuotesIt) / sizeof(kSaverQuotesIt[0]));
  if (strcmp(g_wordClockLang, "en") == 0) { *items = kSaverQuotesEn; *count = (uint8_t)(sizeof(kSaverQuotesEn) / sizeof(kSaverQuotesEn[0])); return; }
  if (strcmp(g_wordClockLang, "fr") == 0) { *items = kSaverQuotesFr; *count = (uint8_t)(sizeof(kSaverQuotesFr) / sizeof(kSaverQuotesFr[0])); return; }
  if (strcmp(g_wordClockLang, "de") == 0) { *items = kSaverQuotesDe; *count = (uint8_t)(sizeof(kSaverQuotesDe) / sizeof(kSaverQuotesDe[0])); return; }
  if (strcmp(g_wordClockLang, "es") == 0) { *items = kSaverQuotesEs; *count = (uint8_t)(sizeof(kSaverQuotesEs) / sizeof(kSaverQuotesEs[0])); return; }
  if (strcmp(g_wordClockLang, "pt") == 0) { *items = kSaverQuotesPt; *count = (uint8_t)(sizeof(kSaverQuotesPt) / sizeof(kSaverQuotesPt[0])); return; }
  if (strcmp(g_wordClockLang, "la") == 0) { *items = kSaverQuotesLa; *count = (uint8_t)(sizeof(kSaverQuotesLa) / sizeof(kSaverQuotesLa[0])); return; }
  if (strcmp(g_wordClockLang, "eo") == 0) { *items = kSaverQuotesEo; *count = (uint8_t)(sizeof(kSaverQuotesEo) / sizeof(kSaverQuotesEo[0])); return; }
  if (strcmp(g_wordClockLang, "tlh") == 0) { *items = kSaverQuotesTlh; *count = (uint8_t)(sizeof(kSaverQuotesTlh) / sizeof(kSaverQuotesTlh[0])); return; }
  if (strcmp(g_wordClockLang, "l33t") == 0) { *items = kSaverQuotesL33t; *count = (uint8_t)(sizeof(kSaverQuotesL33t) / sizeof(kSaverQuotesL33t[0])); return; }
  if (strcmp(g_wordClockLang, "sha") == 0) { *items = kSaverQuotesSha; *count = (uint8_t)(sizeof(kSaverQuotesSha) / sizeof(kSaverQuotesSha[0])); return; }
  if (strcmp(g_wordClockLang, "val") == 0) { *items = kSaverQuotesVal; *count = (uint8_t)(sizeof(kSaverQuotesVal) / sizeof(kSaverQuotesVal[0])); return; }
  if (strcmp(g_wordClockLang, "bellazio") == 0) { *items = kSaverQuotesBellazio; *count = (uint8_t)(sizeof(kSaverQuotesBellazio) / sizeof(kSaverQuotesBellazio[0])); return; }
}
```

- [ ] **Step 3: Update `lvglScreenSaverSetBalloonText` to roll category**

In `lvglScreenSaverSetBalloonText()` (~line 10275), change the call to `lvglScreenSaverQuotePackForLang`:

Find:
```cpp
  lvglScreenSaverQuotePackForLang(&quotes, &n);
```

Replace with:
```cpp
  // Roll thought category with weighted probabilities
  const uint32_t catRoll = lvglScreenSaverRandNext() % 100;
  uint8_t cat;
  if (catRoll < 30)      cat = THOUGHT_PHILOSOPHY;
  else if (catRoll < 60) cat = THOUGHT_HACKER;
  else if (catRoll < 75) cat = THOUGHT_META;
  else if (catRoll < 90) cat = THOUGHT_WEATHER;
  else                    cat = THOUGHT_EASTER_EGG;
  g_saver.thoughtCategory = cat;
  lvglScreenSaverQuotePackForLang(&quotes, &n, cat);
```

- [ ] **Step 4: Compile**

Same compile command. Expected: compiles successfully.

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: add categorized thoughts (hacker/meta/weather/easter_egg) in 7 languages"
```

---

### Task 8: Wire everything into activation/deactivation + main loop

**Files:**
- Modify: `scrybar.ino:10398-10510` (lvglSetScreenSaverActive + handleScreenSaverLoop)

- [ ] **Step 1: Add new state resets to `lvglSetScreenSaverActive(true)`**

In the `if (on)` block of `lvglSetScreenSaverActive`, after `lvglScreenSaverRespawnCow()` and before `g_saver.lastStepMs = millis()`, insert:

```cpp
    // Pasture Simulator state init
    g_saver.eventActive = SAVER_EVENT_NONE;
    g_saver.eventEndMs = 0;
    g_saver.eventCooldownMs = millis() + 30000UL; // 30s grace before first event
    g_saver.starBorrowedMask = 0;
    g_saver.cowState = COW_GRAZE;
    g_saver.cowChewFrame = 0;
    g_saver.cowChewNextMs = millis() + 400UL;
    g_saver.cowStateNextMs = millis() + 8000UL + (lvglScreenSaverRandNext() % 12000UL);
    g_saver.thoughtCategory = THOUGHT_PHILOSOPHY;
    g_saver.cloudOffset = 0;
    g_saver.cloudNextMs = 0;
    g_saver.cowPrevState = COW_GRAZE;
    g_saver.skyNextMs = 0; // force immediate sky phase eval
    lvglScreenSaverUpdateSkyPhase(millis());
```

- [ ] **Step 2: Add cleanup to `lvglSetScreenSaverActive(false)`**

In the `else` block (deactivation), after `g_saver.wakeGuardUntilMs = millis() + 900UL;`, insert:

```cpp
    g_saver.eventActive = SAVER_EVENT_NONE;
    g_saver.starBorrowedMask = 0;
```

- [ ] **Step 3: Wire new update functions into `handleScreenSaverLoop`**

In `handleScreenSaverLoop`, after `g_saver.lastStepMs = nowMs;` and before the existing `lvglScreenSaverUpdateStars(nowMs);`, insert:

```cpp
  lvglScreenSaverUpdateSkyPhase(nowMs);
  lvglScreenSaverUpdateClouds(nowMs);
  lvglScreenSaverUpdateEvent(nowMs);
```

The existing calls to `lvglScreenSaverUpdateStars`, `lvglScreenSaverUpdateField`, `lvglScreenSaverUpdateBalloon`, `lvglScreenSaverUpdateFooter` remain as-is after these new calls.

- [ ] **Step 4: Compile**

Same compile command. Expected: compiles successfully. This is the first time all pieces are wired together.

- [ ] **Step 5: Commit**

```bash
git add scrybar.ino
git commit -m "screensaver: wire Pasture Simulator into activation and main loop"
```

---

### Task 9: Compile, upload, and test on device

**Files:**
- Modify: `config.h` (temporarily reduce idle timeout for testing)
- Modify: `scrybar.ino` (bump FW_BUILD_TAG)

- [ ] **Step 1: Bump firmware build tag**

In `config.h`, bump `FW_BUILD_TAG` to the next r-number and update `FW_RELEASE_DATE`.

- [ ] **Step 2: Temporarily reduce screensaver timeout for testing**

In `config.h`, change:
```cpp
#define SCREENSAVER_IDLE_USB_MS 5000UL
#define SCREENSAVER_IDLE_BATTERY_MS 5000UL
```

- [ ] **Step 3: Full compile**

```bash
arduino-cli compile --clean \
  --build-path /tmp/arduino-build-scrybar \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  /Users/enuzzo/Library/CloudStorage/Dropbox/Mitnick/scrybar
```

Expected: compiles successfully. Note flash/RAM usage.

- [ ] **Step 4: Upload to device**

```bash
arduino-cli upload -p /dev/cu.usbmodem83201 \
  --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=custom,PSRAM=opi \
  --input-dir /tmp/arduino-build-scrybar \
  /Users/enuzzo/Library/CloudStorage/Dropbox/Mitnick/scrybar
```

- [ ] **Step 5: Monitor serial output**

```bash
python3 -c "
import serial, time
s = serial.Serial('/dev/cu.usbmodem83201', 115200, timeout=3)
t0 = time.time()
while time.time() - t0 < 120:
    line = s.readline().decode('utf-8', errors='replace').strip()
    if line: print(line)
s.close()
"
```

Wait 5s for screensaver to activate. Watch for:
- `[SCRNSVR] ON`
- `[SCRNSVR] skyPhase=N`
- `[SCRNSVR] cowState=N`
- `[SCRNSVR] event=<name>` (may take a few minutes)
- No crashes or watchdog resets

- [ ] **Step 6: Visual verification checklist**

Verify on device:
- Sky shows clouds (day) or stars (night) based on current time
- Cow walks, chews (mouth alternates), stops (IDLE state)
- Thought balloon shows varied categories (watch for hacker/meta quotes)
- Footer shows time/date with jitter
- Touch wakes device immediately
- No visual glitches or overlapping labels

- [ ] **Step 7: Restore normal timeout**

In `config.h`, restore:
```cpp
#define SCREENSAVER_IDLE_USB_MS 7200000UL
#define SCREENSAVER_IDLE_BATTERY_MS 7200000UL
```

- [ ] **Step 8: Final compile + upload**

Repeat compile and upload with normal timeout.

- [ ] **Step 9: Commit**

```bash
git add scrybar.ino config.h
git commit -m "r249: screensaver Pasture Simulator — day/night, events, cow state machine, hacker thoughts"
```
