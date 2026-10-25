# Preparare la scheda, passo per passo (VesevOS 1.8.0b)

Questa guida porta una scheda **ESP32-S3 SuperMini** nuova (o da rifare) fino a VesevOS funzionante con l'aggiornamento senza cavo.
Tempo: circa 40 minuti la prima volta. Gli esempi sono per Mac; per Linux e Windows cambia solo il nome della porta (vedi passo 3).
[English version](PREPARE-THE-BOARD.md)

**Come funziona, in due parole.** Nella scheda vivono due programmi: il **recovery** (piccolo, serve solo ad aggiornare) e **VesevOS**.
La prima installazione si fa con il cavo USB. Poi VesevOS si aggiorna dalla pagina web (menu utente > *Aggiorna tramite recovery*),
senza cavo e senza scheda SD.

## Cosa serve
- La scheda ESP32-S3 SuperMini (4 MB di flash, 2 MB di PSRAM) e un cavo USB **che trasmette dati** (non solo di ricarica).
- Un computer con Python 3 (per `esptool`). Per compilare tu i file serve anche ESP-IDF: vedi il [README](../README.md), sezione di compilazione.
- Il repository VesevOS scaricato da GitHub (pulsante *Code > Download ZIP*) e decompresso.
- I **tre file da scrivere**: il recovery (`VesevOS-Recovery-0.5.1.bin`), il firmware (`VesevOS-<versione>.bin`, per esempio `VesevOS-1.8.0b.bin`)
  e i due file di avvio (bootloader e tabella delle partizioni) prodotti dalla compilazione del recovery.
  I file pronti stanno nella cartella `firmware/` del repository (con SHA-256 e `indice.json`); bootloader e tabella delle partizioni, se non compili tu, stanno in `flash/bin/` (`bootloader-1.8.0.bin`, `partition-table-1.8.0.bin`).

## Passo 1 - Come sono fatti i file e dove vanno
VesevOS usa ESP-IDF: non serve nessun IDE e non esiste un pulsante "Carica". I file si scrivono con `esptool`, a **indirizzi espliciti**.

| Indirizzo | Contenuto | Dimensione massima |
|---|---|---|
| `0x0` | bootloader (`bootloader/bootloader.bin` della cartella di build del recovery) | - |
| `0x8000` | tabella delle partizioni (`partition_table/partition-table.bin` della stessa cartella) | - |
| `0x10000` | recovery (`VesevOS-Recovery-0.5.1.bin`) | 1.048.576 byte |
| `0x110000` | firmware (`VesevOS-<versione>.bin`) | 2.424.832 byte |
| `0x360000` | `spiffs` (i file di configurazione): **non si scrive**, la crea il firmware | - |

La tabella delle partizioni e descritta in `idf-recovery/partitions.csv` (il progetto di compilazione del recovery, accanto al repository, oggi non pubblicato); il file pronto e `flash/bin/partition-table-1.8.0.bin`.
Il recovery 0.5.1 pesa 873.200 byte, il firmware 1.8.0b 2.212.400 byte: entrambi entrano.

**Non scrivere mai il firmware a `0x10000`**: cancellerebbe il recovery. Il pulsante "Carica" di un IDE e `idf.py flash` scrivono li: non usarli per VesevOS.

## Passo 2 - Controllo dei file
Gli script nella cartella `recovery/` usano `recovery/controlli.sh`: rifiuta il recovery al posto del firmware (e il contrario), il file `.merged.bin`
e i file che non sono programmi ESP32-S3 (intestazione, marchio `{FW:VesevOS}` o `=== VesevOS Recovery`, dimensione). Per saltarli: `FORZA=1` (sconsigliato).
Prova a secco (non scrive nulla): `bash -c '. recovery/controlli.sh; vos_chk_fw firmware/1.8.0b/VesevOS-1.8.0b.bin && echo ok'`.

## Passo 3 - Strumento di caricamento (esptool)
1. Nel Terminale: `python3 -m pip install esptool`
2. Collega la scheda. Trova la porta: sul Mac `ls /dev/cu.usbmodem*` (es. `/dev/cu.usbmodem14401`); su Linux `/dev/ttyACM0`; su Windows `COM5` (Gestione dispositivi).
3. Chiudi ogni programma che usa la porta (monitor seriale).
4. Se la scheda non risponde: tieni premuto **BOOT**, premi e lascia **RESET**, lascia BOOT, e riprova.

