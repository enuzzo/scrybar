# Display copy corrections — 2026-10-23

This copy review changes only `src/ui_strings.h`. The source-hashed baseline remains in `baseline.json`. `strings-candidate.json` records the first 13 edits against the original r320 `.ino`; `candidate-expanded.json` records all 21 string edits together with the separately owned palette/widget corrections. Each report retains its exact source hashes.

All 14 language tables were inspected. Twenty-one fields changed:

| Language | Fields | Change |
|---|---|---|
| French | `weatherOffline`, `touchToCloseAnywhere`, `generatingQr`, `rssOffline`, `rssFeedError`, `rssDisabled` | Restore **Météo**, **où**, **Génération du QR**, **réessayez**, **bientôt** and **désactivé** on paths that preserve supported Unicode. |
| German | `rssSyncing`, `rssDisabled`, `rssFeedError` | Replace the incomplete hybrid “wird sync” with **RSS wird synchronisiert...**, simplify the disabled label to **RSS deaktiviert**, and use **verfügbar**. |
| Spanish | `weatherOffline`, `rssOffline`, `rssFeedError` | Restore accents in **conexión**, **Conéctate** and **automático**. |
| Portuguese | `rssOffline`, `rssFeedError` | Use the pronominal imperative **Conecte-se ao WiFi** and the accents in **indisponível/automática**, preserving the table’s existing Portuguese variety. |
| Esperanto | `touchToClose`, `touchToCloseAnywhere`, `rssFeedError` | Replace **Tusu** (“cough”) with **Tuŝu** (“touch”), and restore **baldaŭ**. |
| Pirate | `forecast3h`, `forecastNa` | Replace the misleading “3 bells” with **In 3h, matey**, keeping the nautical voice. |
| Klingon | `forecast3h`, `forecastNa` | Replace the past-oriented **qen 3rep** with **wej rep pIq**, meaning three hours from now. |

Bellazio and Shakespeare are byte-for-byte unchanged. Pirate’s other fields, including “Cap’n”, “Hoist the WiFi flag”, “scallywag” and the treasure-map QR message, are unchanged. Italian, English, Latin, leetspeak and Valley Girl are also unchanged. German **Schliessen** remains valid Swiss spelling rather than being replaced with a different regional convention.

The Portuguese pronominal construction is supported by [Priberam](https://dicionario.priberam.org/conectar-se). Klingon future timing follows the [Klingon Language Institute lesson](https://www.kli.org/duolingo/say-when-an-event-was-2/) and its [documented `pIq` construction](https://www.kli.org/tlhIngan-Hol/2003/March/msg00098.html). The Esperanto distinction is documented in [Reta Vortaro](https://www.retavortaro.de/revo/art/tus.html). Nautical bells mark half-hour stages within a watch, as described by the [US Coast Guard](https://www.mycg.uscg.mil/News/Article/3616329/eight-bells-and-all-is-well/).

## Verification

- Seven focused audit/parser tests pass.
- All 196 format signatures match the baseline exactly; no placeholder was added, removed or changed in type.
- Native LVGL 8 measured 478 raw/formatted fixtures across all 14 language tables and targeted widgets: no missing glyphs, fallback glyphs or unhandled bound overflow. The expanded report also checks 532 color pairs, with no required/advisory contrast or theme-lifecycle findings on that source snapshot.
- The longest updated Pirate forecast measures **170px within its 195px widget**. The longer French QR phrase and Esperanto **ŝ** are supported by the actual Dosis font and fit the intended QR hint bounds.
- The original baseline file and its source hashes were preserved. No firmware build, USB operation, Git operation or `.ino`/configuration edit was performed for this review.

## Remaining scope

The separate firmware correction now folds only external article text to ASCII (`showIndex >= 0`), preserving supported Unicode in internal RSS offline/sync/error messages. The final eight accent fixes were applied after verifying that real path. External article handling remains unchanged. The separate widget correction also connects the localized QR string fields; they were unused in the baseline.

The actual km/h-to-knots value conversion is part of the separate weather-widget correction; the Pirate unit string remains **knots**. This review does not replace physical display checks or specialist proofreading of every Latin/Klingon formulation. Remaining playful language choices were retained instead of being rewritten speculatively.
