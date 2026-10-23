# ScryBar WebUI — redesign e verifica

Il lavoro riguarda le due interfacce web effettivamente usate: la pagina pubblica Astro e il Control Surface generato dal firmware. Il pannello conserva route, nomi dei campi e API esistenti; la landing usa le catture reali del prodotto e gli esempi nativi dichiarati. Companion rimane un’app SwiftUI distinta.

| Superficie | Prima | Risultato |
| --- | --- | --- |
| Landing | Introduzione lunga, CTA duplicate, informazioni avanzate molto presenti, immagini Stats obsolete | Dimostrazione azionabile di nove viste e sette temi, quattro famiglie di funzioni, setup breve, Companion e installer con stati espliciti |
| Control Surface | Lista unica di sezioni chiuse, molte descrizioni, Save sempre disponibile | Tre destinazioni Display / Wi-Fi / Content, disclosure accessibili, controlli più larghi su desktop, Save fisso e stato delle modifiche |
| Salvataggio | Preview e persistenza non sempre distinguibili | Tema di preview nel browser, luminosità live in RAM, Save per NVS; Reload invia un POST vuoto e conserva le scelte in corso |
| Telefono | Molto testo prima dei comandi e azioni piccole | Campi da almeno 16 px, pulsanti principali da 44 px, disposizione a colonna, ultimo feed raggiungibile sopra la barra Save |

Il conteggio prodotto è allineato alla configurazione: **sette temi, quattordici lingue del display, otto di Wikipedia, nove viste nella gallery, cinque feed RSS e uno–otto articoli per feed**. Doom resta un extra opzionale, senza inventare una decima cattura della gallery.

## Direzione e scelte

È stata applicata `design-taste-frontend`, con le istruzioni per app al pannello e per pagina alla landing. Le estrazioni riproducibili sono conservate in [landing-direction.json](evidence/landing-direction.json) e [control-direction.json](evidence/control-direction.json).

- **Landing — Colour chapters**, seed 1752849615: la proporzione 640×172 del display guida la pagina, con capitoli a colore pieno e immagini utili. Nessuna fotografia stock di altro hardware, nessuna testimonianza o cliente inventato. Le quattro famiglie evitano di ripetere nove schede dopo la gallery.
- **Control — True-black flat / Settings in two columns**, seed 1263176608: task chiari e toni di fondo per i gruppi di campi. La descrizione occupa circa 28% della griglia desktop; i controlli circa 72%. Su telefono si impilano.
- Palette e caratteri di marca sono mantenuti: anche il violetto di Default e il carattere di Toxic Candy. Il nero letterale della direzione non sostituisce i sette temi esistenti.
- Il titolo “Desk, meet ScryBar.” rimane su due righe ai sei viewport verificati; “Your desk knows things now.” accompagna una spiegazione concreta del prodotto.
- Nessun autosave aggiunto. Il testo distingue browser preview, luminosità applicata in RAM e dati salvati sul dispositivo.

## Immagini e versioni

Mac Stats usa i sette render nativi in `screenshots/design-quality/native-stats/`, con dati di esempio anonimi. Diagnostics usa il render nativo `screenshots/display-quality-r321/native/scrybar-default--info--normal.png`, con rete di esempio, indirizzo di documentazione e QR example.invalid. Le PNG sono copiate senza ritocchi; le didascalie dichiarano sample data. Il precedente asset Diagnostics con identificatori personali non è importato né emesso dal build.

Home segue i sette temi; Mac Stats ha sette render coerenti. Le altre viste mantengono il tema della propria cattura, come indicato accanto al selettore. L’immagine social è una nuova cattura del DOM del sito con il render anonimo, riproducibile tramite `web/tools/capture-og.mjs`.

La preparazione della release aggiorna il manifest a **firmware 0.9.5 / r322** e il download a **Companion 0.3.15 (51), Preview, signed ad hoc, not notarized**. La verifica del download pubblico è registrata separatamente dal controllo del link nel markup: [QA della release](QA.md#preparazione-della-release-v095). Il testo dell’installer deriva dal manifest; il workflow aggiorna la versione prima di compilare Astro.

## Confronti visivi

| Superficie | Prima390 px | Dopo390 px | Dopo1440 px |
| --- | --- | --- | --- |
| Landing | [Prima](screenshots/landing-before-390.jpg) | [Dopo](screenshots/landing-after-390.png) | [Desktop](screenshots/landing-after-1440.png) |
| Control r322, fixture redatta | [Prima](screenshots/control-before-390.jpg) | [Dopo](screenshots/control-r322-after-390.png) | [Desktop](screenshots/control-r322-after-1440.png) |

Sono state ispezionate anche la [pagina completa mobile](screenshots/landing-after-full-390.png), la [pagina completa desktop](screenshots/landing-after-full-1440.png), il fallback installer WebKit e il pannello a 320 px con font bloccati. I render del Control mostrano HTML del dispositivo catturato dal coordinatore e poi redatto; API e modifiche nelle prove browser sono locali.

La matrice, le prove di persistenza rispetto alla RAM, i rilievi corretti e i limiti sono in [QA.md](QA.md). Nessuna pubblicazione del sito è inclusa in questa fase.
