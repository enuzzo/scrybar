# ScryBar — Future Views (active shortlist)
# Aggiornato: 2026-03-10

Questo file contiene solo idee ancora vive. Le feature gia' realizzate o scartate
sono state rimosse dal backlog.

Ogni view ha: descrizione, fattibilita' tecnica, effort stimato, rischi.

---

## 1. DOOM — "ci gira Doom?"

**Concept:** Prima view interattiva/game. Doom shareware sul display,
controlli: IMU gyroscope per girarsi, touch per sparare/usare/avanzare.

**Fattibilita' tecnica:**
- CPU ESP32-S3 @ 240MHz e 8MB OPI PSRAM sono sufficienti per un port serio.
- Esistono gia' port ESP32 da studiare e adattare.
- Display 640x172 ultra-wide: opzioni sane sono render a 320x172 con pillar-box
  oppure crop verticale centrato da 320x200.
- Audio assente in v1: muto, nessun problema.
- Il vero nodo e' la pipeline video: Doom deve scrivere in modo piu' diretto
  del normale loop LVGL.

**Controlli candidati:**
- Gyro asse Z: rotazione sinistra/destra
- Tilt avanti/indietro: move forward/backward
- Touch sinistro: use
- Touch destro: fire

**Base tecnica consigliata (ricognizione 2026-03-10):**
- Donor principale: `ducalex/retro-go`, prendendo `prboom-go` come base del motore.
- Non conviene portarsi dietro tutto `retro-go`: per ScryBar serve solo il core DOOM
  piu' il glue locale.
- Nodo critico: `retro-go` prevede driver display nativi soprattutto per
  `ILI9341/ST7789`; qui va scritto il glue per il nostro display `AXS15231B`
  / pipeline `esp_lcd`.
- Fallback utile solo per spike: `espressif/esp32-doom`, ma come proof-of-concept,
  non come base finale.

**Come iniziare davvero:**
1. Ispezionare `prboom-go` e il suo layer platform / video.
2. Ottenere il primo frame sul display ScryBar, anche senza input.
3. Solo dopo agganciare controlli `gyro + touch`.

**Effort:** ALTO
**Rischio:** MEDIO-ALTO

---

## 2. Radio Metadata — "Now Playing"

**Concept:** Mostra artista + titolo della traccia in onda su una radio internet,
senza riprodurre l'audio. E' una view "what's playing", non un player.

**Fattibilita' tecnica:**
- Molte radio Shoutcast/Icecast espongono metadata ICY dentro lo stream HTTP.
- Basta aprire la connessione con `Icy-MetaData: 1`, leggere il primo blocco
  metadata, estrarre `StreamTitle='Artista - Titolo'`, chiudere.
- Traffico minimo: nessun buffering audio, solo pochi KB.
- URL stream configurabile via web config.
- Refresh ogni 15-30 secondi.

**Note pratiche:**
- Funziona con radio internet vere, tipo SomaFM o Radio Swiss Jazz.
- Non funziona con Spotify/Apple Music.

**Effort:** MEDIO
**Rischio:** MEDIO

---

## 3. Orologio Hacker — Binary / Hex / UNIX

**Concept:** Alternativa nerd al word clock. Tre modalita':
- **Binary:** ore e minuti in colonne di bit
- **HEX:** `0x0E:0x3B:0x1C`
- **UNIX:** timestamp che scorre in tempo reale

Tap per ciclare tra le modalita'.

**Fattibilita' tecnica:**
- Tutto locale, zero rete.
- Binary: piccola matrice di oggetti on/off.
- Hex: label monospace, gia' perfetta col set font esistente.
- UNIX: label aggiornata ogni secondo.
- Stato preferito salvabile in NVS.

**Effort:** BASSISSIMO
**Rischio:** ZERO

---

## Priorita' suggerita

| # | View | Effort | Wow factor | Utilita' |
|---|------|--------|------------|----------|
| 1 | DOOM | Alto | LEGGENDARIO | Benchmark |
| 2 | Radio Metadata | Medio | Alto | Media |
| 3 | Hacker Clock | Bassissimo | Medio | Media |

**Raccomandazione attuale:** si parte da DOOM. Hacker Clock resta il side quest
facile, Radio Metadata e' la view elegante e strana che vale la pena fare dopo.
