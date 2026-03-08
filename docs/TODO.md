# ScryBar — Future Views (idee post-Snow)
# Scritto: 2026-03-08

Queste sono idee per nuove "view" da aggiungere come pagine LVGL
navigabili con swipe, in aggiunta alle 4 correnti (INFO → HOME → AUX → WIKI).

Ogni view ha: descrizione, fattibilità tecnica, effort stimato, rischi.

---

## 1. DOOM — "ci gira Doom?"

**Concept:** Prima view interattiva/game. Doom shareware (Episode 1) sul display,
controlli: IMU gyroscope per girarsi, touch per sparare/usare/avanzare.

**Fattibilità tecnica:**
- CPU ESP32-S3 @ 240MHz >> 33MHz i486 originale. Port ESP32 già esistenti su GitHub
  (es. `fabgl/doom`, `nicowillis/doom-esp32`).
- RAM: 8MB OPI PSRAM — Doom ha bisogno di ~1.5-2MB. Abbondante.
- WAD: partition `app3M_fat9M_16MB` ha 9MB FAT. `doom1.wad` (shareware) è 4.1MB. Ci sta.
- Display: 640×172 è ultra-widescreen (3.7:1). Soluzione: renderizzare a 320×172,
  pillar-box con barre nere, oppure crop verticale centrato da 320×200.
- Audio: nessun DAC accessibile → muto o buzzer (skip per v1).
- Pipeline: il framebuffer DMA attuale va bypassato per Doom; serve scrittura diretta
  dell'area display dal task loop, non dal tile-transpose.

**Controlli con IMU:**
- `IMU_SHAKE_GYRO_DPS` già letto → soglie per turn left/right
- Gyro asse Z → rotazione orizzontale
- Tilt avanti/indietro → walk forward/backward
- Touch screen: tap sinistro = use, tap destro = fire

**Effort:** ALTO — 2-3 sessioni intensive
**Rischio:** MEDIO (port codice, adattamento display pipeline)

---

## 2. ANSI / BBS Art — "la view più figa"

**Concept:** Mostra ANSI art in stile BBS anni '90. Fonte: API di 16colo.rs
(archivio storico ANSI/ASCII art). Nuova arte ogni N minuti, con scroll
verticale per pezzi alti. Palette CGA 16 colori.

**Fattibilità tecnica:**
- 16colo.rs REST API: `https://16colo.rs/api/v1/` — free, nessuna auth.
  Endpoint: `/pack/{pack}/file/{filename}/view` restituisce il file ANSI raw.
- ANSI parser: CSI escape sequences (`ESC[` + codici colore `30-37`, `40-47`,
  `90-97`, `100-107`). Da implementare (~200 righe).
- Font: IBM PC 8×16 (CP437) — da includere come array C o come font LVGL bitmap.
  Alla dimensione 8×16: 640/8=80 cols × 172/16=10 rows → esattamente 80×10.
  Perfetto per ANSI classica a 80 colonne!
- Render: ogni carattere = lv_obj con background/foreground color, oppure
  direttamente sul canvas con fillRectCanvas + drawChar.
- Scrolling: per ANSI alte (tipicamente 25+ righe) → scroll verticale automatico
  come un terminale.
- Blink attribute: supportare lampeggio (`ESC[5m`) con timer LVGL.

**Dataset suggerito:** pack "blocktronics", "fire", "lazarus" su 16colo.rs.
Possibile curare una lista locale di URL per evitare dipendenza dalla rete.

**Effort:** MEDIO — 1-2 sessioni
**Rischio:** BASSO (nessuna dipendenza hardware, solo parsing testo)

---

## 3. Crypto Ticker

**Concept:** BTC / ETH / SOL prezzi in tempo reale con variazione 24h e
mini sparkline. Aggiornamento ogni 60 secondi.

**Fattibilità tecnica:**
- CoinGecko API v3 — free tier, no auth:
  `/api/v3/simple/price?ids=bitcoin,ethereum,solana&vs_currencies=usd&include_24hr_change=true`
- Sparkline: `/api/v3/coins/{id}/market_chart?vs_currency=usd&days=1&interval=hourly`
  → array di 24 prezzi → grafico lineare con lv_chart o drawLine custom.
- Struttura dati: estendere RssState / creare CryptoState (~50 byte).
- Rate limit: 30 req/min free → abbondante per aggiornamenti ogni 60s.

