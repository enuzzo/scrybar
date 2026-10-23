# Pagine native LVGL — verifica del 23 ottobre 2026

Il renderer [render_display_quality_native.py](../../../tools/render_display_quality_native.py) estrae dal firmware le funzioni che creano i widget, applicano temi e font e aggiornano le pagine. Compila con LVGL 8 e i veri font Dosis installati nel progetto. Non ricostruisce il layout in HTML/CSS e non accede a USB, WiFi, NVS o credenziali.

Le immagini sono render software dei sorgenti con dati anonimi. Non attestano luminosità, calibrazione, leggibilità sul pannello fisico, framerate o comportamento dei feed reali. DOOM usa il vero renderer TITLEPIC/HUD di fallback: non è una sessione del motore PrBoom.

## Matrice delle dieci pagine

Sono stati generati e ispezionati i 70 render normali: dieci pagine per sette temi. Le immagini sono 640×172; i fogli sottostanti mostrano ogni tema alla risoluzione nativa. I nomi dei temi corrispondono ai sorgenti: ScryBar Default, Cyberpunk 2077, Toxic Candy, Tokyo Transit, Minimal Brutalist Mono, Mint Protocol e Cathode Ray.

| Pagina | Provenienza e dati della prova | Foglio dei sette temi |
|---|---|---|
| HOME | Inizializzatori reali di orologio e meteo; frase, data e valori meteo sintetici; icone Meteocons incorporate reali | [HOME](../../../screenshots/display-quality-r321/native/home--contact.png) |
| INFO | Pannello reale; testo hardware sostituito con dati di documentazione; QR `https://example.invalid` | [INFO](../../../screenshots/display-quality-r321/native/info--contact.png) |
| RSS | Inizializzatore e updater reali; articolo anonimo su `example.org`; badge senza favicon | [RSS](../../../screenshots/display-quality-r321/native/rss--contact.png) |
| WIKI | Deck reale; categoria Random, articolo di esempio, nessuna favicon | [WIKI](../../../screenshots/display-quality-r321/native/wiki--contact.png) |
| NOW PLAYING | Inizializzatore/updater reali; traccia anonima, copertina di test realmente incorporata; superficie cromatica deterministica | [NOW PLAYING](../../../screenshots/display-quality-r321/native/nowplaying--contact.png) |
| DOOM | Funzioni reali di palette, TITLEPIC e HUD; IMU neutrale sintetica; grafica indipendente dal tema | [DOOM](../../../screenshots/display-quality-r321/native/doom--contact.png) |
| TRANSIT | Updater reale; quattro partenze sintetiche: imminente, 2 minuti, ritardo, colore API quasi bianco | [TRANSIT](../../../screenshots/display-quality-r321/native/transit--contact.png) |
| LAUNCH | Updater e countdown reali; tre missioni sintetiche e orologio deterministico | [LAUNCH](../../../screenshots/display-quality-r321/native/launch--contact.png) |
| MAC STATS | Widget e updater reali; telemetria e storico sintetici | [MAC STATS](../../../screenshots/display-quality-r321/native/macstats--contact.png) |
| BAMBU | Widget e updater reali; stampa anonima al 63%; camera esclusa dalla prova | [BAMBU](../../../screenshots/display-quality-r321/native/bambu--contact.png) |

Il [manifest normale](../../../screenshots/display-quality-r321/native/manifest.json) contiene hash del sorgente letto, inventario delle funzioni estratte e bounds delle label. Le esecuzioni più recenti registrano anche hash della translation unit estratta e della tabella UiStrings. Questi identificano l'evidenza anche se nel frattempo cambia la parte Web dello stesso `.ino`.

La rigenerazione finale dopo il freeze r321 ha SHA-256 sorgente `76cd1d728e86ba8a78807e9a77fb9eee8705cc028fe50b4365a28219c3fc84de` in tutte le gallerie. I render delle pagine mutate sono stati ricontrollati; [inspection-delta.json](../../../screenshots/display-quality-r321/native/inspection-delta.json) registra le differenze rispetto ai render già ispezionati. I campioni di superficie e le opacità reali dei widget sono in [surface-samples.json](../../../screenshots/display-quality-r321/surface-samples.json).

## Difetti individuati e ricontrollati

| Difetto | Evidenza iniziale | Esito nel candidato |
|---|---|---|
| Launch: titolo lungo sopra titolo pagina e ora | Il testo centrale occupava x43–597; `LAUNCHES` occupava x8–95 e l'ora iniziava a x595 | Il contenitore centrale ora è x120–519 con ellissi; non interseca le due etichette laterali |
| Transit: testo scuro sul badge di fallback | `routeColor=0xF2F5F5` veniva scartato per lo sfondo ma riutilizzato nella scelta del testo | Il testo deriva dallo sfondo effettivamente utilizzato; `IC 3` resta leggibile nei sette temi |
| Launch: TBD e segnali di decollo | Il font numerico non copriva tutte le lettere; gli accenti chiari erano deboli su Mint | TBD visibile nel font testuale; LIFTOFF e ultimo minuto leggibili nella prova Mint e nei temi scuri |

