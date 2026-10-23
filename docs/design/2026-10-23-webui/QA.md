# WebUI — QA del 23 ottobre 2026

La verifica comprende build esterni a Dropbox, browser reali in modalità headless, catture del browser e controlli dei contratti dei form. I conteggi qui sotto si riferiscono ai casi automatizzati dichiarati; non sono una certificazione di accessibilità completa né una prova su iPhone fisico.

## Risultati riproducibili

| Prova | Ambiente e copertura | Risultato | Evidenza |
| --- | --- | --- | --- |
| Landing ROOT | Chromium, 320/375/390/430/768/1440 px |107/107 | [JSON](evidence/landing-chromium.json) |
| Landing WebKit | WebKit,320/390 px |84/84 | [JSON](evidence/landing-webkit.json) |
| Landing Pages base | Chromium, 390 px, URL `/scrybar/` |81/81 | [JSON](evidence/landing-pages-base.json) |
| Control canonical r322 | Chromium, sei larghezze×sette temi×tre sezioni e interazioni |183/183 | [JSON](evidence/control-r322-chromium.json) |
| Control canonical r322 WebKit |320/390 px, sette temi×tre sezioni e interazioni |79/79 | [JSON](evidence/control-r322-webkit.json) |
| Stato luminosità r322 | JS effettivo della `.ino`, metadata RAM/NVS dichiarati di fixture |9/9 Chromium +9/9 WebKit | [Chromium](evidence/brightness-metadata-chromium.json), [WebKit](evidence/brightness-metadata-webkit.json) |
| Taste Control | Inspector app, 1440/768/390 px |0 FAIL, 0 WARN | [JSON](evidence/control-r322-taste.json) |
| Taste landing | Inspector pagina, 1440/768/390 px |0 FAIL con due override espliciti; 4 WARN | [JSON](evidence/landing-taste.json) |

Gli script versionati sono `web/tools/browser-qa.mjs`, `control-panel-qa.mjs` e `brightness-state-qa.mjs`. Richiedono un runtime Playwright già presente. I due script Control rifiutano un host diverso dal loopback: i test non possono applicare impostazioni al dispositivo per errore.

La gallery è stata esercitata nelle 63 combinazioni tema/vista, verificando immagine decodificata 640×172, una sola immagine attiva e una sola scelta tema premuta. La coppia Home/Stats usa le proprie risorse per ciascun tema. Tutte le immagini della pagina vengono decodificate prima delle catture complete: il lazy loading non lascia aree bianche nelle catture di pagina.

## Interazioni controllate

- Menu mobile da tastiera; ArrowDown entra nei link anche quando la preferenza Safari esclude i link dal Tab; Escape chiude e ripristina il focus.
- Dialog della gallery: apertura, immagini e note aggiornate, avanzamento, Escape, ritorno al pulsante di partenza; su telefono l’immagine resta esplorabile a dimensione utile.
- Installer: caricamento, browser non supportato, contesto non sicuro, errore del loader e retry reale. Successo/errore usano un loader di fixture; nessuna porta seriale richiesta.
- Form: dirty da tema, range, checkbox, ordine e lista RSS; Save disabilitato quando pulito e normale submit senza JavaScript.
- Reload: POST a `/reload` con body vuoto; risposta accettata dichiarata “requested”; valori, ordine e perfino una bozza RSS invalida restano nella pagina. Il fallimento conserva i valori e non dichiara completamento.
- RSS: massimo cinque feed, aggiunta/edit/rimozione, conteggio visibile, guardia sulla bozza non aggiunta, serializzazione dei cinque slot al submit.
- Meteo e partenze: risposte di fixture, coordinate negative immesse manualmente, fermata selezionata e ID mantenuti. Le chiamate ai servizi reali non sono necessarie ai test.
- Ultima azione RSS raggiungibile sopra Save, diagnostica espandibile, nessun overflow dei controlli ai viewport verificati, font bloccati e reduced motion.

## Rilievi trovati e corretti

| Rilievo | Correzione |
| --- | --- |
| Colonna titoli del Control troppo larga | Griglia desktop circa 28%/72% |
| Save disponibile anche senza cambiamenti | Fingerprint dei campi e stato pulito/dirty |
| Reload poteva applicare/scartare scelte tramite navigazione | Fetch allo stesso endpoint con body vuoto, nessun cambio documento |
| Nuova struttura RSS perdeva il badge del conteggio | Nodo originale preservato e aggiornato dagli handler esistenti |
| Retry del modulo installer riusava un URL fallito in cache | URL distinto per ciascun tentativo successivo |
| Tab sui link non uniforme in WebKit/macOS | Accesso esplicito via ArrowDown, senza dipendere dalla preferenza sistema |
| Screenshot completo scattato prima del lazy loading | Decodifica esplicita di tutte le immagini nella procedura di cattura |
| URL manifest errato nel build Pages (`/scrybarmanifest.json`) | Separatore BASE_URL normalizzato; fetch reale di `/scrybar/manifest.json` aggiunto ai test, oltre al loader di fixture |
| Riapertura dopo preview luminosità confondeva RAM e NVS | Due metadata con valori realmente salvati; baseline iniziale basata su quelli, non sui range RAM |