**Effort:** BASSO — 1 sessione
**Rischio:** BASSO

---

## 4. Snake

**Concept:** Snake classico su 640×172. Griglia 40×10 (celle 16×16px).
High score salvato in NVS. Controlli: touch tap sinistra/destra per girare,
oppure IMU tilt.

**Fattibilità tecnica:**
- Game loop a 10 FPS (LVGL timer ogni 100ms).
- Griglia 40×10 → 400 celle. Stato: array uint8_t[400] (EMPTY/SNAKE/FOOD).
- Render: fillRectCanvas per ogni cella (nessuna dipendenza LVGL widget).
- Collisione: O(1) con lookup array.
- Food: random con `esp_random()`.
- NVS: `nvs_set_i32("snake", "hiscore", val)`.
- IMU: gyro roll per steer — già letto in `g_imuData`.

**Effort:** BASSO-MEDIO — 1 sessione
**Rischio:** BASSO

---

## 5. System Stats / Diagnostics

**Concept:** Dashboard real-time dello stato del dispositivo. Heap, PSRAM,
WiFi RSSI, uptime, temperatura CPU, batteria, frequenza refresh LVGL.
Tipo "htop" per ScryBar.

**Fattibilità tecnica:**
- `esp_get_free_heap_size()`, `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`
- `WiFi.RSSI()`, `esp_timer_get_time()` per uptime
- Temperatura: `temperatureRead()` (sensore interno ESP32-S3, ±2°C)
- Batteria: già letta in `g_battVoltage`
- LVGL: `lv_disp_get_default()->driver->render_start_cb` timing per FPS
- Tutto locale, zero rete.

**Effort:** BASSISSIMO — mezza sessione
**Rischio:** ZERO

---

## 6. Pomodoro Focus Timer

**Concept:** Timer Pomodoro classico: 25min lavoro → 5min pausa.
Visualizzazione ad arco che si svuota. Touch per start/pause/reset.
IMU shake per skip al prossimo slot.

**Fattibilità tecnica:**
- lv_arc per l'arco progress (widget LVGL nativo).
- Timer: lv_timer ogni 1000ms, decrementa secondi rimanenti.
- Stato: WORK / SHORT_BREAK / LONG_BREAK (ogni 4 pomodori).
- Contatore sessioni in NVS (persistente tra reboot).
- Notifica fine slot: lampeggio display + (opzionale) buzzer.
- Totale codice stimato: ~150 righe.

**Effort:** BASSO — 1 sessione
**Rischio:** BASSO

---

## 7. Partenze Treni — Luino / Lombardia

**Concept:** Prossime 4-5 partenze dalla stazione più vicina (Luino,
linea Domodossola-Milano). Mostra: orario, destinazione, binario, ritardo.

**Fattibilità tecnica:**
- API: Trenord non ha API pubblica ufficiale, ma:
  a) ViaggiaTreno (RFI) — API JSON non documentata ma usata da community
     `http://www.viaggiatreno.it/viaggiatrenonew/resteasy/viaggiatreno/partenze/{stazione_id}/{datetime}`
  b) OpenTransportData (SBB/CH) — copre Luino (confine svizzero)
     `https://transport.opendata.ch/v1/stationboard?station=Luino&limit=5`
     REST JSON, gratuita, documentata.
- OpenTransportData è la scelta migliore: gratuita, documentata, affidabile.
- Stazione "Luino" su rete SBB: codice `8300064`.
- Struttura: array di 5 partenze, refresh ogni 60s.

**Effort:** BASSO-MEDIO — 1 sessione
**Rischio:** BASSO (API svizzera affidabile)

---

## 8. Quote of the Day — "Pensiero del giorno"

**Concept:** Una citazione al giorno, visualizzata in tipografia bella e grande.
Autore + testo. Cambio ogni X ore o ogni swipe (modalità "sfoglia").

**Fattibilità tecnica:**
- ZenQuotes API: `https://zenquotes.io/api/today` — free, no auth.
  Restituisce JSON `[{"q":"testo","a":"autore"}]`.
- Alternativa: Quotable.io `https://api.quotable.io/random` (random).
- Caching: salvare in NVS per avere la citazione offline dopo il primo fetch.
- Layout: testo grande centrato (lvglFontRssNews), autore piccolo sotto.
- Eventuale lingua: aggiungere parametro `lang=it` dove supportato.

