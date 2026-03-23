# M4 — Serial Command Dispatch Table Design Spec

**Date:** 2026-03-23
**Milestone:** M4 (Firmware Polishing Roadmap)
**Scope:** `scrybar.ino` — `handleSerialCommand()` (lines 14603–15015, 412 lines)

---

## Problem

`handleSerialCommand()` is a 412-line if/else chain dispatching ~40 serial commands. Adding a new command requires inserting another if-block and manually updating the hardcoded HELP string. Aliases (e.g., VIEW0/VIEWINFO, WEBCFG/WEB) create duplicate code paths. Commands with arguments (THEME, LANG, WIFIDIRECT) use `startsWith()` checks interleaved with exact-match checks, making the flow hard to follow.

## Approach

**Table-driven dispatch** — replace the if/else chain with a `SerialCmd` struct array and a linear scan lookup. Each command becomes a small self-contained handler function. The orchestrator splits the input on the first space (command name + args), looks up the table, and calls the handler. HELP auto-generates from the table — no more hardcoded command list.

## Architecture

```
handleSerialCommand()     ~20 lines (split + lookup + dispatch)
kSerialCmds[]             ~55 entries (commands + aliases)
cmd* handler functions    ~30 functions (self-contained, 3-50 lines each)
```

### Dispatch Table Structure

```c
struct SerialCmd {
  const char *name;              // uppercase command name
  void (*handler)(const String &args);  // handler, args = rest after first space
};

static const SerialCmd kSerialCmds[] = {
  { "HELP",          cmdHelp },
  { "SNAP",          cmdSnap },
  { "SCREENSHOT",    cmdSnap },
  // ...
};
```

### Orchestrator: `handleSerialCommand(const char *line)`

~20 lines:
1. Null/empty check
2. `raw = String(line); raw.trim()` — preserve original case
3. `cmd = raw; cmd.toUpperCase()` — uppercase copy for matching
4. Split on first space in `cmd` to get `cmdName` (uppercase). Extract `cmdArgs` from `raw` (original case) at same position, then trim. If no space, `cmdArgs` is empty String.
5. Linear scan `kSerialCmds[]` — `strcmp(cmdName.c_str(), entry.name)`
6. Call `entry.handler(cmdArgs)` on match, return
7. Print unknown command warning if no match

**Critical:** args must be extracted from `raw` (original case), not from `cmd` (uppercased), so `LANG it` preserves the lowercase `it`.

### HELP Auto-generation

`cmdHelp()` loops `kSerialCmds[]` and prints all names. No more hardcoded string. Adding a command to the table automatically adds it to HELP output.

### Handler Functions (30 distinct)

Each handler receives `const String &args` (empty for no-arg commands). Commands that support both no-arg and with-arg (THEME, LANG, WIFIDIRECT) branch on `args.length() == 0`.

