# Compilare VesevOS per ogni scheda, passo per passo

Un solo sorgente (`VesevOS/src`) serve tutte le schede. Cambia solo la **configurazione** con cui si compila:
il chip, il file di scheda, il file `sdkconfig.defaults.*` e il file dei componenti bloccati.
Questa guida e scritta per ESP-IDF **5.5.5** (la versione 6.x non e adottata).

La procedura e quella usata dall'autore su macOS. Linux e Windows non sono stati provati dall'autore:
se qualcosa cambia, segnalalo (vedi `SECURITY.md` per le sole questioni di sicurezza, il resto nelle Issue).

## 1. Le schede supportate e cosa cambia

| Scheda | Chip | `VOS_CFG` | `IDF_TARGET` | File di scheda | Configurazione | Componenti bloccati |
|---|---|---|---|---|---|---|
| ESP32-S3 SuperMini | ESP32-S3 | `vesevos` | `esp32s3` (predefinito) | `boards/vos_board_s3_supermini.h` | `idf/sdkconfig.defaults.vesevos` | `idf/dependencies.lock` |
| ESP32-C6 SuperMini | ESP32-C6 | `c6` | `esp32c6` | `boards/vos_board_c6_supermini.h` | `idf/sdkconfig.defaults.c6` | `idf/dependencies.lock.c6` |
| ESP32-C3 SuperMini | ESP32-C3 | `c3` | `esp32c3` | `boards/vos_board_c3_supermini.h` | `idf/sdkconfig.defaults.c3` | `idf/dependencies.lock.c3` |
| ESP32-H2 SuperMini | ESP32-H2 | `h2` | `esp32h2` | `boards/vos_board_h2_supermini.h` | non ancora pronta | non ancora pronto |

La H2 **non si compila ancora**: ha il file di scheda ma manca il firmware (non ha il Wi-Fi).

Come si sceglie la scheda:
- **File di scheda**: `VesevOS/src/boards/vos_board.h` sceglie da solo il file giusto in base al chip della compilazione
  (`CONFIG_IDF_TARGET_ESP32C6`, `..._ESP32C3`, altrimenti S3). Pin, LED, tasti e limiti di ogni scheda stanno solo li.