L’ultimo caso è verificato con quattro stati espliciti: preview 81 / stored 100→dirty; ritorno 100→clean; stored 81 / RAM 81→clean; persistenza fallita o parziale con metadata precedenti→dirty. I metadata non vengono avanzati dalla preview e GET non scrive NVS. L’effettiva prova hardware di save/riapertura/ripristino è distinta da questi fixture ed è coordinata nel report firmware.

## Contrasto e taste gates

I dodici token colore/type di ciascun tema del Control sono serializzati dalla tabella firmware, senza una seconda palette. I sei abbinamenti testo principale/secondario sui tre fondi usati hanno rapporto minimo 5.69:1; [misure](evidence/control-token-contrast.json). Focus visibile e testo sono preservati anche nei temi chiari. I pulsanti principali misurano almeno 44 px; 24 px è il minimo WCAG 2.2 AA, mentre 44 px è l’obiettivo ergonomico del progetto, non una dichiarazione AA aggiuntiva.

L’inspector landing è stato eseguito anche senza override: l’unico FAIL era “Controller” dentro le specifiche in un `<details>` chiuso, associato dal probe al grande wordmark nel footer. Il nodo ha dimensioni 0×0 e non è visibile: il contenuto non è stato alterato per soddisfare questo falso positivo. L’override `eyebrow` documenta esclusivamente tale rilievo; `palette-purple` conserva il violetto di marca.

I quattro WARN restanti sono registrati, non nascosti: numero di dimensioni tipografiche comprendente il wordmark; assenza di video; differenza di 16 px nella stima del margine da parte del probe; nav e contenuto condividono lo stesso contenitore centrato; movimento limitato a preview e swatch. Si mantengono immagini autentiche, lettura stabile sul telefono e reduced motion; non si aggiungono footage estranei o animazioni decorative soltanto per il contatore.

## Build, privacy e limiti

Entrambi i build ROOT e `PAGES_BASE=/scrybar` sono riusciti in una copia esterna. Dopo la correzione del manifest, 17 risorse su 17 rispondono 200 per ciascun build; il manifest reale risponde 200 e la versione visibile coincide con il JSON: [controllo percorsi](evidence/build-path-check.json). I precedenti 80 test Pages non sono il risultato finale: la suite aggiornata ne esegue 81, incluso il fetch reale. Il lockfile è invariato, SHA-256 `1953976071112c43a9682fc32a315af4baa29fe6e8a14293be4686987eb57632`. `web/node_modules` era già ignorata da Git/non tracciata e File Provider; la cache `.astro` preesistente di settembre, anch’essa ignorata/non tracciata, è stata marcata File Provider durante l’audit. Nessuna build di questo lavoro l’ha popolata. Tale cache poteva già essere nel cloud prima dell’esclusione e sugli altri Mac va rigenerata.

Le catture Control e i report versionati usano solo fixture redatte: SSID, MAC e IP personali non sono inclusi. L’HTML originale e l’origine del dispositivo restano nel privato fuori repo; [provenienza finale r322](evidence/control-r322-fixture-provenance.json); la provenienza r321 rimane storica. Il vecchio Diagnostics non è emesso dal build, mentre lo sono il nuovo esempio e tutte le sette Stats: [controllo asset](evidence/public-assets-check.json).

Non sono stati eseguiti da questa QA: provisioning reale Wi-Fi/AP, installazione firmware tramite Web Serial, test con tastiera fisica o browser di un telefono, pubblicazione GitHub Pages, verifica di ogni tecnologia assistiva. La persistenza e il funzionamento sul dispositivo vengono registrati separatamente dal coordinatore; i mock non li sostituiscono.

## Fonti primarie usate