| Handler | Commands (aliases) | Lines | `#if` guard |
|---------|-------------------|-------|-------------|
| `cmdHelp` | HELP | 8 | — |
| `cmdSnap` | SNAP, SCREENSHOT | 6 | — (guard inside handler body, `#else` prints error) |
| `cmdViewToggle` | VIEW, VIEWTOGGLE | 3 | — |
| `cmdViewFirst` | VIEWFIRST, VIEWHOMEFIRST | 3 | — |
| `cmdViewLast` | VIEWLAST, VIEWAUXLAST, VIEWRSSLAST | 3 | — |
| `cmdViewInfo` | VIEW0, VIEWINFO | 3 | — |
| `cmdViewHome` | VIEW1, VIEWHOME | 3 | — |
| `cmdViewAux` | VIEW2, VIEWAUX, VIEWRSS | 6 | — |
| `cmdViewWiki` | VIEW3, VIEWWIKI | 6 | — |
| `cmdViewNowPlaying` | VIEW4, VIEWNOW, VIEWNP, VIEWPLAY | 3 | — |
| `cmdViewDoom` | VIEW5, VIEWDOOM, DOOM | 6 | — |
| `cmdTheme` | THEME | 25 | partial `TEST_WIFI`, `TEST_DISPLAY` |
| `cmdLang` | LANG | 18 | partial `TEST_WIFI` |
| `cmdLangStat` | LANGSTAT | 8 | — (status-only, ignores args) |
| `cmdQrOn` | QRON | 3 | — |
| `cmdQrOff` | QROFF | 3 | — |
| `cmdQrToggle` | QRTOGGLE | 3 | — |
| `cmdSaverOn` | SAVERON | 3 | `TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED` |
| `cmdSaverOff` | SAVEROFF | 4 | `TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED` |
| `cmdSaverStat` | SAVERSTAT | 8 | `TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED` |
| `cmdPwrStat` | PWRSTAT | 7 | — |
| `cmdNavStat` | NAVSTAT | 7 | — |
| `cmdPwrOff` | PWROFF | 3 | — |
| `cmdPwrOffHard` | PWROFFHARD | 3 | — |
| `cmdBatStat` | BATSTAT | 8 | `TEST_BATTERY` |
| `cmdWebCfg` | WEBCFG, WEB | 45 | `TEST_WIFI && WEB_CONFIG_ENABLED` |
| `cmdWifiDirect` | WIFIDIRECT, WIFISETUP | 18 | — |
| `cmdRssDiag` | RSSDIAG | 5 | `TEST_WIFI` |
| `cmdRssStat` | RSSSTAT | 12 | `TEST_WIFI && RSS_ENABLED` |
| `cmdWikiStat` | WIKISTAT | 12 | `TEST_WIFI && RSS_ENABLED` |
| `cmdRssReload` | RSSRELOAD | 10 | `TEST_WIFI && RSS_ENABLED` |
| `cmdWikiReload` | WIKIRELOAD | 10 | `TEST_WIFI && RSS_ENABLED` |
| `cmdReload` | RELOAD | 12 | `TEST_WIFI` |

### Conditional Compilation in Table

Commands behind `#if` guards wrap both the handler definition AND the table entries:

```c
#if TEST_DISPLAY && TEST_NTP && TEST_LVGL_UI && SCREENSAVER_ENABLED
  { "SAVERON",  cmdSaverOn },
  { "SAVEROFF", cmdSaverOff },
  { "SAVERSTAT", cmdSaverStat },
#endif
```

This ensures the table size adjusts at compile time and disabled commands don't appear in HELP.

**Note:** `cmdSnap` is always registered (no table-level guard). The `#if TEST_DISPLAY && DISPLAY_BACKEND_ESP_LCD` guard lives inside the handler body, with the `#else` branch printing an error message — matching current behavior exactly.

### Merged handlers: THEME, LANG, WIFIDIRECT

These commands handle both "show status" (no args) and "set value" (with args) in a single handler:

```c
static void cmdTheme(const String &args) {
  if (args.length() == 0) {
    // print current theme + list all themes
    return;
  }
  // validate + set theme
}
```

## Self-contained Pattern

Each handler function reads globals and calls runtime accessors directly (same M1/M3 pattern). No parameters threaded from the orchestrator beyond `args`.

## Invariants

1. **Identical serial behavior** — every command produces the same output as before
2. **All aliases preserved** — VIEW0/VIEWINFO, WEBCFG/WEB, etc.
3. **All `#if` guards preserved** — disabled features don't register commands
4. **HELP output shows all registered commands** (auto-generated, may differ in order)
5. **`pollSerialCommands()` unchanged** — still calls `handleSerialCommand(buf)`
6. **No new files** — all in `scrybar.ino`

## Measurable Targets

| Metric | Before | After |
|--------|--------|-------|
| `handleSerialCommand()` lines | 412 | ~20 |
| Largest handler | — | <50 lines (`cmdWebCfg`) |
| Total `if (cmd ==` / `cmd.startsWith` | ~35 | 0 |
| Adding a new command requires | new if-block + edit HELP string | 1 table entry + 1 handler |

## Verification Steps

1. `arduino-cli compile --clean` — zero warnings
2. Upload to device, hard reset
3. Send `HELP` — verify all commands listed
4. Test representative commands: `THEME`, `THEME <id>`, `LANG`, `LANG it`, `VIEW0`, `BATSTAT`, `WEBCFG`, `WIFIDIRECT`, `RELOAD`
5. Send unknown command — verify warning message

## Build Tag

Bump `FW_BUILD_TAG` to `DB-M4-r203` and `FW_RELEASE_DATE` to `2026-03-23` after verification.

## Commit

```
refactor(firmware): M4 — table-driven serial command dispatch (412 → ~20 lines)
```
