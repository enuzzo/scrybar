# LAUNCH Page Visual Polish — Handoff (r253)

Stato: **parzialmente completato**. La header è stata fixata. Il resto ha bug non risolti — le modifiche al codice (badge width, row colors) non producono risultati visibili sul device. Serve debug metodico: una modifica → verifica → prossima.

## Cosa è stato fatto e funziona (verificato su device)

1. **Header** — ora usa `lvglResolvedHeaderBg(t)` + `bg_opa = LV_OPA_COVER` + `border_width=0` + `pad_all=0`. Layout 3 colonne: titolo sinistra, mission center, ora destra. Tutti `lvglFontSmall()` (18px), colore `lvglResolvedHeaderText(t)`. Identica a Transit.
2. **Tofu fix** — `\xC2\xB7` (U+00B7, middle dot) sostituito con ASCII `|` in 3 punti (hero vehicle|pad, detail provider|vehicle, detail weather separator). I font Funnel Display coprono solo U+0020–U+007E.
3. **Provider colors** — brightened: SpaceX 0x1E88E5, Rocket Lab 0x5C6BC0, ULA 0x2979FF, ISRO 0xF57C00, Arianespace 0x1976D2, CASC 0xE53935, Roscosmos 0x42A5F5, fallback 0xAB47BC.
4. **Overlay detail** — backdrop 220 opacity (quasi opaco), panel usa `t.screenBg` con radius 8 + border 2px `t.divider`, close button è pill con bg `t.headerBg` + testo bianco, title 22px (`lvglFontRssNews`), countdown 20px, provider 18px, location/window 16px, description 16px wrap, QR 88px con border 3px, tag pills renderizzati.
5. **Weather separator bug** — fixato: no `|` iniziale quando winBuf è vuoto ma c'è weather.
6. **Serial commands** — aggiunti `LAUNCH`/`VIEWLAUNCH`/`VIEW7` per navigazione e `LAUNCHDETAIL [idx]` per testare l'overlay.

## Cosa NON funziona — bug aperti

### BUG 1: Badge width non cambia
Il codice dice `badgeW = 110` per compact rows e `heroBadgeW = 110` per hero, ma sul device i badge sembrano identici alla versione precedente (~82px). **Causa probabile**: i badge usano `lv_obj_remove_style_all()` seguito da `lvglSetBgFlat()` che NON setta `bg_opa`. Quindi il badge è trasparente e l'unica parte visibile è il testo label. Stessa root cause del bug header.

**Fix**: aggiungere `lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN)` dopo `lvglSetBgFlat()` per ogni badge (hero + compact rows). Confronta con Transit che usa `lv_obj_set_style_bg_opa(lineBg, LV_OPA_COVER, LV_PART_MAIN)` esplicitamente (riga ~13859).

### BUG 2: Row tint/colori non visibili
Il codice setta `bg_opa = 22` sulla hero row e `bg_opa = TRANSP` sulle compact rows, ma visivamente sono tutte identiche. **Causa probabile**: `lv_obj_remove_style_all()` + nessun `bg_opa` esplicito dopo. Transit NON usa `remove_style_all` — usa gli stili LVGL di default e li sovrascrive.

**Fix da considerare**: rimuovere `lv_obj_remove_style_all()` dalle row e settare ogni proprietà esplicitamente (come fa Transit), oppure aggiungere `bg_opa = LV_OPA_COVER` dopo ogni `lvglSetBgFlat()`.

### BUG 3: Date/orari non matchano Transit
Launch compact rows: date in `lvglFontMini()` (16px), colore `t.auxMeta`, right-aligned. Transit: times in `lvglFontMeta()` (20px), colore `t.infoText`, right-aligned. Le date in Launch sono più piccole e muted.

**Fix**: portare a `lvglFontMeta()` (20px) e `t.infoText` come Transit.

## Pattern di riferimento: Transit (il gold standard)

```
Header:   30px, lvglResolvedHeaderBg, bg_opa=COVER, border=0, pad=0
          title: lvglFontSmall (18px), lvglResolvedHeaderText
          station: lvglFontSmall (18px), center
          time: lvglFontTiny (14px), right — NB: Transit usa 14px per l'ora!
Row:      35px, NO remove_style_all, border=0, radius=0, pad=0
          odd rows: bg_color=altTint, bg_opa=22
          even rows: bg_opa=TRANSP
Badge:    82x30px, bg_opa=COVER, border=0, radius=6, pad=0
          label: lvglFontMeta (20px), white, SCROLL mode, anim_speed=15
          label size: 76x26 (badge-6, badge-4), align CENTER +3
Dest:     lvglFontMeta (20px), t.infoText, pad_top=(35-20)/2+2
Time:     lvglFontMeta (20px), t.infoText, right-aligned
Sep:      cW-16 wide, 1px, t.divider, opa=30, pos x=8
```

## Root cause del fallimento della sessione

`lv_obj_remove_style_all()` rimuove TUTTE le proprietà di stile inclusa `bg_opa`. `lvglSetBgFlat()` setta solo `bg_color` e `bg_grad_color` ma NON `bg_opa`. Risultato: oggetti con il colore giusto ma trasparenti. La fix è sempre la stessa: dopo ogni `lvglSetBgFlat()` su un oggetto stripped, aggiungere `lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN)`.

Alternativa migliore: NON usare `lv_obj_remove_style_all()` e invece settare ogni proprietà esplicitamente come fa Transit.

## Metodo di lavoro per la prossima sessione

1. UNA modifica alla volta
2. Compile → upload → screenshot → verifica REALE a occhio
3. Se la screenshot non mostra il cambiamento, il cambiamento non è avvenuto — debug prima di andare avanti
4. NON dichiarare "fatto" senza screenshot che lo dimostri
5. Confrontare SEMPRE con Transit side-by-side

## Codice di riferimento

- Launch UI init: `lvglInitLaunchUi()` — cerca "Launch Page LVGL" (~riga 13945)
- Launch detail: `lvglInitLaunchDetail()` — subito dopo
- Launch update: `lvglUpdateLaunchUi()` — popola i dati
- Launch countdown tick: `lvglTickLaunchCountdown()`
- Transit UI init: `lvglInitTransitUi()` — cerca "Transit LVGL init" (~riga 13757)
- Style helpers: `lvglSetBgFlat()` (~riga 9850) — NOTA: non setta bg_opa!
- Resolved colors: `lvglResolvedHeaderBg()` (~riga 9790)
- Font sizes: 14=Tiny, 16=Mini, 18=Small, 20=Meta, 22=RssNews, 24=Body, 30=Title, 32=Big
- Struct: `LvglLaunchUi` (~riga 1133)
- Touch: tap handler (~riga 11767), Y zones: hero 33–76, rows 76–108, 108–140, 140–172
- Serial: `cmdViewLaunch`, `cmdLaunchDetail` (~riga 17080)
