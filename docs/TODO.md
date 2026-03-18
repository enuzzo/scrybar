# ScryBar — Future Views (active shortlist)
# Aggiornato: 2026-03-18

Questo file contiene solo idee ancora vive. Le feature gia' realizzate o scartate
sono state rimosse dal backlog.

Nota: DOOM non e' piu' backlog. E' live sul device.
Per integrazione e gotcha: `knowledge/doom_integration_gotchas.md`.

Ogni view ha: descrizione, fattibilita' tecnica, effort stimato, rischi.

---

## 1. Radio Metadata — "Now Playing"

**Concept:** Mostra artista + titolo della traccia in onda o in riproduzione,
senza riprodurre l'audio. E' una view "what's playing", non un player.

**Direzione aggiornata:**
- `assets/spainify/` resta solo reference visiva per cover art / gerarchia testo / mood.
- L'implementazione ScryBar deve essere piu' universale e basarsi su una companion app macOS.
- Il firmware ha gia' un prototipo visuale live sul device:
  - cover art quadrata full-height;
  - colore di sfondo derivato dalla cover;
  - gerarchia tipografica in stile player;
  - progress bar e controlli `back / pause / forward`;
  - dati ancora fake, utili per iterare il layout sulla barra vera.
- La companion deve:
  - trovare automaticamente la ScryBar in LAN tramite identificazione broadcast/annuncio device;
  - permettere fallback manuale con IP o hostname se l'auto-discovery fallisce o tarda;
  - normalizzare sorgenti diverse (`Music`, `Spotify`, `TIDAL`, podcast, altre app del Mac) in un payload unico per il firmware.
- Stato companion (scaffold iniziale):
  - esiste ora una app macOS Swift in `companion/mac/ScryBarCompanion/`;
  - include discovery `_scrybar._tcp`, fallback manuale `host/IP`, payload preview JSON e provider `System` + `Mock` + `Music.app`;
  - `System` usa `MediaRemote.framework` per leggere il now playing globale che macOS mostra gia' in Control Center / menu bar;
  - il firmware ora espone `GET/POST /api/now-playing` su porta `8080`;
  - la ScryBar ora si annuncia in LAN via Bonjour/mDNS come servizio `_scrybar._tcp`;
  - resta da collegare provider reali aggiuntivi e artwork live dalla companion.

**Piano prossima sessione:**
- portare la cover vera dalla companion al firmware, non solo metadati;
- usare `MediaRemote` anche per `back / pause / next`;
- migliorare il naming della sorgente quando il client di sistema non espone bene `displayName` o `bundleIdentifier`;
- ridurre il numero di fallback app-specifici al minimo necessario.

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
- La companion Mac e' la via preferita per supportare sorgenti "app-driven" invece di stream ICY puri.

**Effort:** MEDIO
**Rischio:** MEDIO

---

## 2. Orologio Hacker — Binary / Hex / UNIX

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
| 1 | Radio Metadata | Medio | Alto | Media |
| 2 | Hacker Clock | Bassissimo | Medio | Media |

**Raccomandazione attuale:** Radio Metadata e' la view elegante e strana che vale
la pena fare dopo. Hacker Clock resta il side quest facile.
