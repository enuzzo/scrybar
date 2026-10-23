# Equivalenza grafica r321 → r322

Le prove grafiche già prodotte su r321 restano applicabili alle parti native di r322 confrontate qui. **Non sono stati rigenerati i 70 render, i 196 render QR o i 910 campioni dell’orologio.** Il confronto è stato fatto contro la copia privata esatta della build r321 e contro il manifest della galleria conservata, non contro una revisione ricostruita.

| Confronto | Esito |
|---|---|
| 125 funzioni aggiuntive nell’inventario della galleria | Corpi identici byte per byte |
| Translation unit LVGL completa, inclusi Stats, temi, strutture e costanti | Identica dopo la sola sostituzione del valore `FW_BUILD_TAG` |
| 51 funzioni effettive di orologio/data + 5 dichiarazioni e tabelle | Identiche byte per byte |
| 35 funzioni display, touch, transizioni, main loop e setup | Identiche byte per byte |
| 33 dipendenze, inclusi 18 font Dosis reali, asset, helper e configurazione LVGL | File identici byte per byte |
| `UiStrings` | File identico: `617580010ac22f5f87021a467a8f4ad7098495754f287379bea255d5da7a548a` |
| Translation unit completa r321 conservata fuori Dropbox | Coincide esattamente con estrazione r321 + fixture anonima attuale |
| 4 test renderer + 2 test orologio + 1 test luminosità persistente | Tutti superati; nessun replay delle matrici |

Gli hash dell’unità LVGL estratta di r321 e del suo manifest coincidono: `ab53fd3c7feb45063831385f6176395708d8a999c8c821b633d5b8df7706d15b`. Dopo aver normalizzato esclusivamente il tag versione, l’unità r322 ha lo stesso contenuto. Il file [r322-equivalence.json](r322-equivalence.json) conserva tutti gli hash, gli inventari e i riferimenti esatti alle prove riutilizzate.

La differenza nel `.ino` completo riguarda solo due snapshot della luminosità salvata, i rispettivi helper NVS/accessor e il loro uso nella pagina Control, il confronto JS con i valori persistenti e il testo «Save changes». `config.h` cambia soltanto `FW_BUILD_TAG`, da r321 a r322. Sono cambiamenti intenzionali: questa verifica non afferma equivalenza dell’intero firmware.

- SHA-256 `.ino` r321: `76cd1d728e86ba8a78807e9a77fb9eee8705cc028fe50b4365a28219c3fc84de`.
- SHA-256 `.ino` r322 verificato: `10b0ac004e8942950c44db75d08fd33ec1b31a78509d1f42f2d0ebd285209dba`.
- La label firmware INFO cambia intenzionalmente versione; nella fixture anonima resta `Example`.
- La persistenza della luminosità cambia intenzionalmente lo stato Control/NVS. A parità di codice dei widget non si deduce uguale luminosità con impostazioni salvate diverse.

Le prove visuali riutilizzate sono descritte in [native-pages.md](native-pages.md) e [wordclock-review.md](wordclock-review.md). Restano render software e test nativi con dati anonimi; questa equivalenza non aggiunge fotografie, misure FPS, test di memoria o prove touch sul dispositivo. Nessun file di credenziali è stato letto e nessun accesso USB è stato eseguito.

Comandi mirati eseguiti:

```sh
python3 tests/render_display_quality_native_test.py
python3 tests/audit_wordclock_native_test.py
python3 tests/backlight_persistence_native_test.py
```

L’ultimo test compila i veri helper di persistenza estratti dal firmware in una cartella temporanea esterna a Dropbox, usando AddressSanitizer e UndefinedBehaviorSanitizer. Copre default, caricamento, anteprima e riapertura, annullamento, errori, scrittura parziale, conferma e clamp. Non legge le impostazioni del dispositivo.
