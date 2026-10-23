# Word clock — revisione finita del 23 ottobre 2026

[audit_wordclock_native.py](../../../tools/audit_wordclock_native.py) estrae le quattordici funzioni reali `composeWordClockSentence*`, i relativi helper e i formatter delle date. Le esegue con il vero `lvglApplyClockSentenceAutoFit` di HOME, LVGL8 e i font del firmware. Non modifica il dispositivo.

La [matrice JSON](wordclock.json) e il [campione leggibile](wordclock.md) corrispondono al sorgente finale r321 con SHA-256 `76cd1d728e86ba8a78807e9a77fb9eee8705cc028fe50b4365a28219c3fc84de`.

## Risultato misurato

| Controllo | Esito |
|---|---:|
| Lingue | 14 |
| Ore campionate | 00, 01, 02, 12, 23 |
| Minuti per ora | 00, 01, 02, 03, 14, 15, 29, 30, 44, 45, 57, 58, 59 |
| Frasi eseguite | 910 |
| Frasi vuote o troncate nel buffer reale | 0 |
| Glifi mancanti nel testo originale | 0 |
| Frasi fuori dall'area HOME | 0 |
| Date troncate nel buffer aggiornato a 96 byte | 0 |
| Errori nei controlli espliciti dei confini | 0 |

Tutti i 910 campioni mantengono Dosis32, con `line_height=39`: nei casi misurati l'auto-fit non ha ridotto il font. Sono compresi Pirate, Shakespeare, Valley e Bellazio.

I controlli dei confini confrontano i minuti appartenenti allo stesso intervallo arrotondato e il passaggio 23:59→00:00 e 00:58→01:00. Pirate e Bellazio mantengono le variazioni intenzionali delle introduzioni per minuto. Esempi controllati esplicitamente: inglese 00:03 e 23:57, singolare spagnolo, `ein Uhr`/`eins` tedesco, ordinali e `antaŭ` esperantisti e numero dodici klingon.

## Correzioni motivate

| Caso | Problema rilevato | Forma verificata dopo la correzione |
|---|---|---|
| Spagnolo 01:15 / 12:45 | Verbo plurale con `la una` | `Es la una y cuarto` / `Es la una menos cuarto` |
| Tedesco 01:15 / 00:58 | `ein` usato anche come numero autonomo | `Viertel nach eins` / `ein Uhr` |
| Klingon 00:00 / 12:00 | La parola usata per dodici significava venti | `wa'maH cha'` |
| Esperanto | Cardinali al posto degli ordinali; preposizione `al` nelle ore mancanti | `unua`, `dek-dua`, `antaŭ la unua` |
| Italiano | Fallback ASCII legacy per È e giorni della settimana | `È l'una`, `Venerdì` |
| Date Pirate | Il buffer da 48 byte tagliava il testo e la parte finale dell'anno in tutti i 65 campioni | Il buffer da 96 conserva `Year of Our Plunder 2026`; la label può applicare la propria ellissi visiva |

La concordanza spagnola è esplicitata da [FundéuRAE](https://www.fundeu.es/consulta/que-hora-es-o-que-horas-son-200/). La distinzione fra numero autonomo tedesco e forma prima di Uhr è descritta da [Duden](https://www.duden.de/sprachwissen/sprachratgeber/Ein-eins-unbestimmter-Artikel-oder-Zahlwort). Gli ordinali e la preposizione nelle indicazioni dell'ora sono documentati dal [PMEG](https://bertilow.com/pmeg/gramatiko/nombroj/horoj.html). Per Klingon, il [Klingon Language Institute](https://www.kli.org/tlhIngan-Hol/2004/January/msg00040.html) distingue esplicitamente dodici e venti nella tabella numerica.

Le prove native dei glifi `È`, `ì`, `ŭ` e `ŝ` sono positive in Dosis32, Dosis16 e Dosis Semibold20. È stato rimosso il solo ripiegamento ASCII dell'orologio interno: il precedente percorso trasformava, per esempio, `zwölf` in `zwolf`. I contenuti esterni conservano il loro percorso di normalizzazione.

## Limiti

Questa è una verifica finita di buffer, UTF-8, glifi, geometria e alcuni casi grammaticali concreti. Non è una certificazione linguistica completa di quattordici lingue. Latino, Klingon e le voci giocose mantengono il registro scelto dal progetto; non viene rivendicata un'autenticità classica o canonica più ampia.

Le date usano un giorno valido di esempio, venerdì 23 ottobre 2026: non è stata enumerata l'intera combinazione di mesi, anni bisestili e giorni. Il font-fit usa la geometria reale di HOME nel tema normale; gli altri temi condividono i medesimi font e bounds. Colori e resa fisica appartengono alle verifiche separate della matrice delle pagine e dell'hardware.

## Riproduzione

```sh
python3 tools/render_display_quality_native.py --pages HOME --themes scrybar-default \
  --out-dir /tmp/scrybar-clock-home
python3 tools/audit_wordclock_native.py
python3 tests/audit_wordclock_native_test.py
```

Il primo comando prepara la libreria LVGL e i font nella directory temporanea esterna a Dropbox. L'audit riusa quei prodotti locali; non installa dipendenze o copie per un altro Mac.
