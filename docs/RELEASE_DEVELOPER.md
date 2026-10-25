# VesevOS - Release Developer (guida per chi sviluppa e rilascia)

Ultimo aggiornamento: 25 ottobre 2026 (versione 1.8.0b; sezioni 1, 4, 5 e 6 per ESP-IDF; le altre non riviste dalla 1.7.45). Si pubblica anche su GitHub (docs/). La procedura passo-passo sta nella checklist di rilascio (documento interno); qui c'e il quadro per chi lavora sul codice.

## 1. Ambiente
- Scheda: ESP32-S3 SuperMini (4 MB flash, 2 MB PSRAM, USB nativa, LED WS2812 su GPIO48, solo antenna interna).
- Compilazione: ESP-IDF puro con `idf.py` (progetto `idf/` accanto al repository per VesevOS, `idf-recovery/` per il recovery; comandi nel README, sezione di compilazione). Niente Arduino IDE ne arduino-cli.
- Tabella partizioni (4 MB): `idf-recovery/partitions.csv` (progetto del recovery, accanto al repository): recovery 1 MB a 0x10000 + app0 a 0x110000 (2,31 MB) + spiffs a 0x360000. Un rilascio pubblica anche `recovery/` (sorgenti IDF, script, LEGGIMI).
- Librerie nel progetto (`idf/components`): PsychicHttp 3.1.2 (copia modificata) e aj_json (ArduinoJson 7.x). mbedTLS e il resto vengono da ESP-IDF.
- Licenza: GPL-3.0-or-later oppure licenza commerciale (COMMERCIAL.md). Il core Arduino (LGPL) non e piu nel prodotto (ESP-IDF puro dalla fase 5).

## 2. Struttura
- `VesevOS/` firmware: `src/` con una cartella per strato: `core/` nucleo, `security/` accessi e cifratura, `net/` rete, `devices/` Device Manager e pin/LED, `drivers/` hardware e librerie radio (`vos_drv_*`), `packages/` servizi opzionali, `interfaces/` web e shell. Gli include sono relativi al file. `vos_page.h`, `vos_common_data.h`, `vos_lang_en.h` e `vos_license_data.h` sono GENERATI (in `src/interfaces/` e `src/core/`).
- `web/index.html` pagina; `lang/` lingue generate.
- `tools/`: `lang_src.json` (traduzioni it/en/es/de), `mklang.py`, `mkpage.py`, `mklicense.py`, `mkcommon.py`.
- `tools/stub/`: finti header per compilare sul computer. `tools/webtest/`: finto server + Playwright. `tools/hosttest/`: prove sul computer (`run.sh`).
- `docs/`: manuali it/en, immagini, KNOWN_ISSUES (it/en), RELEASE_DEVELOPER (it/en).

