# ScryBar — idee e direzioni future

**Ricerca:** 17 luglio 2026

**Scopo:** base di discussione per la sessione di lunedì

**Stato:** proposte, non impegni di implementazione

> ⭐️ **Direzione consigliata:** far evolvere ScryBar da “otto schermate da
> sfogliare” a un **ambient desk OS locale e contestuale**. Le viste restano, ma
> il dispositivo porta in primo piano ciò che serve adesso — riunione, focus,
> musica, viaggio, build, allarme — e torna silenziosamente alla home quando il
> momento è passato.

## Executive read

La ricerca suggerisce che ScryBar ha già una base più completa della maggior
parte dei desk display: un'identità visiva forte, fonti dati utili, touch, IMU,
configurazione web e un companion macOS nativo. Il salto di qualità non è
aggiungere altre dieci pagine indipendenti, ma introdurre tre elementi comuni:

1. **un motore di contesto**, che ordina e attiva le viste in base al momento;
2. **un formato universale per card e notifiche**, così nuove integrazioni non
   richiedono sempre una nuova pagina scritta nel firmware;
3. **un canale di azioni sicuro verso il Mac e la casa**, con pairing e comandi
   esplicitamente consentiti.

Lo stesso hardware offre ancora RTC, microSD, doppio microfono, codec audio,
BLE e pin di espansione quasi inutilizzati. Sono opportunità reali, ma audio e
voce devono restare una seconda fase: prima conviene consolidare il modello di
interazione e la sicurezza della LAN.

---

## 1. Cosa permette davvero l'hardware

La scheda è la **Waveshare ESP32-S3-Touch-LCD-3.49**: pannello IPS 640×172,
non AMOLED. La documentazione ufficiale conferma ESP32-S3R8, 8 MB PSRAM,
16 MB flash, touch capacitivo AXS15231B, RTC PCF85063, IMU QMI8658, slot TF,
codec ES8311/ES7210, doppio microfono, BLE 5 e uscita speaker.

| Risorsa | Uso attuale | Opportunità | Priorità |
|---|---|---|---|
| Display IPS 640×172 | 8 viste + DOOM | card contestuali, overlay e mini-app | Alta |
| Touch | swipe e azioni locali | palette di comandi, notifiche con azioni | Alta |
| IMU QMI8658 | shake/tilt e saver | flip-to-mute, lift-to-wake, orientamento | Alta |
| Companion macOS | Now Playing + statistiche | agenda, focus, azioni Mac, AI usage | Alta |
| RTC PCF85063 | sostanzialmente inutilizzato | boot immediato e modalità offline | Alta |
| Wi-Fi/mDNS | dati e config LAN | MQTT, Home Assistant, card push | Alta |
| BLE 5 | inutilizzato | presenza e provisioning di backup | Media |
| microSD/TF | inutilizzata | asset pack, audio, log, cache estesa | Media |
| Audio ES8311/ES7210 | inutilizzato | chime, timer, radio, push-to-talk | Media/bassa |
| Doppio microfono | inutilizzato | note vocali e assistente PTT | Sperimentale |
| Pin I2C/UART | inutilizzati | luce/CO₂/qualità aria e accessori | Sperimentale |

**Nota pratica:** l'uscita audio richiede che la variante/custodia effettivamente
usata abbia uno speaker collegato all'header. Va verificato sul pezzo prima di
impostare una roadmap audio.

---

## 2. Panorama dei progetti affini

### Progetti sullo stesso hardware

