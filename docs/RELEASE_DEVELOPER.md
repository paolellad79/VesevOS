# VesevOS - Release Developer (guida per chi sviluppa e rilascia)

Ultimo aggiornamento: 10 ottobre 2026, versione 1.7.45. Si pubblica anche su GitHub (docs/). La procedura passo-passo sta nella checklist di rilascio (claude/VesevOS-checklist-rilascio.md nel Progetto); qui c'e il quadro per chi lavora sul codice.

## 1. Ambiente
- Scheda: ESP32-S3 SuperMini (4 MB flash, 2 MB PSRAM, USB nativa, LED WS2812 su GPIO48, solo antenna interna).
- Arduino IDE, core Arduino-ESP32 3.3.x, partizione "Huge APP" (solo per il compilatore; la tabella vera e in recovery/VesevOS_Recovery/partitions.csv: recovery 1 MB + app0 2,31 MB). Rilascio: oltre a VesevOS/ si pubblica recovery/ (sketch, script, LEGGIMI).
- Librerie: PsychicHttp 3.1.2, ArduinoJson 7 (altro: mbedTLS e ESP-IDF dal core).
- Licenza: GPL-3.0-or-later oppure licenza commerciale (COMMERCIAL.md). Il core Arduino (LGPL) non si distribuisce con il repo.

## 2. Struttura
- `VesevOS/` firmware: `VesevOS.ino` + `src/` con una cartella per strato: `core/` nucleo, `security/` accessi e cifratura, `net/` rete, `devices/` Device Manager e pin/LED, `drivers/` hardware e librerie radio (`vos_drv_*`), `packages/` servizi opzionali, `interfaces/` web e shell. Gli include sono relativi al file. `vos_page.h`, `vos_common_data.h`, `vos_lang_en.h` e `vos_license_data.h` sono GENERATI (in `src/interfaces/` e `src/core/`).
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
2. Compilare TUTTI i file con gli stub (g++ -c) e cercare funzioni doppie (nm).
3. `tools/hosttest/run.sh` (serve `$CORE` = cartella `cores/esp32` del core Arduino). Oggi 162 prove. `python3 tools/checklayers.py` deve finire senza KO.
4. Pagina: `tools/webtest/server.js` + `smoke172.js` -> "TUTTO OK".
5. Compilazione vera e prova sulla scheda: la fa il proprietario.

## 5. Versioni
- Funzioni nuove = numero nuovo (1.7.45). Solo correzioni di bug = lettera (1.7.9b).
- La versione va in: `vos_common.h`, `VesevOS.ino`, nome ZIP, CHANGELOG, README it/en, SBOM, `mkcommon.py`, LEGGIMI, manuali.
- Rifacimento di architettura: un passo = una versione, mai due strati insieme.

## 6. Rilascio
- Prima il semaforo legale e l'"ok" del proprietario; poi i due ZIP: `VesevOS-X.Y.Z.zip` (da compilare) e `VesevOS-repo.zip` (per GitHub).
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
