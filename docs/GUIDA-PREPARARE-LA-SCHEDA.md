# Preparare la scheda, passo per passo (VesevOS 1.7.45)

Questa guida porta una scheda **ESP32-S3 SuperMini** nuova (o da rifare) fino a VesevOS funzionante con l'aggiornamento senza cavo.
Tempo: circa 40 minuti la prima volta. Gli esempi sono per Mac; per Linux e Windows cambia solo il nome della porta (vedi passo 5).
[English version](PREPARE-THE-BOARD.md)

**Come funziona, in due parole.** Nella scheda vivono due programmi: il **recovery** (piccolo, serve solo ad aggiornare) e **VesevOS**.
La prima installazione si fa con il cavo USB. Poi VesevOS si aggiorna da solo dalla pagina web (menu utente > *Aggiorna firmware*),
senza cavo e senza scheda SD.

## Cosa serve
- La scheda ESP32-S3 SuperMini (4 MB di flash, 2 MB di PSRAM) e un cavo USB **che trasmette dati** (non solo di ricarica).
- Un computer con Arduino IDE 2.x e Python 3.
- Il repository VesevOS scaricato da GitHub (pulsante *Code > Download ZIP*) e decompresso.

## Passo 1 - Arduino IDE e librerie
1. Installa **Arduino IDE 2.x** dal sito arduino.cc.
2. *Impostazioni > URL aggiuntivi per il Gestore schede*: aggiungi `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
3. *Strumenti > Scheda > Gestore schede*: cerca **esp32** (di Espressif) e installa la versione **3.3.x** o piu recente.
4. *Strumenti > Gestisci librerie*: installa **PsychicHttp** (3.1.x), **ArduinoJson** (7.x) e **NimBLE-Arduino** (2.x, di h2zero).

## Passo 2 - Impostazioni della scheda
In *Strumenti* scegli, **sia per il recovery sia per VesevOS**:

| Voce | Valore |
|---|---|
| Scheda | ESP32S3 Dev Module |
| PSRAM | QSPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |

Lo schema delle partizioni serve solo al compilatore per controllare la dimensione: quello vero lo mette lo script del passo 6.

## Passo 3 - Compila il recovery
1. Apri `recovery/VesevOS_Recovery/VesevOS_Recovery.ino`.
2. *Sketch > Esporta binario compilato*. Nella cartella dello sketch nasce `build/esp32.esp32.esp32s3/`.
3. Il file `VesevOS_Recovery.ino.bin` deve pesare **meno di 1.048.576 byte** (circa 1.010.000: ci entra, con poco margine).

## Passo 4 - Compila VesevOS
1. Apri `VesevOS/VesevOS.ino` (la cartella deve contenere anche `src/`).
2. *Sketch > Esporta binario compilato*.
3. Il file `VesevOS.ino.bin` deve pesare **meno di 2.424.832 byte** (circa 2.026.000).

**Non usare mai il pulsante *Carica* dell'IDE** per VesevOS: scriverebbe al posto sbagliato e cancellerebbe il recovery.

## Passo 5 - Strumento di caricamento (esptool)
1. Nel Terminale: `python3 -m pip install esptool`
2. Collega la scheda. Trova la porta: sul Mac `ls /dev/cu.usbmodem*` (es. `/dev/cu.usbmodem14401`); su Linux `/dev/ttyACM0`; su Windows `COM5` (Gestione dispositivi).
3. Chiudi il *Monitor seriale* dell'IDE (occupa la porta).
4. Se la scheda non risponde: tieni premuto **BOOT**, premi e lascia **RESET**, lascia BOOT, e riprova.

## Passo 6 - Backup (consigliato) e installazione
**Backup della flash** (circa 8 minuti, una volta sola; permette di tornare com'era):
`python3 -m esptool --chip esp32s3 -p PORTA -b 460800 read_flash 0 0x400000 ~/vesevos-backup-4MB.bin`

**Installazione.** Nella cartella `recovery/` del repository:

- **Scheda nuova o da rifare da zero** (CANCELLA tutto, anche la configurazione):
  `./installa.sh PORTA CARTELLA_BUILD_RECOVERY CARTELLA_BUILD_VESEVOS/VesevOS.ino.bin`
- **Scheda gia con VesevOS** (non cancella la configurazione):
  `./carica-tutto.sh PORTA CARTELLA_BUILD_RECOVERY CARTELLA_BUILD_VESEVOS/VesevOS.ino.bin`

Esempio (Mac):
`./installa.sh /dev/cu.usbmodem14401 ~/Documents/Arduino/recovery/VesevOS_Recovery/build/esp32.esp32.esp32s3 ~/Documents/Arduino/VesevOS/build/esp32.esp32.esp32s3/VesevOS.ino.bin`

Gli script controllano che i file entrino negli spazi e ti dicono se manca qualcosa. Alla fine dicono "Fatto".
Se vedi *"no sync reply"*: chiudi il monitor seriale, ripeti il passo 5.4 e rilancia.

## Passo 7 - Primo avvio di VesevOS
1. Apri il Monitor seriale (115200) e premi RESET sulla scheda. All'avvio il recovery aspetta 3 secondi, poi parte VesevOS.
2. Leggi la schermata di benvenuto: rete Wi-Fi `VesevOS`, password casuale (12 caratteri) e indirizzo `http://192.168.4.1`.
3. Collegati a quella rete con telefono o PC, apri la pagina e **scegli la password del pannello** (almeno 6 caratteri).
4. Segui la **configurazione guidata** (lingua, paese, nome, antenna, Wi-Fi di casa, ora). Dettagli: manuale, capitolo 1.

