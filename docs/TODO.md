# ScryBar — Future Views (active shortlist)
# Aggiornato: 2026-03-11

Questo file contiene solo idee ancora vive. Le feature gia' realizzate o scartate
sono state rimosse dal backlog.

Nota: DOOM non e' piu' backlog. E' live sul device.
Per integrazione e gotcha: `knowledge/doom_integration_gotchas.md`.

Ogni view ha: descrizione, fattibilita' tecnica, effort stimato, rischi.

---

## 1. Radio Metadata — "Now Playing"

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
