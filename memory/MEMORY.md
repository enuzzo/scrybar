# ScryBar — Claude Memory

## Workflow Rules (user-confirmed)

- **Dopo ogni modifica al firmware: compile + flash automaticamente**, senza aspettare conferma.
- **Usare sempre flash completo** (`--clean`, scrittura dell'intero binario). Non usare flash incrementali o delta.
- Comandi canonici da `knowledge/project_knowledge.md`:
  - Port fisso: `/dev/cu.usbmodem83101`
  - Compile: `arduino-cli compile --clean --build-path /tmp/arduino-build-scrybar --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi .`
  - Upload: `arduino-cli upload -p /dev/cu.usbmodem83101 --fqbn esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi --input-dir /tmp/arduino-build-scrybar .`

## Versioning

- `FW_BUILD_TAG` e `FW_RELEASE_DATE` in `config.h` — **incrementare r-number e aggiornare data ad ogni release**.
- Visibili su: web UI (hero card, sotto logo) e device INFO panel (`ScryBar Stats`, colonna destra).
- Corrente: `DB-M0-r136`, `2026-02-28`.

## Architettura UI Localization

- `g_wordClockLang` è il pivot lingua globale (word clock + tutta la display UI).
- `src/ui_strings.h` — struct `UiStrings` + 10 istanze statiche (`kUiLang_it/en/fr/de/es/pt/la/eo/nap/tlh`).
- `activeUiStrings()` — dispatcher runtime in `scrybar.ino`.
- `weatherCodeUiLabel(code)` / `weatherCodeShort(code)` — dispatcher per label meteo.
- Web UI rimane in inglese per design.
- Selettore lingua nella web config si chiama **"System Language"** (non più "Word Clock Language").

## INFO Panel (`lvglCreateScreenLayout` + `lvglUpdateInfoPanel`)

- QR: dimensione adattiva `min(130, infoColsH/2)`, ancorato `LV_ALIGN_BOTTOM_MID` — non usare `lv_obj_set_pos()` per il QR.
- Sfondo uniforme nero (`#000000`) su entrambe le colonne, nessun bordo colorato, divisore neutro grigio 1px tra le colonne.
- Colonna sinistra: wifi/ssid/bat/pwr/src/dns/mac — senza label `[net]` e senza `ip:` (già in navbar header).
- Colonna destra: titolo dinamico con `FW_BUILD_TAG`, poi uptime/heap/cpu/lang/ntp; QR in basso.