## Passo 8 - Aggiornare in futuro (senza cavo)
1. Compila ed esporta la nuova versione (passo 4).
2. Apri la pagina di VesevOS, entra come **Amministratore**, menu utente > **Aggiorna firmware** > *Riavvia nel recovery*.
3. La pagina mostra un indirizzo (per esempio `http://192.168.1.50:80/`). Aprilo in una **finestra privata** del browser e scrivi **http**, non https.
   Se in 30 secondi la scheda non trova la rete di casa, crea la rete Wi-Fi `VesevOS-recovery`: la password e sul monitor seriale.
4. Entra con utente e password di un Amministratore di VesevOS.
5. Scegli `VesevOS.ino.bin`. Il campo *SHA-256 atteso* e facoltativo: se lo compili, il file deve coincidere.
6. *Carica e installa*. La pagina controlla il file (intestazione, marchio VesevOS, dimensione) prima di inviarlo; la scheda lo ricontrolla prima di attivarlo.
7. Dopo circa 10 secondi VesevOS riparte con la versione nuova. Per tornare senza aggiornare: *Avvia VesevOS senza aggiornare*.

Dalla seriale si puo anche scrivere `recovery` (dice se c'e) e `recovery now` (riavvia nel recovery). Nel recovery: `i` informazioni, `b` avvia VesevOS, `r` riavvia.

## Se qualcosa non va
| Problema | Rimedio |
|---|---|
| *no sync reply* o porta non trovata | Cavo solo-ricarica? Prova un altro cavo. Chiudi il monitor seriale. Entra in modo download con BOOT + RESET. |
| Il recovery e troppo grande | Usa lo stesso core (3.3.x) e le stesse impostazioni del passo 2. |
| La pagina del recovery non si apre | Finestra privata e `http://IP:80/` (i browser spesso forzano https). Controlla l'IP sul monitor seriale. |
| *Non e un firmware VesevOS (manca il marchio)* | Hai scelto il file sbagliato (per esempio quello del recovery o `.merged.bin`). Serve `VesevOS.ino.bin` della 1.7.44 o piu recente. |
| Dopo un errore VesevOS non parte | Normale: il recovery resta acceso. Ricarica il file buono dalla sua pagina. |
| Password dell'amministratore persa | Tasto BOOT 8 secondi (manuale, capitolo 2). Nel recovery, se non c'e nessun amministratore, serve il codice a 8 cifre mostrato sulla seriale. |
| Voglio tornare com'era | `python3 -m esptool --chip esp32s3 -p PORTA -b 460800 write_flash 0 ~/vesevos-backup-4MB.bin` |

## Sicurezza: da sapere
- Il recovery usa **solo HTTP** (niente cifratura): la password non viaggia in chiaro (accesso a sfida e risposta), ma il codice di sessione e il file si vedono nella rete locale. Usalo su una rete di cui ti fidi.
- Il controllo del file verifica che sia un firmware VesevOS completo; **non e una firma digitale**. Scarica i file solo da fonti fidate e, se vuoi, confronta lo SHA-256.
- Non e consulenza legale. Vedi [SECURITY.md](../SECURITY.md) per segnalare problemi di sicurezza.
