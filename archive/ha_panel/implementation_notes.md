# Home Assistant Panel — Archive Notes (r228)

## Feature Description

The HA panel was a dedicated swipe page showing:
- **Left column**: 4 sensor badges (label + live value from HA API)
- **Right column**: 2x2 control grid (toggle switches / buttons triggering HA services)
- **QR fallback**: shown when HA is not configured, pointing to the web config URL

Polling used the background netTask on Core 1 (NET_REQ_HA_POLL / NET_REQ_HA_SERVICE).
Touch on the control grid called haCallService() which enqueued NET_REQ_HA_SERVICE.

## Files Modified (r228)

- `config.h` — HA_BADGE_COUNT, HA_CONTROL_COUNT, timing constants, HA_CTRL_* macros,
  HaBadgeConfig, HaControlConfig, HaConfig structs (lines 52–84)
- `scrybar.ino` — all implementation (see sections below)

## Key Symbols to Add Back

### config.h
Copy the contents of `archive/ha_panel/config_additions.h` back into `config.h`
after the `TouchReleaseInfo` struct (around line 51).

### scrybar.ino — in order of appearance

1. **UI flag** (~line 706): `static constexpr uint8_t UI_VIEW_FLAG_HA = 0x40;`
   Add to `UI_VIEW_MASK_DEFAULT`.

2. **Page enum** (~line 915): `UI_PAGE_HA = 6,` in `enum UiPageMode`.

3. **HaState struct** (~line 841):
   ```cpp
   struct HaState {
     char    values[HA_BADGE_COUNT][32];
     char    units[HA_BADGE_COUNT][16];
     bool    controlStates[HA_CONTROL_COUNT];
     bool    valid       = false;
     uint32_t lastFetchMs = 0;
     bool    fetchError  = false;
     bool    dirty       = false;
   };
   static HaConfig  g_haConfig  = {};
   static HaState   g_haState   = {};
   ```

4. **NetRequestType** (~line 864):
   ```cpp
   NET_REQ_HA_POLL,      // fetch all HA badge + toggle states
   NET_REQ_HA_SERVICE,   // fire a service, then re-poll
   ```

5. **Forward declarations** (~line 883):
   ```cpp
   static void netFetchHaStates();
   static void netFetchHaService(uint8_t ctrlIdx);
   static bool updateHaFromApi(bool force);
   static void haCallService(uint8_t ctrlIdx);
   ```

6. **LvglHaUi struct** (~line 1043): full struct with header/panel/badge/ctrl/qr fields.

7. **Globals** (~line 1110):
   ```cpp
   static LvglHaUi g_haUi;
   static lv_obj_t *g_lvglHaRoot = nullptr;
   ```

8. **NVS helpers** (~line 3793): `nvsLoadHaConfig()`, `nvsSaveHaConfig()`

9. **NVS loader call** (~line 3874): `nvsLoadHaConfig(prefs);`

10. **NVS save call** (~line 4004): `const size_t nHa = nvsSaveHaConfig(prefs);`
    Also restore `ha=%u` in the Serial.printf format and `(unsigned)nHa` in the args.

11. **Web section** (~line 4519): `buildWebHaSection()` function (~90 lines)
    Caller in `buildWebConfigPage()`: `buildWebHaSection(html);`

12. **parseHaConfig** (~line 4922): ~65-line POST parser function
    Caller in `applyRuntimeConfigFromRequest()`: `if (!parseHaConfig(errorOut, hasInput)) return false;`

13. **webRequestHasConfigParams** (~line 5465): `if (g_webCfg.server.hasArg("ha_url")) return true;`

14. **LVGL forward decls** (~line 8430):
    ```cpp
    static void lvglInitHaUi(LvglHaUi &ui, lv_obj_t *root);
    static void lvglUpdateHaUi(LvglHaUi &ui, bool force);
    ```

15. **uiPageName** (~line 8452): `case UI_PAGE_HA: return "HA";`

16. **uiViewFlagForPage** (~line 8467): `case UI_PAGE_HA: return UI_VIEW_FLAG_HA;`

17. **uiPageInSwipeCarousel** (~line 8499): `case UI_PAGE_HA:`

18. **kSwipePageOrder** (~line 8513): `UI_PAGE_HA,`

19. **lvglApplyThemeStyles HA block** (~line 9361): the `if (g_haUi.header) { ... }` block

20. **handleFeedDeckTapRelease** (~line 10466): the `if (r.isTap && g_uiPageMode == UI_PAGE_HA) { ... return; }` block

21. **netTask cases** (~line 8148):
    ```cpp
    case NET_REQ_HA_POLL: { netFetchHaStates(); ... break; }
    case NET_REQ_HA_SERVICE: { netFetchHaService(req.param); ... break; }
    ```

22. **Net fetch functions** (~line 7962):
    `netFetchHaStates()`, `netFetchHaService()`, `updateHaFromApi()`, `haCallService()`

23. **LVGL init functions** (~line 13432):
    `lvglInitHaUi()` (~130 lines), `lvglUpdateHaUi()` (~87 lines)

24. **initLvglUi HA section** (~line 14379):
    ```cpp
    g_lvglHaRoot = lvglCreatePageRoot(scr, cW, cH);
    lv_obj_set_pos(g_lvglHaRoot, cW, 0);
    lvglInitHaUi(g_haUi, g_lvglHaRoot);
    ```

25. **updateLvglUi HA case** (~line 14600):
    ```cpp
    if (g_uiPageMode == UI_PAGE_HA) {
      lvglUpdateHaUi(g_haUi, force);
      g_clock.lastSecond = timeinfo.tm_sec;
      g_clock.lastDateKey = dateKey;
      g_uiNeedsRedraw = false;
      return;
    }
    ```

26. **loop() call** (~line 15787): `updateHaFromApi(false);`

27. **lvglApplyPageDrag** (~line 12462):
    ```cpp
    if (g_lvglHaRoot) lv_anim_del(g_lvglHaRoot, lvglSetObjXAnim);
    {UI_PAGE_HA, g_lvglHaRoot},  // in pages[] array
    ```

28. **lvglApplyPageVisibility** (~line 12530):
    `{g_lvglHaRoot, UI_PAGE_HA, 0},`  // in slots[] array

29. **JSON API** (~line 4819): `out += F(",\"doom\":"); ...` (the doom/ha views were adjacent —
    optionally add `"ha":` field to the JSON views object)

## NVS Keys
`ha_url`, `ha_token`, `ha_b0_eid`..`ha_b3_eid`, `ha_b0_lbl`..`ha_b3_lbl`,
`ha_b0_unit`..`ha_b3_unit`, `ha_c0_type`..`ha_c3_type`, `ha_c0_lbl`..`ha_c3_lbl`,
`ha_c0_eid`..`ha_c3_eid`, `ha_c0_dom`..`ha_c3_dom`, `ha_c0_svc`..`ha_c3_svc`