La galleria [stress](../../../screenshots/display-quality-r321/stress/manifest.json) include cinque stati Launch per sette temi: titolo lungo, TBD, LIFTOFF, ultimo minuto e lista compatta. Sono stati ispezionati tutti i 35 render Launch. Esempio: [cinque stati Mint](../../../screenshots/display-quality-r321/stress/launch--mint-protocol--contact.png).

La stessa esecuzione include Transit: il caso `long` usa una stazione di 45 caratteri; gli altri quattro nomi di stato sono controlli ripetuti, non ulteriori stati Transit. Il conteggio totale di 70 immagini di questa galleria non equivale a 70 condizioni distinte.

La galleria [Stats](../../../screenshots/display-quality-r321/stats-states/manifest.json) aggiunge 28 render: warning, critical, stale e waiting per sette temi. La prova Mint è stata ispezionata direttamente: [quattro stati](../../../screenshots/display-quality-r321/stats-states/macstats--mint-protocol--contact.png). I valori warning usano CPU 65%/75°C e GPU 70%/80°C; quelli critical 85%/90°C e 92%/95°C. Sono fixture, non misurazioni del Mac.

## QR e lingue

La galleria [QR](../../../screenshots/display-quality-r321/qr/manifest.json) contiene 196 render: WIKI e LAUNCH, quattordici lingue, sette temi. Sono state ispezionate direttamente le quattordici lingue di entrambe le pagine su Mint; le verifiche native di glifi, formati e bounds per tutte le lingue sono riportate separatamente in [candidate-expanded.md](candidate-expanded.md).

Esempi dei quattordici testi: [WIKI su Mint](../../../screenshots/display-quality-r321/qr/wiki--mint-protocol--contact.png), [LAUNCH su Mint](../../../screenshots/display-quality-r321/qr/launch--mint-protocol--contact.png). La generazione del QR è quella LVGL del firmware; questo controllo non certifica la scansione con una fotocamera fisica.

INFO pubblico è una fixture esplicita: SSID `ExampleNetwork`, MAC `02:00:00:00:00:24`, IP riservato alla documentazione `192.0.2.24`, etichetta firmware `Example`, QR `https://example.invalid`. [PNG normale](../../../screenshots/display-quality-r321/native/scrybar-default--info--normal.png). Nessuno di questi valori proviene dal dispositivo.

## Riproduzione e limiti del modello

```sh
python3 tools/render_display_quality_native.py \
  --out-dir screenshots/display-quality-r321/native --states normal

python3 tools/render_display_quality_native.py \
  --out-dir screenshots/display-quality-r321/stress --pages LAUNCH TRANSIT \
  --states long tbd liftoff final-minute compact

python3 tools/render_display_quality_native.py \
  --out-dir screenshots/display-quality-r321/stats-states --pages MACSTATS \
  --states warning critical stale waiting

python3 tools/render_display_quality_native.py \
  --out-dir screenshots/display-quality-r321/qr --pages WIKI LAUNCH \
  --states qr-it qr-en qr-fr qr-de qr-es qr-pt qr-la qr-eo \
           qr-l33t qr-sha qr-val qr-bellazio qr-pir qr-tlh

python3 tests/render_display_quality_native_test.py
```

Binari, oggetti, log CMake e PPM restano nella cartella temporanea esterna a Dropbox. `--build-dir` rifiuta una directory dentro Dropbox. PNG e manifest restano nel repository come output rigenerabile; l'attuale regola Git `/screenshots/` li ignora. Sorgenti del renderer, fixture, test e questo documento permettono di rigenerarli.

La baseline anonima dei 70 render normali r320 è conservata in `screenshots/display-quality-r321/baseline-r320/`; deriva dal sorgente privato della build r320, non da una ricostruzione a memoria. Nessun file `secrets.h` viene letto, analizzato o incluso.

Ogni processo crea una sola pagina isolata. Prima del tema risolve la geometria, come avviene nella sequenza completa del firmware dopo gli altri inizializzatori: senza quel passaggio il test INFO ricreava erroneamente un QR da 64px. Questo errore del primo harness è stato corretto prima dell'asset pubblico, che mostra il QR da 128px.

Queste prove non verificano tutte le interazioni fra pagine contemporaneamente allocate, i lifecycle delle modali, la memoria sul microcontrollore, le transizioni fisiche, il gioco DOOM attivo o le copertine ricevute in rete. Le prove del compositor e la regressione hardware del firmware restano verifiche separate. Non viene dichiarata una corrispondenza perfetta fra questi 70 render e 70 fotografie del display.

L'orologio effettivo è stato verificato separatamente: [910 campioni nelle quattordici lingue](wordclock-review.md), con le funzioni e l'auto-fit reali del firmware.