## Passo 4 - Backup (consigliato)
**Backup della flash** (circa 8 minuti, una volta sola; permette di tornare com'era):
`python3 -m esptool --chip esp32s3 -p PORTA -b 460800 read_flash 0 0x400000 ~/vesevos-backup-4MB.bin`

## Passo 5 - Installazione con il cavo
- **Scheda nuova o da rifare da zero** (CANCELLA tutto, anche la configurazione):
  ```
  python3 -m esptool --chip esp32s3 -p PORTA -b 460800 erase_flash
  python3 -m esptool --chip esp32s3 -p PORTA -b 460800 write_flash --flash_mode keep --flash_freq keep --flash_size 4MB \
    0x0 flash/bin/bootloader-1.8.0.bin 0x8000 flash/bin/partition-table-1.8.0.bin 0x10000 firmware/recovery/0.5.1/VesevOS-Recovery-0.5.1.bin 0x110000 firmware/1.8.0b/VesevOS-1.8.0b.bin
  ```
- **Solo il firmware**, su una scheda che ha gia il recovery (non tocca recovery e configurazione):
  `recovery/carica-firmware.sh PORTA firmware/1.8.0b/VesevOS-1.8.0b.bin`
  (e come `write_flash 0x110000 <file>`, dopo i controlli del passo 2).

Controlla sempre di non aver scambiato i file: il recovery va a `0x10000`, il firmware a `0x110000`.
Per chi preferisce gli script: `recovery/installa.sh PORTA CARTELLA [FIRMWARE]` (scheda nuova, cancella tutto) e `recovery/carica-tutto.sh PORTA CARTELLA FIRMWARE` (senza cancellare); la cartella e quella della compilazione ESP-IDF del recovery e contiene `VesevOS_Recovery.bin`, `bootloader.bin` e `partition-table.bin`. Gli script fanno gli stessi controlli del passo 2. La prima installazione "da zero" con questi file e script NON risulta provata su una scheda in un rapporto scritto: fai il backup e prova prima su una scheda di prova.
Se vedi *"no sync reply"*: chiudi il monitor seriale, ripeti il passo 3.4 e rilancia.

**Senza terminale:** la pagina di prima installazione `flash/` (da abilitare con GitHub Pages, vedi `flash/README.md`) scrive gli stessi file dal browser, con la porta seriale (Web Serial) e controlli su SHA-256, marchio e dimensione. **NON PROVATA con una scheda reale.**

## Passo 6 - Primo avvio di VesevOS
1. Apri il Monitor seriale (115200) e premi RESET sulla scheda. All'avvio il recovery aspetta 3 secondi, poi parte VesevOS.
2. Leggi la schermata di benvenuto: rete Wi-Fi `VesevOS`, password casuale (12 caratteri) e indirizzo `http://192.168.4.1`.
3. Collegati a quella rete con telefono o PC, apri la pagina e **scegli la password del pannello** (almeno 6 caratteri).
4. Segui la **configurazione guidata** (lingua, paese, nome, antenna, Wi-Fi di casa, ora). Dettagli: manuale, capitolo 1.

## Passo 7 - Aggiornare in futuro (senza cavo)
1. Compila la nuova versione, oppure prendi il file pronto da `firmware/` e controlla il suo SHA-256 (vedi `firmware/LEGGIMI.md`).
2. Apri la pagina di VesevOS, entra come **Amministratore**, menu utente > **Aggiorna tramite recovery** > *Riavvia nel recovery*.
3. La pagina mostra un indirizzo (per esempio `http://192.168.1.50:80/`). Aprilo in una **finestra privata** del browser e scrivi **http**, non https.
   Se in 30 secondi la scheda non trova la rete di casa, crea la rete Wi-Fi `VesevOS-recovery`: la password e sul monitor seriale.
4. Entra con utente e password di un Amministratore di VesevOS.
5. Scegli `VesevOS-<versione>.bin`. Il campo *SHA-256 atteso* e facoltativo: se lo compili, il file deve coincidere.
6. *Carica e installa*. La pagina controlla il file (intestazione, marchio VesevOS, dimensione) prima di inviarlo; la scheda lo ricontrolla prima di attivarlo.
7. Dopo circa 10 secondi VesevOS riparte con la versione nuova. Per tornare senza aggiornare: *Avvia VesevOS senza aggiornare*.

Dalla seriale si puo anche scrivere `recovery` (dice se c'e) e `recovery now` (riavvia nel recovery). Nel recovery: `i` informazioni, `b` avvia VesevOS, `r` riavvia.

## Se qualcosa non va
| Problema | Rimedio |
|---|---|
| *no sync reply* o porta non trovata | Cavo solo-ricarica? Prova un altro cavo. Chiudi il monitor seriale. Entra in modo download con BOOT + RESET (passo 3.4). |
| Il recovery e troppo grande | Il limite e 1.048.576 byte (passo 1): usa la configurazione di compilazione del progetto `idf-recovery`. |
| La pagina del recovery non si apre | Finestra privata e `http://IP:80/` (i browser spesso forzano https). Controlla l'IP sul monitor seriale. |
| *Non e un firmware VesevOS (manca il marchio)* | Hai scelto il file sbagliato (per esempio quello del recovery o `.merged.bin`). Serve `VesevOS-<versione>.bin` di una versione con il marchio (1.7.44 o piu recente). |
| Dopo un errore VesevOS non parte | Normale: il recovery resta acceso. Ricarica il file buono dalla sua pagina. |
| Password dell'amministratore persa | Tasto BOOT 8 secondi (manuale, capitolo 2). Nel recovery, se non c'e nessun amministratore, serve il codice a 8 cifre mostrato sulla seriale. |
| Voglio tornare com'era | `python3 -m esptool --chip esp32s3 -p PORTA -b 460800 write_flash 0 ~/vesevos-backup-4MB.bin` |

## Sicurezza: da sapere
- Il recovery usa **solo HTTP** (niente cifratura): la password non viaggia in chiaro (accesso a sfida e risposta), ma il codice di sessione e il file si vedono nella rete locale. Usalo su una rete di cui ti fidi.
- Il controllo del file verifica che sia un firmware VesevOS completo; **non e una firma digitale**. Lo SHA-256 dice che il file non e rotto, non chi lo ha fatto; la scritta "ufficiale" nell'elenco e solo una scritta. Scarica i file solo da questo repository e confronta lo SHA-256.
- Non e consulenza legale. Vedi [SECURITY.md](../SECURITY.md) per segnalare problemi di sicurezza.
