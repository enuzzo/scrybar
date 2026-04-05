# Pirate Language (`"pir"`) — Design Spec

**Date:** 2026-04-05
**Status:** Approved

## Overview

Add Pirate English as a new creative/constructed language to ScryBar's i18n system. Follows the existing vtable pattern (14th language, joining the "Creative & Constructed" group alongside Klingon, l33t, Shakespearean, Valley Girl, Bellazio, Latin, Esperanto).

Language code: `"pir"`

## Tone

Over-the-top pirate: heavy on exclamations, insults, nautical jargon. Every string should make you grin.

## Components

### 1. Word Clock (`composeWordClockSentencePir`)

Rotating pirate exclamations based on minute position, with insult rotation via `tm_min % 4`:

| Minute | Pattern |
|--------|---------|
| `:00` | "Blimey! It be X o'clock, ye {insult}!" |
| `:05-:25` | "{Exclaim}! It be N past X, ye {insult}!" |
| `:15` | "Shiver me timbers! Quarter past X!" |
| `:30` | "Arrr! Half past X, ye {insult}!" |
| `:35-:55` | "{Exclaim}! It be N to X, ye {insult}!" |
| `:45` | "Avast! Quarter to X, ye {insult}!" |

**Insult rotation** (`tm_min % 4`): scurvy dog, landlubber, bilge rat, barnacle brain

**Exclamation rotation**: Yarr, Ahoy, Avast, Arrr

**Hour words**: one through twelve, standard English (pirates spoke English).

### 2. Weather Short Labels (`kWeatherShortPir`)

| Field | Value |
|-------|-------|
| clear | "Fair Winds" |
| cloudy | "Gloomy" |
| overcast | "Grey Skies" |
| fog | "Fog Bank" |
| rain | "Squall" |
| snow | "Blizzard" |
| storm | "Tempest" |
| na | "????" |

### 3. Weather UI (24 WMO entries, `kWeatherUiPir`)

Full pirate flavor for each WMO condition:

0. "Clear skies ahead, smooth sailin'!"
1. "Mostly clear, a fine day to plunder!"
2. "Partly cloudy, keep yer eyes peeled!"
3. "Overcast — grey as Davy Jones' locker!"
4. "Fog bank rollin' in, can't see the bow!"
5. "Icy fog — the sea be cursed!"
6. "Light drizzle on the poop deck"
7. "Drizzle comin' down steady"
8. "Heavy drizzle, batten down!"
9. "Freezin' drizzle — the riggin's icin' up!"
10. "Light rain, nothin' a pirate can't handle"
11. "Rain fallin' like cannonballs"
12. "Heavy rain — man the bilge pumps!"
13. "Freezin' rain — the ship be glazed!"
14. "Light snow, flurries on the quarterdeck"
15. "Snow fallin' thick as stolen gold"
16. "Heavy snow — blizzard on the high seas!"
17. "Snow grains peltin' the crew"
18. "Light rain showers, a spit from the sky"
19. "Rain showers blowin' sideways"
20. "Heavy showers — it be a deluge!"
21. "Snow showers mixin' with the squall"
22. "Thunderin' tempest — all hands on deck!"
23. "Tempest with cannonball hail — take cover!"

### 4. UI Strings (`kUiLang_pir`)

| Field | Value |
|-------|-------|
| windFmt | `"Wind %.0f knots"` |
| windNa | `"Wind -- knots"` |
| forecast3h | `"In 3 bells: %s"` |
| forecastNow | `"Now: %s"` |
| forecastNa | `"In 3 bells: --"` |
| weatherOffline | `"Weather be offline, Cap'n"` |
| wifiOff | `"WiFi be down"` |
| rssOffline | `"RSS be offline.\nHoist the WiFi flag\nan' try again."` |
| rssFeedError | `"Feed be lost at sea.\nSearchin' the horizon\nautomatically."` |
| rssSyncing | `"Syncin' the RSS...\nHold fast, matey.\n"` |
| rssDisabled | `"RSS be disabled in config."` |
| touchToClose | `"Tap to dismiss, ye scallywag"` |
| touchToCloseAnywhere | `"Tap anywhere to dismiss, ye scallywag"` |
| generatingQr | `"Drawin' the treasure map..."` |

### 5. Date Formatting (`formatDatePir`)

Format: `"Saturday the 5th of April, Year of Our Plunder 2026"`

- Standard English weekday/month names
- Ordinal day suffix (1st, 2nd, 3rd, 4th...)
- "Year of Our Plunder" instead of plain year

### 6. Web UI Registration

Add `{"pir", "Pirate"}` to `kLangsFun[]` (Creative & Constructed optgroup).

## Files Modified

| File | Change |
|------|--------|
| `src/ui_strings.h` | Add `kUiLang_pir` struct |
| `scrybar.ino` | Add weather labels, word clock fn, date fn, vtable entry, web UI option |

## Testing

- Compile clean
- Flash and switch language to "pir" via web config
- Verify word clock text at various times
- Verify weather labels display correctly
- Verify date formatting on INFO page
