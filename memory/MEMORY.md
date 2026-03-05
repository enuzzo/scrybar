# ScryBar — Claude Memory

## Workflow Rules (user-confirmed)

- **Dopo ogni modifica al firmware: compile + flash automaticamente**, senza aspettare conferma.
- **Usare sempre flash completo** (`--clean`, scrittura dell'intero binario). Non usare flash incrementali o delta.
- Comandi canonici da `knowledge/project_knowledge.md`:
  - Porta seriale: usare sempre quella attualmente enumerata (`/dev/cu.usbmodem*`)
  - Compile: `arduino-cli compile --clean --build-path /tmp/arduino-build-scrybar --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi .`
  - Upload: `arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi --input-dir /tmp/arduino-build-scrybar .`

## Versioning

- `FW_BUILD_TAG` e `FW_RELEASE_DATE` in `config.h` — **incrementare r-number e aggiornare data ad ogni release**.
- Visibili su: web UI (hero card, sotto logo) e device INFO panel (`ScryBar Stats`, colonna destra).
- Corrente: `DB-M0-r143`, `2026-03-04`.

## Theming + Fonts (r143)

- Sistema temi runtime unificato su tre superfici:
  - firmware LVGL (`UiThemeLvglTokens`)
  - web config embedded (`UiThemeWebTokens`)
  - design system (`assets/scrybar_design_system`)
- Theme id attivi: `scrybar-default`, `cyberpunk-2077`, `toxic-candy`.
- Cambio tema:
  - seriale: `THEME <id>`
  - web/API: `ui_theme=<id>` su `/config` o `/api/config`
- Font per tema:
  - default: Montserrat (LVGL built-ins)
  - cyberpunk: `Space Mono` (`scry_font_space_mono_{12,16,20,24,28,32}`)
  - toxic candy: `Delius Unicase` (`scry_font_delius_unicase_{12,16,20,24,28,32}`)
- Font web aggiornati:
  - cyberpunk: stack monospace Space Mono
  - toxic: `Delius Unicase` come `--font-family`
- Orologio: auto-fit dinamico (`g_lvglClockL1`) con scelta della taglia massima che entra nello spazio.

## Architettura UI Localization

- `g_wordClockLang` è il pivot lingua globale (word clock + tutta la display UI).
- `src/ui_strings.h` — struct `UiStrings` + **14 istanze** (`kUiLang_it/en/fr/de/es/pt/la/eo/nap/tlh/l33t/sha/val/bellazio`).
- `activeUiStrings()` — dispatcher runtime in `scrybar.ino`.
- `weatherCodeUiLabel(code)` / `weatherCodeShort(code)` — dispatcher per label meteo.
- Web UI rimane in inglese per design.
- Selettore lingua nella web config si chiama **"System Language"**; lingue divise in `<optgroup>`: "Creative & Constructed" (bellazio, val, l33t, sha, nap, eo, la, tlh) e "Modern Languages" (en, it, es, fr, de, pt).
- **Recipe per aggiungere una lingua**: aggiungi `composeWordClockSentence*`, `weatherCodeShort*`, `weatherCodeUiLabel*`, `formatDate*`, `kUiLang_*`, registra in `kAllowed[]`, `kLangsFun[]`/`kLangsStd[]`, e tutti e 5 i dispatcher.

## Napoletano (nap) — vocabolario autentico (r140)

- Numeri: `cinche` (5), `diece` (10), `nu quarto` (15), `vinte` (20), `vinte e cinche` (25), `mmeza` (30)
- Ore speciali: `ll'una` (1), `'e seje` (6), `'e unnece` (11), `'e dudece` (12)
- Struttura "past": `So' 'e tre e nu quarto` (3:15) / `E' ll'una e cinche` (1:05)
- Struttura "to": `'E quatte manco nu quarto` (3:45) / `'E sette manco vinte` (6:40)
- Mesi: jennaro, frevaro, marzo, abbrile, maggio, giugno, luglio, austo, settembre, uttombre, nuvembre, decembre
- Giorni: lunnerì, marterì, miercurì, gioverì, viernarì, sabbato, dummeneca
- Meteo: assulato (clear), ce sta 'o sole (mainly clear), schizzechea (drizzle), chiove (rain), 'a neva (snow), tempurale (storm)

## INFO Panel (`lvglCreateScreenLayout` + `lvglUpdateInfoPanel`)

- QR: dimensione adattiva `min(130, infoColsH/2)`, ancorato `LV_ALIGN_BOTTOM_MID` — non usare `lv_obj_set_pos()` per il QR.
- Sfondo uniforme nero (`#000000`) su entrambe le colonne, nessun bordo colorato, divisore neutro grigio 1px tra le colonne.
- Colonna sinistra: wifi/ssid/bat/pwr/src/dns/mac — senza label `[net]` e senza `ip:` (già in navbar header).
- Colonna destra: titolo dinamico con `FW_BUILD_TAG`, poi uptime/heap/cpu/lang/ntp; QR in basso.