## 3. Regole di codice
- Sorgenti firmware solo ASCII. Testi con `tr()`/`trf()` (firmware) e `t()`/`tf()` (pagina). Chiavi uniche; segnaposto uguali nelle lingue.
- Ogni rotta web ha il ruolo (guest/operatore/admin). Mai password, chiavi o codici nel log (lo legge anche il guest).
- Dati toccati da piu task: mutex o `portMUX`. Operazioni lunghe mai nel loopTask; watchdog alimentato.
- Memoria: attenzione alla frammentazione; prima di aprire TLS o BLE controllare il blocco libero piu grande (`ESP.getMaxAllocHeap()`).
- Limite 500 righe per file e nessuna duplicazione (regole del rifacimento, per gradi).
- STRATI (schema nel Progetto: `VesevOS-schema-strati.md`): driver (`vos_drv_*`, unici a toccare l'hardware o la libreria radio) -> Device Manager (`vos_dev`) -> nucleo -> package/servizi -> interfacce (web, shell) -> app. Ogni strato usa solo quello sotto. Un servizio chiede al driver e non include mai la libreria dell'hardware. `python3 tools/checklayers.py` conta gli accessi diretti fuori dai driver: il debito di partenza e in `tools/layers_baseline.json` e puo solo calare (BLE: gia a zero).

## 4. Come si costruisce e si prova
1. `python3 tools/mklicense.py` (se manca), `python3 tools/mklang.py` (deve finire con "ok"), `python3 tools/mkpage.py`.
2. Compilare TUTTI i file con gli stub (`tools/stub/compila.sh`, g++ -c) e cercare funzioni doppie (nm). Gli stub imitano ancora vecchi header: sono strumenti per le sole prove sul computer; la compilazione vera e `idf.py build`.
3. `tools/hosttest/run.sh` (serve `$CORE` = cartella `cores/esp32` del core Arduino-ESP32, usata solo come strumento delle prove sul computer: non entra nel prodotto e non e nel repository; `run.sh` dipende ancora da quel core, da riallineare). Oggi 162 prove. `python3 tools/checklayers.py` deve finire senza KO (controlla anche che Arduino non ci sia piu (dalla fase 5 la baseline deve essere vuota): `String`, `Serial`, `millis/micros/delay` e `#include <Arduino.h>` fuori da `vos_drv_*`; il debito di oggi sta in `tools/layers_baseline.json` e puo solo calare).
4. Pagina: `tools/webtest/server.js` + `smoke172.js` -> "TUTTO OK".
5. Compilazione vera (`idf.py -B <cartella> build`) e prova sulla scheda: la fa il proprietario.

## 5. Versioni
- Funzioni nuove = numero nuovo (1.7.45). Solo correzioni di bug = lettera (1.7.9b).
- La versione va in: `vos_common.h` (VOS_VERSION), nome ZIP, CHANGELOG, README it/en, SBOM, `mkcommon.py`, LEGGIMI, manuali.
- Rifacimento di architettura: un passo = una versione, mai due strati insieme.

## 6. Rilascio
- Prima il semaforo legale (Riepilogo + semaforo in un messaggio, regole in `docs/progetto/REGOLE-LEGALI.md`) e l'"ok" del proprietario. Poi: registro legale, `NOTICE.txt` (si genera da `licenses/NOTICE.template.txt` con `python3 tools/mklicense.py`: non si modifica a mano), SBOM (`SBOM.spdx.json`) e CHANGELOG; poi i due ZIP: `VesevOS-X.Y.Z.zip` (sorgenti, lingue, NOTICE e LEGGIMI) e `VesevOS-repo.zip` (per GitHub). Il repo pubblicato porta anche `firmware/` (file compilati con `idf.py`, SHA-256 e `indice.json`: ricalcola gli SHA con `shasum -a 256` dai file copiati) e `flash/` (pagina di prima installazione). Il file `.elf` non si pubblica mai.
- Commit: titolo `X.Y.Z - ...`, descrizione, riga `Controllo legale: verde/giallo/rosso`.
- Il push lo fa il proprietario. Dopo: copia nel Progetto, STATO e da-fare aggiornati.
- I documenti si pubblicano SEMPRE in italiano e in inglese (file .en.md).

## 7. Diagnostica utile
- Seriale: `help`, `ver`, `free`, `free detail`, `reboots` (diario), `log 30`, `serial`, `cert`, `selftest`.
- Il diario dei riavvii registra la fase (`L:start`, `L:shell`, `L:ble`, ... `L:wdt`) in cui la scheda si e fermata.

## 8. Cosa non si fa
Niente MapSVG, exFAT, password di default uguali per tutti, servizi senza controllo accessi, librerie senza licenza chiara o incompatibili con GPL-3.0 + commerciale, dati personali fuori senza consenso, canali o potenza oltre il paese.

## 9. Documenti collegati
- Problemi noti: `docs/KNOWN_ISSUES.md` (italiano), `docs/KNOWN_ISSUES.en.md` (inglese).
- Progetto (non pubblicati): checklist di rilascio, semaforo, registro legale, piano giallo-verde, piano email, da-fare, ripartenza.