| Progetto | Segnale interessante | Cosa imparare | Cautela |
|---|---|---|---|
| [TuneBar](https://github.com/VaAndCob/TuneBar) | radio internet, player SD, meteo/AQI, audio, OTA | l'audio e la microSD sono fattibili sulla scheda | licenza dichiarata in modo non uniforme; pattern, non codice |
| [Candybar / Desk Status Bar](https://github.com/ryanmr/candybar) | RTC, cache meteo, chime, auto-dim, pet reattivo all'IMU | sfrutta quasi tutte le periferiche e ha power mode sensati | nessuna licenza formale chiara nel README; non riusare codice |
| [AI Usage Bar ESP32](https://github.com/captainkie/ai-usage-esp32) | quota Claude/Codex/Gemini, companion pixel, Mac remote | l'AI usage è un bisogno attuale e perfetto per il formato a barra | progetto molto vicino: differenziare con architettura generale |
| [ESP32 PC Monitor Display](https://github.com/Jboling/ESP32-PC-Monitor-Display) | metriche PC via USB seriale o Wi-Fi | il trasporto duale può rendere il companion più robusto | oggi orientato Windows/NVIDIA |
| [PhoneBed](https://github.com/htx-studio/PhoneBed) | sveglia e rituale “telefono a letto” | una funzione comportamentale semplice può valere più di una dashboard | hardware aggiuntivo rilevante |
| [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) | wake word offline, audio streaming, MCP e controllo IoT | prova che ESP32-S3 può essere un terminale vocale/agentico | cloud, privacy e complessità molto maggiori |

### Ecosistemi da cui prendere il modello, non l'estetica

| Ecosistema | Pattern utile per ScryBar |
|---|---|
| [AWTRIX 3](https://github.com/Blueforcer/awtrix3) | mini-app e notifiche inviate via HTTP/MQTT |
| [Tidbyt](https://tidbyt.dev/docs/build/authoring-apps) | app configurabili, cache locale e rotazione programmata |
| [TRMNL](https://help.trmnl.com/en/articles/9510536-private-plugins) | plugin tramite polling, webhook e merge di fonti |
| [LaMetric](https://docs.lametric.com/en/latest/) | indicator app, button app e notifiche con suono/azioni |
| [DeskThing](https://deskthing.app/) | companion desktop + catalogo di app installabili |
| [BUSY Bar](https://busy.app/) | focus visibile, stato call automatico, API locale e smart home |
| [ESPHome/LVGL](https://esphome.io/components/lvgl/) | touch dashboard e integrazione Home Assistant su ESP32 |

### Storia di prodotto che emerge

**Osservato:** i prodotti più longevi separano il dispositivo dalle singole
integrazioni. Il display offre un runtime semplice; computer, webhook o server
producono contenuto configurabile.

**Inferenza per ScryBar:** il vantaggio non sarà avere più feed hard-coded, ma
essere il desk display con la migliore combinazione di **local-first, personalità
visiva e integrazione Mac**.

---

## 3. Matrice delle idee

Scala effort: **S** piccola, **M** media, **L** grande, **XL** programma di lavoro.

Il punteggio considera valore quotidiano, differenziazione e riuso architetturale.

| # | Idea | Valore | Effort | Rischio | Punteggio |
|---:|---|:---:|:---:|:---:|:---:|
| 1 | Context Deck: viste ordinate dal momento | 5 | M | M | **9.4/10** |
| 2 | Agenda + countdown riunione | 5 | M | L | **9.2/10** |
| 3 | Notifiche/action card universali | 5 | M | M | **9.1/10** |
| 4 | Focus/Busy/Pomodoro sincronizzato | 5 | M | L | **8.9/10** |
| 5 | Card Kit / mini-app JSON | 5 | L | M | **8.8/10** |
| 6 | Home Assistant + MQTT Discovery | 4 | L | M | **8.3/10** |
| 7 | Mac Control Deck + Shortcuts | 4 | L | H | **8.0/10** |
| 8 | AI Credits & Agent Pulse | 4 | M | M | **8.0/10** |
| 9 | RTC + offline-first + dati “stale” espliciti | 4 | S/M | L | **7.9/10** |
| 10 | Power modes, quiet hours e motion wake | 4 | M | L | **7.8/10** |
| 11 | Build/CI/PR monitor | 4 | M | M | **7.6/10** |
| 12 | QR/deep-link handoff universale | 3 | S/M | L | **7.5/10** |
| 13 | Ambient pet reattivo | 3 | M | L | **7.2/10** |
| 14 | Audio feedback, sveglie e chime | 3 | M | M | **6.9/10** |
| 15 | microSD asset pack e cache | 3 | M/L | M | **6.8/10** |
| 16 | USB HID macro strip | 4 | L | H | **6.7/10** |
| 17 | Push-to-talk / voice notes / AI | 4 | XL | H | **6.4/10** |
| 18 | BLE presence e away mode | 3 | M | M/H | **6.2/10** |
| 19 | Sensori I2C ambientali | 3 | M + HW | M | **5.9/10** |
| 20 | Mirroring parziale del Mac | 3 | XL | H | **5.5/10** |
| 21 | Multi-ScryBar e scene di stanza | 3 | L | M | **5.4/10** |
| 22 | Matter | 3 | XL | H | **4.8/10** |

---

## 4. Proposte prioritarie, con dettaglio

### 1. Context Deck — la barra sceglie cosa mostrare

**Problema:** con più viste, lo swipe diventa una libreria. Le informazioni
importanti esistono ma non arrivano nel momento giusto.

**Esperienza proposta:** ogni vista ha uno stato `pinned`, `eligible`,
`suppressed` o `urgent`. Il motore locale ricalcola l'ordine, senza interrompere
la persona durante un'interazione.

Esempi:

- 10 minuti prima di un meeting, Agenda diventa la seconda vista;
- durante la musica, Now Playing sale di priorità;
- in orario pendolare, Timetable resta vicino alla home;
- una build fallita produce un overlay, poi resta come badge;
- di notte rimangono clock, timer e meteo essenziale;
- quando il Mac è assente, spariscono Mac Stats e Mac Remote.

**Implementazione:** introdurre un `ViewContext` comune con `priority`, `ttl`,
`freshness`, `attention` e `source`. Il Context Deck decide l'ordine, mentre le
preferenze web stabiliscono quali automazioni sono ammesse.

**Guardrail UX:** mai cambiare pagina sotto il dito; applicare il nuovo ordine
solo a gesto concluso o al ritorno alla home.

**Successo:** meno swipe per raggiungere la vista rilevante e nessuna sorpresa
durante il touch.

### 2. Agenda + Meeting Pulse

**Problema:** la prossima riunione è una delle informazioni più utili da tenere
fuori dal monitor principale.

**Esperienza proposta:** titolo breve, inizio/fine, countdown, colore del
calendario e indicatore del servizio. A T−2 minuti la card può diventare un
overlay discreto. Un tap apre il link sul Mac; un secondo gesto mostra QR e
dettagli.

**Implementazione:** il Companion usa EventKit con consenso esplicito e invia
solo gli eventi futuri necessari. EventKit su macOS supporta accesso e notifiche
di cambiamento del database. Il parsing dei link può coprire Zoom, Meet, Teams,
FaceTime e URL generici.

**Privacy:** titolo e URL non devono finire nei log firmware. Offrire modalità
“solo orario” e filtri per calendario.

**Dipendenze:** permesso Calendario; pairing LAN prima di azioni bidirezionali.

### 3. Notification Rail + Action Cards

**Problema:** manca un modo generico per far comparire eventi effimeri senza
creare una pagina dedicata.

**Esperienza proposta:** una rail superiore o un overlay con quattro priorità:

- `ambient`: badge o ticker, nessuna interruzione;
- `info`: card temporanea, si chiude da sola;
- `action`: uno o due pulsanti touch;
- `urgent`: resta finché viene riconosciuta.

Ogni evento ha `id`, `title`, `body`, `icon`, `color`, `ttl`, `dedupeKey`,
`actions` e `deepLink`. Quiet hours, rate limiting e una inbox degli ultimi
eventi evitano il “notification firehose”.

**API proposta:** `POST /api/v1/notifications`, autenticata. L'azione torna al
Companion o a Home Assistant; il firmware non esegue URL/comandi arbitrari.

**Prime integrazioni:** timer, build terminata, pacco consegnato, lavatrice,
meeting, batteria Mac, reminder scelti dall'utente.

### 4. Focus / Busy / Pomodoro

**Problema:** ScryBar informa, ma non aiuta ancora a proteggere l'attenzione.

**Esperienza proposta:** un gesto lungo avvia Focus. Il display mostra obiettivo,
tempo restante e “free at 16:20”; la barra può diventare leggibile anche da
lontano. Il break ha un linguaggio più leggero e un chime opzionale.

**Automazioni possibili:** presenza di una call, microfono attivo, calendario,
app in primo piano o Shortcut avviato dal Mac. La modalità deve funzionare anche
offline e poter essere sempre disattivata localmente.

**Integrazione Mac:** esporre App Intents del Companion (`Start Focus`, `Show
Message`, `Switch View`, `Set Brightness`) così l'utente può combinarli in
Shortcuts senza dare al firmware accesso generale al computer.

### 5. Card Kit — mini-app senza ricompilare il firmware

**Problema:** ogni nuova fonte oggi costa parsing, stato, UI e release firmware.

**Proposta:** un formato dichiarativo limitato, non HTML/JavaScript. Tipi iniziali:

- `hero_metric` — valore grande + delta;
- `progress` — una o più barre/archi;
- `list` — massimo 4 righe;
- `timeline` — eventi con tempo;
- `status_grid` — 2–4 stati;
- `image_text` — bitmap/cache + testo;
- `chart` — serie breve e campionata.

**Schema comune:** tema, icona Lucide rasterizzata o ID interno, unità, stato di
freschezza, azioni, refresh desiderato e fallback offline.

**Architettura consigliata:** rendering nel firmware, acquisizione dati nel
Companion/Home Assistant. Niente codice di terzi eseguito sull'ESP32.

**Perché è strategico:** permette di aggiungere sport, finanza, GitHub, voli,
spedizioni, server e sensori come pacchetti dati, non come fork del firmware.

### 6. Home Assistant / MQTT

**Esperienza proposta:** ScryBar compare automaticamente come device con entità
per disponibilità, batteria, pagina, tema, luminosità e firmware; espone button
per cambiare vista e accetta notifiche/card.

**Trasporto:** MQTT 5 con Discovery, birth/last-will e payload compatti. In
alternativa REST/WebSocket tramite Companion per utenti senza broker.

**Prime card Home Assistant:** energia domestica, temperatura/CO₂, lavatrice,
porta, meteo locale e allarmi selezionati.

**Scelta di prodotto:** non trasformare ScryBar in un pannello domotico pieno di
pulsanti. Massimo 3–4 entità favorite e azioni deliberate.

### 7. Mac Control Deck

**Esperienza proposta:** palette configurabile di 4–6 azioni:

- play/pause, precedente, successivo e volume;
- apri app/URL;
- esegui uno Shortcut scelto dall'utente;
- mute microfono o output;
- blocca schermo/sleep con conferma;
- avvia Focus o una routine.

**Sicurezza:** il Companion è il broker. Ogni azione ha un ID allowlisted; il
firmware non invia shell command. Azioni sensibili richiedono hold o conferma e
sono disabilitate finché il device non è paired.

**Nota tecnica:** i controlli media system-wide su macOS possono dipendere da
API non pubbliche o automazione specifica per app. Shortcuts/App Intents sono la
via preferibile e più stabile.

### 8. AI Credits & Agent Pulse

**Perché ora:** l'uso di Codex/Claude/Gemini ha finestre e limiti che diventano
informazione operativa. Diversi tool macOS recenti esistono proprio per questo.

**Esperienza proposta:** due gauge (`5h`, `week`), reset, provider e indicatore
di ritmo: “al consumo attuale finisci prima/dopo il reset”. Una seconda card
mostra task agentici attivi, completati o in attesa di approvazione.

**Implementazione:** provider modulare nel Companion. Preferire CLI locali con
output strutturato o file di stato; non copiare token nel firmware. CodexBar
offre già una CLI adatta a script e CI e può essere una integrazione opzionale,
non una dipendenza obbligatoria.

**Differenziazione:** collegare l'usage al Context Deck e alle notifiche, non
creare un firmware mono-funzione come gli altri progetti.

### 9. RTC e offline-first esplicito

**Esperienza proposta:** clock immediato al boot anche senza Wi-Fi; ogni card di
rete conserva l'ultimo dato con timestamp e stile “stale”, invece di diventare
vuota o apparentemente aggiornata.

**Implementazione:** sincronizzare PCF85063 dopo NTP, leggerlo prima della rete,
salvare snapshot piccoli in NVS e più grandi su microSD opzionale. Un unico
modello `Freshness` evita che ogni pagina inventi il proprio fallback.

**Valore:** piccola feature visibile ogni volta che router/API non collaborano;
fondazione per agenda, timer, quiet hours e power modes.

### 10. Power modes e quiet hours

**Proposta:** profili `ACTIVE`, `IDLE`, `NIGHT`, `BATTERY`, `AWAY` con refresh,
Wi-Fi e luminosità distinti. L'IMU può fare lift-to-wake; il Mac può segnalare
lock/sleep/assenza.

**Importante:** la scheda non ha un sensore di luce ambientale integrato. La
prima versione deve usare orari, alimentazione, movimento e stato Mac. Un BH1750
esterno può diventare un accessorio, non un requisito.

### 11. Build, CI e PR monitor

**Esperienza proposta:** una timeline compatta per repository favorito: branch,
build/test in corso, durata, esito e PR review. Un fallimento urgente produce
una notification card; un successo resta ambient.

**Implementazione:** Companion consulta GitHub tramite token nel Keychain o usa
`gh` locale; il device riceve solo uno stato ridotto. Nessuna credenziale GitHub
sul firmware.

**Estensione naturale:** stato di task Codex/Claude, test hardware in corso e
timer di deploy.

### 12. Handoff universale via QR/deep link

Generalizzare il QR già usato dall'RSS:

- meeting → link di join;
- viaggio → percorso/stazione;
- musica → album o artista;
- build → run/PR;
- Wikipedia/news → articolo;
- errore → pagina diagnostica locale.

Il tap breve esegue l'azione sul Mac quando paired; il tap lungo mostra il QR.
Questo rende coerente il passaggio “glance → approfondimento”.

---

## 5. Idee di personalità e sperimentazione

### Ambient pet

Un piccolo personaggio opzionale che reagisce a meteo, focus, musica, tilt,
batteria e limiti AI. Deve vivere ai margini e non rubare spazio ai dati. Una
sola mascotte con stati ben disegnati è meglio di molti sprite incoerenti.

### Earcon, chime e sveglia

Feedback sonoro molto breve per timer, successo/errore e allarme. Volume,
quiet hours e mute fisico obbligatori. È una buona prima prova dell'audio prima
di radio o voce.

### Radio e player microSD

TuneBar dimostra la fattibilità di MP3/AAC/FLAC e stream radio. Per ScryBar lo
terrei come “Audio Pack” opzionale, perché un player completo compete con la
semplicità del prodotto e aggiunge buffering, TLS e controlli complessi.

### Push-to-talk e voice notes

Prima versione deliberatamente **push-to-talk**, non always-listening. Il Mac
può ricevere Opus/PCM, trascrivere e restituire risposta o promemoria. La barra
mostra sempre uno stato evidente del microfono. Wake word e cloud assistant
arrivano solo dopo una review privacy e una scelta esplicita dell'utente.

### microSD content pack

Pacchetti versionati per font, mascotte, suoni, bitmap, stazioni radio e cache.
Manifest con checksum e compatibilità firmware. Non caricare binari eseguibili:
solo asset validati e con limiti di dimensione.

### USB HID macro strip

Il native USB dell'ESP32-S3 può teoricamente affiancare CDC e HID. Potrebbe
diventare una Touch Bar esterna per shortcut e media, ma aumenta il rischio di
input involontari e la complessità del descriptor USB. Va prototipata su branch
separato, con macro allowlisted e un interruttore evidente.

### BLE presence

Usare il telefono/watch come segnale “vicino/lontano” per wake/away. RSSI BLE è
rumoroso: servono isteresi, tempo minimo e fallback. Non usarlo per sicurezza o
per decisioni irreversibili.

### Sensori esterni

Un connettore/accessorio I2C può offrire luce ambientale, temperatura/umidità,
CO₂ o qualità aria. Conviene prima definire un piccolo protocollo di capability,
così il firmware scopre il modulo senza build dedicate.

### Mirroring parziale

Il Mac renderizza una card o una regione 640×172 e invia solo tile cambiate.
Utile per visualizzazioni arbitrarie, ma consuma banda/CPU e indebolisce
accessibilità e consistenza. Moonshot, non fondazione.

### Multi-device e scene

Gruppi `desk`, `door`, `kitchen`: notifiche mirate, temi sincronizzati e stato
busy condiviso. Ha senso solo dopo pairing, identificatori stabili e API v1.

### Matter

Interessante per Apple/Google Home, ma MQTT/Home Assistant offre prima più
valore con meno complessità. Rivalutare dopo aver stabilizzato le capability e
le azioni locali.

---

## 6. Architettura consigliata

### Modello a tre strati

1. **Ambient views** — clock, meteo, RSS, Wikipedia, transit, launch, media,
   Mac stats.
2. **Moment layer** — agenda imminente, timer, alert, build, busy status.
3. **Action layer** — comandi paired verso Companion/Home Assistant.

### Contratti dati minimi

```text
CardPayload
  id, type, title, values[], icon, accent
  freshness, ttl, priority, actions[]

Moment
  id, startsAt, endsAt, urgency, interruptPolicy
  card, dedupeKey, dismissPolicy

Action
  id, label, target, confirmation, expiresAt
```

### API v1 proposta

```text
POST /api/v1/pair
GET  /api/v1/capabilities
GET  /api/v1/state
POST /api/v1/cards/{id}
POST /api/v1/notifications
POST /api/v1/context
POST /api/v1/actions/{id}/result
```

Ogni write richiede pairing. Payload con schema, dimensione e TTL limitati.
Nessuna shell, URL arbitrario o JavaScript sul firmware.

### Companion modulare

Introdurre un protocollo concettuale `ScryBarProvider`:

```text
providerID
capabilities
snapshot()
events()
perform(actionID)
```

Provider iniziali: `NowPlaying`, `MacStats`, `Calendar`, `Focus`, `AIUsage`,
`GitHub`, `Shortcuts`. Ogni provider dichiara permessi e frequenza, così la UI
può mostrare chiaramente cosa legge e perché.

### Sicurezza prima delle azioni

- pairing con codice/QR e token per device;
- token in Keychain sul Mac e NVS cifrata/obfuscated sul device;
- allowlist di action ID;
- replay protection e TTL;
- conferma locale per lock/sleep o azioni domestiche sensibili;
- setup AP protetto e possibilità di revocare tutti i peer;
- TLS verificato dove possibile; nessun downgrade silenzioso.

---

## 7. Sequenza consigliata

### Vertical slice A — “ScryBar sa cosa succede adesso” ⭐️

1. RTC + modello freshness.
2. Agenda via EventKit.
3. Notification Rail con TTL/dedupe.
4. Context Deck minimo: meeting, media, commute, Mac online.
5. Focus timer locale.

Questa sequenza migliora subito l'uso quotidiano e crea i contratti riusabili.

### Vertical slice B — “ScryBar si integra”

1. pairing/auth LAN;
2. Card Kit con 4 layout;
3. Home Assistant/MQTT;
4. App Intents e Shortcuts;
5. AI Usage + GitHub/build provider.

### Vertical slice C — “ScryBar sente e risponde”

1. verifica speaker/microfoni sull'hardware reale;
2. chime/earcon e sveglia;
3. microSD asset/audio pack;
4. push-to-talk verso Companion;
5. solo in seguito wake word o assistente cloud.

---

## 8. Cose da non fare adesso

- Non aggiungere una pagina hard-coded per ogni API di moda.
- Non creare un app store che esegue codice arbitrario sull'ESP32.
- Non leggere tutte le notifiche macOS con API private e fragili.
- Non abilitare comandi Mac generici o shell remota.
- Non attivare un microfono always-on senza indicatore, consenso e privacy model.
- Non usare BLE RSSI come presenza “sicura”.
- Non introdurre Matter prima di avere una API locale stabile.
- Non copiare codice dai progetti affini senza una verifica puntuale delle licenze.

---

## 9. Decision sheet per lunedì

### Decisione 1 — Qual è la promessa principale?

- **A. Ambient intelligence:** mostra la cosa giusta al momento giusto. ⭐️
- B. Mac control surface: una Touch Bar esterna potente.
- C. Open display platform: mini-app per maker e Home Assistant.
- D. AI desk companion: usage, agent status e voce.

Consiglio: **A come promessa**, C come architettura, B/D come pacchetti.

### Decisione 2 — Primo vertical slice

- **Agenda + Focus + Notification Rail** ⭐️
- AI Usage + Agent Pulse
- Home Assistant + Card Kit
- Audio + alarm + radio

Consiglio: il primo. Tocca un bisogno quotidiano e prepara tutto il resto.

### Decisione 3 — Quanto deve essere dinamico il carousel?

- ordine completamente automatico;
- solo pin e suggerimenti;
- automatico con “mai interrompere mentre interagisco”. ⭐️

### Decisione 4 — Audio

- nessun audio;
- earcon/timer soltanto; ⭐️
- radio/player;
- push-to-talk/assistant.

### Decisione 5 — Ecosistema

- solo firmware ufficiale;
- provider nel Companion;
- Card Kit + API pubblica; ⭐️
- marketplace/community app completo.

---

## 10. Fonti e confidenza

### Fonti primarie / confidenza alta

- [Waveshare — pagina prodotto](https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm)
- [Waveshare — wiki e demo hardware](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49)
- [Apple EventKit](https://developer.apple.com/documentation/eventkit)
- [Apple App Intents](https://developer.apple.com/documentation/appintents)
- [Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt)
- [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/)
- [Home Assistant WebSocket API](https://developers.home-assistant.io/docs/api/websocket/)
- [ESPHome LVGL](https://esphome.io/components/lvgl/)

### Implementazioni pubbliche / confidenza media

- [TuneBar](https://github.com/VaAndCob/TuneBar)
- [Candybar](https://github.com/ryanmr/candybar)
- [AI Usage Bar ESP32](https://github.com/captainkie/ai-usage-esp32)
- [ESP32 PC Monitor Display](https://github.com/Jboling/ESP32-PC-Monitor-Display)
- [PhoneBed](https://github.com/htx-studio/PhoneBed)
- [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32)
- [DeskThing](https://github.com/ItsRiprod/DeskThing)
- [CodexBar](https://github.com/steipete/CodexBar)

### Pattern di ecosistema / confidenza alta sul modello, non sulla domanda

- [Tidbyt developer docs](https://tidbyt.dev/docs/build/authoring-apps)
- [TRMNL private plugins](https://help.trmnl.com/en/articles/9510536-private-plugins)
- [LaMetric developer docs](https://docs.lametric.com/en/latest/)
- [BUSY Bar](https://busy.app/)
- [AWTRIX 3](https://github.com/Blueforcer/awtrix3)

Le idee qui sopra combinano fatti hardware, pattern osservati e inferenze di
prodotto. I punteggi non rappresentano domanda di mercato misurata: servono a
rendere concreta la conversazione di lunedì.
