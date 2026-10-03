# VesevOS

Piccolo sistema operativo (firmware) per **ESP32-S3 SuperMini**, scritto per Arduino IDE.
Pagina web di controllo, shell (web e seriale), Wi-Fi, ora via NTP, file, LED, tutto protetto da password.

> Stato: in sviluppo (versione 1.4.1). Il progetto cresce a fasi, vedi "Prossimi passi".

## Cosa fa oggi

- **Rete**: modo AP (la scheda crea la sua rete Wi-Fi) e modo client, IP automatico o fisso, scansione reti, nome host e dominio configurabili (`nome.local` con mDNS).
- **Pagina web** con schede: Riepilogo, Stato, Wi-Fi, Shell, Task, Ora, File, LED, Pin, Config.
- **Shell** web e seriale con molti comandi (digita `help`).
- **Sicurezza**: password con SHA-256 salato e ripetuto, blocco dopo 5 errori, sessioni a tempo. La password protegge pagina, API e shell.
- **Ora**: NTP, fusi orari, formati data/ora, temperatura in C o F, server NTP locale.
- **File**: memoria interna (LittleFS) con cartelle, carica/scarica/modifica.
- **CPU**: velocita automatica o fissa (80/160/240 MHz), temperatura interna, allarme se troppo calda.
- **LED**: LED RGB WS2812 (stato del sistema, battito legato al carico CPU, colore fisso) e un LED aggiuntivo.
- **Configurazione** in stile OpenWrt (`/vesevos.conf`), scaricabile e ripristinabile.
- **Pin**: elenco dei pin in uso.
- **Lingue**: italiano nel firmware; inglese, spagnolo e tedesco come file separati (`lang/`), caricabili dalla pagina senza ricompilare. Si puo aggiungere una lingua con un solo file.
- **Licenze e note legali** consultabili dalla pagina e dalla shell (`license`).

## Requisiti

- Scheda **ESP32-S3 SuperMini** (ESP32-S3FH4R2: 4 MB flash, 2 MB PSRAM).
- Arduino IDE con core **esp32 3.3.x** o piu recente.
- Librerie: **ESP32Async ESPAsyncWebServer** e **AsyncTCP** (ESP32Async).

## Impostazioni Arduino IDE

| Voce | Valore |
|---|---|
| Scheda | ESP32S3 Dev Module |
| PSRAM | QSPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS) |

## Installazione

1. Scarica o clona il repository.
2. Apri la cartella `VesevOS/` (deve contenere `VesevOS.ino`) con Arduino IDE.
3. Imposta la scheda come nella tabella, poi carica lo sketch.
4. Apri il monitor seriale a 115200 per vedere i messaggi di avvio.

## Primo accesso

1. Collegati alla rete Wi-Fi **VesevOS** (password iniziale `vesevos123`).
2. Apri `http://192.168.4.1`.
3. Scegli la password della scheda (minimo 6 caratteri). Vale anche per la shell seriale.

**Password dimenticata:** tieni premuto il pulsante **BOOT** per 8 secondi a scheda gia accesa.
Non tenerlo premuto all'accensione: la scheda entrerebbe in modo download.

## Colori del LED RGB (modo "Stato")

| Colore | Significato |
|---|---|
| Arancio | Avvio |
| Blu | Modo AP (rete propria) |
| Giallo | Sto collegandomi al Wi-Fi |
| Verde | Collegata al Wi-Fi |
| Rosso lampeggiante | Allarme (temperatura alta) |

## Organizzazione del repository

```
VesevOS/      sketch Arduino (VesevOS.ino + file .h/.cpp)
web/          pagina web in HTML (sorgente di vos_page.h)
lang/         file di lingua pronti da caricare (en, es, de) - GENERATI da tools/lang_src.json
licenses/     testi delle licenze (GPL, LGPL, Apache) e modello della nota legale
tools/        mkpage.py (pagina), mklang.py (lingue), mklicense.py (testi legali)
NOTICE.txt    note legali: titolarita, licenze, riferimenti normativi
CHANGELOG.md  cronologia delle versioni
```

Se modifichi `web/index.html`, rigenera la pagina con:

```
python3 tools/mkpage.py
```

I sorgenti usano solo caratteri ASCII (le lettere accentate nella pagina sono entita HTML).

## Lingue

L'italiano e dentro il firmware. Per le altre lingue:

1. Apri la pagina, scheda **Config > Lingue**.
2. Scegli il file (`en.json`, `es.json` o `de.json` dalla cartella `lang/`) e premi **Carica lingua**.
3. Scegli la lingua dal menu in alto a destra. La scheda la ricorda; da shell: `lang en`.

Ogni lingua occupa circa 17 KB della memoria interna. **Per aggiungere una lingua**: copia una
voce di `tools/lang_src.json`, aggiungi la colonna con il nuovo codice in `langs` e nelle voci,
poi esegui `python3 tools/mklang.py`. Il controllo avvisa se manca qualche testo.

## Problemi noti

- Il LED aggiuntivo sul pin 38 non si accende sulla scheda di prova: in verifica (potrebbe essere il GPIO2).

## Prossimi passi

- Aggiornamento del firmware dalla pagina web (OTA).
- Icone e grafica della pagina, tabella Task piu leggibile, captive portal in modo AP.
- Servizi con avvio automatico e ritardato, cron, modalita sicura.
- Programmi Lua, MQTT, installazione da internet, store firmato, HTTPS, scheda SD.

## Licenza

Doppia licenza:

- **GPL v3 o successiva** (file `LICENSE`): gratuita. Chi distribuisce VesevOS modificato, o dentro un
  prodotto, deve pubblicare il proprio codice con la stessa licenza.
- **Licenza commerciale**: per usare VesevOS in prodotti chiusi. Vedi `COMMERCIAL.md`.

Titolarita, licenze delle librerie di terzi e riferimenti normativi: vedi `NOTICE.txt` (anche nella
pagina, scheda Config > Licenze, e nella shell con `license`).

Per contribuire con codice leggi `CONTRIBUTING.md`.