- [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum/) e [Target Size Enhanced](https://www.w3.org/WAI/WCAG22/Understanding/target-size-enhanced/): 24 px minimo e 44 px obiettivo touch.
- [WAI-ARIA accordion pattern](https://www.w3.org/WAI/ARIA/apg/patterns/accordion/): pulsanti reali, stato expanded, controllo da tastiera.
- [MDN prefers-reduced-motion](https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/At-rules/%40media/prefers-reduced-motion): alternativa senza movimento non essenziale.
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/): supporto del browser e flusso di installazione. Pin del loader 10.2.1 conservato; nessuna migrazione implicita dell’installer.
- [Waveshare ESP32-S3-Touch-LCD-3.49](https://docs.waveshare.com/ESP32-S3-Touch-LCD-3.49): modello esatto, display IPS e differenze della revisione V2.
- [Asset ufficiali Companion 0.3.14](https://github.com/enuzzo/scrybar/releases/expanded_assets/companion-v0.3.14): download pubblico Preview, distinto dal firmware stabile.


## Checkpoint della release v0.9.5 prima della correzione del reader

Dopo l’autorizzazione alla pubblicazione, manifest e copy sono stati aggiornati a firmware 0.9.5/r322 e Companion 0.3.15 (51). Le precedenti prove restano il record del redesign; entrambi i build esterni sono stati ricompilati e la suite è stata ripetuta sui metadati finali:

| Prova release | Risultato | Evidenza |
| --- | --- | --- |
| ROOT Chromium, sei viewport |107/107 | [JSON](evidence/release-0.9.5-root.json) |
| ROOT WebKit, 320/390 px |84/84 | [JSON](evidence/release-0.9.5-webkit.json) |
| Pages base, manifest reale 0.9.5 |81/81 | [JSON](evidence/release-0.9.5-pages.json) |
| Versioni/link/Preview, release note aperte a 390/1440 px, entrambe le basi |24/24 | [JSON](evidence/release-0.9.5-copy.json) |
| Asset same-origin e manifest |17/17 per base, HTTP 200 | [JSON](evidence/release-0.9.5-paths.json) |
| Immutabilità | Parti/offset del manifest, lockfile e `.ino` r322 invariati | [JSON](evidence/release-0.9.5-immutability.json) |

Le catture [Companion mobile](screenshots/release-0.9.5-companion-390.png), [Companion desktop](screenshots/release-0.9.5-companion-1440.png) e [release note mobile](screenshots/release-0.9.5-notes-390.png) sono state ispezionate. Il DMG è dichiarato Preview/ad hoc/non notarizzato. L’URL nel markup è verificato; **il download reale resta in attesa della pubblicazione e non è conteggiato come superato**. Nessun push, upload o deploy è stato eseguito dall’agente frontend.


## Checkpoint copy Companion 0.3.15 (52) e formato versione CI

La build 52 sostituisce il checkpoint precedente. Sito, README e changelog descrivono il reader MediaRemote nativo, il controllo coerente di sorgente/brano/album e l’HD opzionale: almeno 512 px reali e corrispondenza della copertina. Non si dichiara HD un’immagine piccola ingrandita. La sorgente osservata durante le prove native ha fornito 150 × 150 px anche dopo una richiesta più grande; non è presentato come limite universale del player o di MediaRemote.

I quattro build esterni combinano base ROOT/Pages e versione manifest locale `0.9.5`/CI `v0.9.5`. Il prefisso `v` viene mantenuto identico nel JSON scaricato realmente e nel testo generato della pagina. Nessun cambio al manifest della repo è stato necessario per simulare la versione CI.

| Prova finale build 52 | Risultato | Evidenza |
| --- | --- | --- |
| ROOT Chromium, sei viewport |107/107 | [JSON](evidence/release-build52-root.json) |
| ROOT WebKit, 320/390 px |84/84 | [JSON](evidence/release-build52-webkit.json) |
| Pages CI `v0.9.5`, 390 px |81/81 | [JSON](evidence/release-build52-pages-ci.json) |
| Copy, link, Preview/ad hoc, disclosure artwork e release aperta; 390/1440 px nelle quattro combinazioni |64/64 | [JSON](evidence/release-build52-copy.json) |
| Risorse e manifest reali nelle quattro combinazioni |17/17 HTTP 200 per combinazione; versione esatta | [JSON](evidence/release-build52-paths.json) |
| Immutabilità | `.ino` r322, lockfile e parti/offset manifest invariati | [JSON](evidence/release-build52-immutability.json) |

Le catture curate [Companion mobile](screenshots/release-build52-companion-390.png), [Companion desktop](screenshots/release-build52-companion-1440.png) e [release note mobile](screenshots/release-build52-release-notes-390.png) sono state ispezionate dopo la revisione. Il nuovo testo si legge senza overflow, la disclosure spiega la qualità disponibile e i numeri delle prove producer restano nel record di release, distinti dall’accettazione fisica.

Il link DMG punta a Companion 0.3.15 (52) ed è verificato come markup. **Download reale e QA del sito pubblico restano in attesa della pubblicazione CI** e non entrano nei conteggi locali sopra. Nessun push, upload o deploy è stato eseguito dall’agente frontend.


## Freeze finale Companion 0.3.15 (53)

La build finale diventa 53 per una correzione del ciclo di vita del reader. I riferimenti nel sito, README e changelog sono allineati; direzione visiva e funzioni web restano quelle già verificate. La suite completa del checkpoint 52 sopra resta storica e non viene conteggiata nuovamente come prova eseguita su 53. L’owner Companion registra separatamente la suite finale di 92 test Swift, i controlli nativi e l’installazione.

Sono stati ricompilati soltanto ROOT con manifest locale `0.9.5` e Pages con il formato CI esatto `v0.9.5`. Il controllo mirato a 390/1440 px ha verificato versione 0.3.15 (53), link DMG, etichetta Preview/ad hoc/non notarizzato, testo HD condizionale, disclosure e assenza di overflow nelle release aperte: **32/32**, [evidenza](evidence/release-build53-copy.json). Asset e manifest reali rispondono **17/17 HTTP 200 per base**, con versione visibile esattamente coincidente con il JSON: [evidenza](evidence/release-build53-paths.json).

La [cattura mobile finale](screenshots/release-build53-companion-390.png) è stata ispezionata. Il download reale del DMG e il browser QA sul sito pubblico restano in attesa della pubblicazione CI; non sono inclusi nei risultati locali. Nessun push o deploy eseguito dall’agente frontend.