**Effort:** BASSISSIMO — mezza sessione
**Rischio:** BASSO

---

## 9. Orologio Hacker — Binary / Hex / UNIX

**Concept:** Alternativa al word clock per nerd. Tre modalità:
- **Binary:** ore e minuti in colonne di bit LED-style
- **HEX:** `0x0E:0x3B:0x1C` (ore, minuti, secondi in hex)
- **UNIX:** timestamp UNIX che scorre in tempo reale

Tap per ciclare tra le tre modalità.

**Fattibilità tecnica:**
- Tutto locale, zero rete.
- Binary: matrice di lv_obj_t cerchi (on/off) — max 4 bit × 6 colonne = 24 oggetti.
- Hex: lv_label con font monospace (Space Mono già disponibile nel progetto).
- UNIX: lv_label aggiornato ogni secondo con `mktime()`.
- Salvataggio modalità preferita in NVS.

**Effort:** BASSISSIMO — mezza sessione
**Rischio:** ZERO

---

## 10. Radio Metadata — "Now Playing"

**Concept:** Mostra titolo + artista della canzone in onda su una radio internet
preferita. Nessun audio (il device non ha DAC) — solo i metadati ICY.

**Fattibilità tecnica:**
- Protocollo ICY metadata: all'interno dello stream HTTP c'è un header
  `Icy-MetaInt` che indica ogni quanti byte arriva un blocco metadata con
  `StreamTitle='Artista - Titolo'`.
- Implementazione: apri connessione HTTP con header `Icy-MetaData: 1`,
  leggi solo i metadata byte, chiudi. Nessun buffering audio.
- Dimensione chunk: tipicamente < 256 byte di metadata ogni ~8KB stream.
  Puoi aprire, leggere il primo chunk metadata, chiudere → ~1-2KB di traffico.
- URL stream configurabile via web config.
- Refresh ogni 15-30 secondi.
- Layout: nome stazione in alto, "♪ Artista — Titolo" al centro grande.

**Note:** Stazioni Shoutcast/Icecast supportano ICY. Spotify/Apple Music no.
Lista stazioni suggerite: Radio Swiss Jazz, RaiPlay Radio 2, SomaFM.

**Effort:** MEDIO — 1 sessione
**Rischio:** MEDIO (parsing ICY stream su ESP32 richiede attenzione ai timeout)

---

## 11. TETRIS

**Concept:** Tetris classico. Il display 640×172 è perfetto: campo di gioco
stretto al centro (10×17 celle da 10px), prossimo pezzo + score sui lati.

**Fattibilità tecnica:**
- Game loop: lv_timer a velocità variabile (livello).
- Pezzi: 7 tetromini standard, rotazione con SRS (Super Rotation System) o
  classica Nintendo.
- Controlli: touch sinistra = move left, touch destra = move right,
  tap centro = rotate, swipe down = hard drop.
- IMU: tilt left/right per spostamento.
- High score in NVS.
- Layout: `[SCORE | campo 100px | NEXT]` tutto nella larghezza 640px.

**Effort:** MEDIO — 1-2 sessioni
**Rischio:** BASSO

---

## Priorità suggerita

| # | View | Effort | Wow factor | Utilità |
|---|------|--------|------------|---------|
| 1 | System Stats | Bassissimo | Basso | Alta |
| 2 | Quote of Day | Bassissimo | Medio | Media |
| 3 | Hacker Clock | Bassissimo | Medio | Media |
| 4 | ANSI / BBS Art | Medio | ALTISSIMO | Estetica |
| 5 | Treni Luino | Basso | Alto | Alta |
| 6 | Crypto Ticker | Basso | Medio | Media |
| 7 | Pomodoro | Basso | Medio | Alta |
| 8 | Snake | Basso-Medio | Alto | Divertimento |
| 9 | Radio Metadata | Medio | Alto | Media |
| 10 | Tetris | Medio | Alto | Divertimento |
| 11 | DOOM | Alto | LEGGENDARIO | Benchmark |

**Raccomandazione sessione prossima:** System Stats + Hacker Clock (mezza giornata,
zero rischio, risultato immediato) — poi ANSI come progetto di una sessione.
DOOM come progetto standalone quando c'è voglia di avventura.