- **Funzioni opzionali**: `boards/vos_packages.h` e il file di scheda decidono cosa entra nel firmware
  (per esempio la PSRAM, il coprocessore ULP e il caricatore ELF esistono solo sull'S3).
- **`VOS_CFG`** sceglie il file `sdkconfig.defaults.<VOS_CFG>`; per C3 e C6 sceglie anche il file dei componenti bloccati.

## 2. Cosa serve

- Un computer con almeno 8 GB di RAM e circa 10 GB di disco liberi (ogni cartella di build occupa circa 300 MB).
- `git`, `python3` e `cmake` (li installa lo script di ESP-IDF o il sistema).
- **ESP-IDF 5.5.5**. Istruzioni ufficiali: <https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/get-started/index.html>.
- Un cavo USB dati e la scheda (solo per caricare il firmware: per compilare non serve).

## 3. Passo 1 - Installare ESP-IDF 5.5.5

```bash
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.5 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3,esp32c3,esp32c6
```

A ogni nuova finestra del terminale, prima di compilare:

```bash
. ~/esp/esp-idf/export.sh
```

Controllo: `idf.py --version` deve dire `ESP-IDF v5.5.5`. Con un'altra versione la compilazione puo non riuscire.

## 4. Passo 2 - Mettere le cartelle al loro posto

Il progetto ESP-IDF cerca i sorgenti in `../repo/VesevOS/src`. Le cartelle vanno quindi affiancate cosi:

```
VesevOS-esp32/          la tua cartella di lavoro (il nome e libero)
  repo/                 questo repository
  idf/                  il progetto ESP-IDF di VesevOS (CMakeLists.txt, main/, components/, sdkconfig.defaults.*, partitions.csv, dependencies.lock*)
  build/                la creerai tu: una sottocartella per scheda
```

```bash
mkdir -p ~/VesevOS-esp32 && cd ~/VesevOS-esp32
git clone https://github.com/paolellad79/VesevOS.git repo
mkdir build
```

Il progetto ESP-IDF e nella cartella `idf/` del repository appena clonato. **Spostala accanto a `repo/`**, perche i suoi file cercano i sorgenti in `../repo/VesevOS/src`:

```bash
mv repo/idf idf
```

I sorgenti del firmware stanno sempre in `repo/VesevOS/src`: non esiste una copia per scheda.

## 5. Passo 3 - Compilare una scheda

Regola importante: **ogni scheda ha la sua cartella di build** (`-B`). Non cambiare mai chip dentro la stessa cartella.

La prima compilazione scarica i componenti gestiti (`mdns`, `esp-nimble-cpp`, `littlefs`) e dura circa 11 minuti;
sul C6 il collegamento finale puo durare molto di piu (in una prova passata circa 50 minuti).

### ESP32-S3

```bash
. ~/esp/esp-idf/export.sh
cd ~/VesevOS-esp32/idf
VOS_CFG=vesevos idf.py -B ../build/s3 build
```

### ESP32-C6

```bash
. ~/esp/esp-idf/export.sh
cd ~/VesevOS-esp32/idf
VOS_CFG=c6 idf.py -B ../build/c6 -DIDF_TARGET=esp32c6 build
```

### ESP32-C3

```bash
. ~/esp/esp-idf/export.sh
cd ~/VesevOS-esp32/idf
VOS_CFG=c3 idf.py -B ../build/c3 -DIDF_TARGET=esp32c3 build
```

Il messaggio `1/2 app partitions are too small for binary` e **atteso**: riguarda lo slot del recovery.
Gli avvisi di compilazione (`warning`) non sono errori.

## 6. Passo 4 - Controllare il risultato

In ogni cartella di build (`../build/s3`, `../build/c6`, `../build/c3`) trovi `VesevOS.bin` e `VesevOS.elf`.

1. **Dimensione**: il `.bin` deve stare nello slot dell'applicazione, **massimo 2.424.832 byte**.
   ```bash
   ls -l ../build/s3/VesevOS.bin
   ```
2. **Impronta SHA-256** (serve per controllarlo prima di caricarlo):
   ```bash
   shasum -a 256 ../build/s3/VesevOS.bin       # su Linux: sha256sum
   ```
3. **Nome** del file da consegnare: `VesevOS-<CPU>-<versione>.bin`, con la CPU `S3`, `C3` o `C6` e la versione
   scritta in `VesevOS/src/core/vos_common.h` (`VOS_VERSION`). Esempio: `VesevOS-C3-1.8.30k.bin`.
4. Il file `.elf` serve solo a leggere i crash: non va pubblicato.

Dimensioni della 1.8.30k, per confronto: S3 1.978.336 byte, C3 2.075.072 byte, C6 2.127.872 byte.

## 7. Passo 5 - Caricare il firmware sulla scheda

**Non usare `idf.py flash`** e nemmeno il tasto "Carica" di un editor: scrivono a `0x10000` e **cancellano il recovery**.
La disposizione della flash e: recovery a `0x10000`, firmware a `0x110000`.

Strade giuste (dettagli in [`GUIDA-PREPARARE-LA-SCHEDA.md`](GUIDA-PREPARARE-LA-SCHEDA.md)):
1. **Scheda nuova**: pagina di prima installazione del sito o `esptool` con i file del recovery e del firmware.
2. **Scheda con VesevOS gia installato**: pagina della scheda, *Sicurezza > Aggiornamenti > Carica da file*
   (il firmware passa dal recovery e viene controllato con l'SHA-256 prima di essere scritto).

Prima di caricare una versione mai provata, usa **una sola scheda** e controlla che parta; poi aggiorna le altre.

## 8. Controlli sul computer (facoltativi, per chi sviluppa)

Dalla cartella `repo/`:

```bash
python3 tools/checklayers.py      # controllo degli strati (deve finire con "OK")
python3 tools/mklang.py           # traduzioni (deve finire con "ok")
tools/stub/compila.sh             # compila tutti i file con finti header, senza scheda
```

`tools/stub/compila.sh` ha bisogno, accanto al repository, delle cartelle `psychic` (PsychicHttp 3.1.2), `aj` (ArduinoJson 7) e `mbed` (Mbed-TLS 3.6, solo gli header).
Se cambi il sorgente della pagina in `web/src/`, rigenera la pagina prima di compilare: `python3 tools/mkweb.py`, poi `mklang.py`, `mkcommon.py` e `mkpage.py`.

## 9. Aggiungere una scheda nuova (per sviluppatori)

1. Scrivi `VesevOS/src/boards/vos_board_<chip>_supermini.h` partendo da quello di una scheda simile
   (pin, LED, tasto, limiti, funzioni presenti).
2. Aggiungi il chip nella catena di `boards/vos_board.h`.
3. Crea `idf/sdkconfig.defaults.<cfg>` e, per i chip diversi dall'S3, il file `idf/dependencies.lock.<cfg>`
   (la riga `MATCHES "^(c3|c6|h2)$"` di `idf/CMakeLists.txt` va estesa).
4. Controlla la RAM libera dopo l'avvio e il margine del `.bin` sullo slot (2.424.832 byte).
5. Prova prima su una scheda sola. Non modificare gli `eFuse`.

## 10. Problemi frequenti

| Sintomo | Causa probabile | Cosa fare |
|---|---|---|
| `idf.py: command not found` | non hai caricato l'ambiente | `. ~/esp/esp-idf/export.sh` |
| Errori su componenti o versioni | ESP-IDF diverso dalla 5.5.5 | reinstalla la 5.5.5 (non la 6.x) |
| `dependencies.lock` riscritto o errori dopo aver cambiato chip | stessa cartella `-B` usata per due chip | una cartella `-B` per scheda e sempre `VOS_CFG` giusto |
| Il `.bin` supera 2.424.832 byte | funzioni in piu o configurazione sbagliata | controlla `VOS_CFG` e `boards/vos_packages.h` |
| Il collegamento finale dura moltissimo | normale sul C6 | aspetta; serve RAM libera e disco |
| La scheda non parte dopo il caricamento | firmware nello slot sbagliato | non usare `idf.py flash`; ricarica dal recovery |

## 11. Cosa non e provato

- La H2 non si compila ancora.
- Compilazione su Linux e Windows: non provata dall'autore.
- Il sonno leggero e sospeso nella 1.8.30k: lo standby usa il sonno profondo.
