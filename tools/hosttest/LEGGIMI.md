# Prove sul computer (senza scheda)

Compilano alcune parti del firmware sul PC con finti pezzi della scheda e controllano che facciano quello che devono.
Non servono la scheda ne l'Arduino IDE. Non vanno mai dentro il firmware.

Uso: `tools/hosttest/run.sh` (serve g++ e il nucleo Arduino-ESP32 in `$CORE` (da impostare, obbligatorio);
da quel nucleo si copiano solo `WString.cpp`, `Print.cpp`, `Stream.cpp`, che restano sotto LGPL-2.1 e NON sono nel repository).

Cosa provano oggi
- `test_util.cpp`: indirizzi IP, maschere, tempo acceso, testo pulito, SHA-256 (valori noti), numeri casuali, nomi di scheda e dominio.
- `test_config.cpp`: la configurazione esce e rientra uguale, i valori fuori misura vengono limitati, le password non finiscono nel testo pubblico.
- `test_log.cpp`: i livelli [E][W][I][D] del registro (anche con testo scelto dall'utente a meta riga), il filtro "registra fino a", i caratteri strani.
- `test_serial.cpp`: le impostazioni della seriale (a capo, eco, comandi, registro, benvenuto, attesa, velocita in prova e conferma).

Come aggiungere una prova: nuovo file `test_<nome>.cpp` con `CHECK(...)` e `return done("nome")`; se serve una parte nuova del firmware, aggiungerla all'elenco in `run.sh`.
Regola dello standard v2.0: ogni bug corretto genera una prova che non lo faccia tornare.

Limiti (da dire chiaramente): si provano solo le parti senza hardware; Wi-Fi, Bluetooth, USB e RAM vera si provano solo sulla scheda.
Le funzioni `isAlphaNumeric` ecc. nel finto `Arduino.h` erano sempre vere: corrette il 7/10, ora fanno il lavoro vero.

Prove che NON usano Arduino (script `.sh`, compilano i file veri con pochi finti; ognuna si lancia da sola)
- `prova_prog.sh`: Funzionalita (`vos_func`), gruppi di primitive che un'app ELF chiede (`vos_elf_use`, su un ELF costruito a mano) e blocco delle funzionalita tolte nel registro (`vos_prim`).
- `prova_cfgprog.sh`: configurazione di sottosistemi (vdb, vde), Funzionalita tolte e programmi con avvio automatico: valori di fabbrica, esporta/importa, id sconosciuti, elenco troppo lungo, file vecchio.
- `prova_sdreg.sh`, `prova_berryhw.sh`: funzioni Berry `sd_*` e `gpio_*`, `i2c_*`, `spi_*` (permessi, ruolo e funzionalita tolta). Altre: `prova_prim.sh`, `prova_elfrun.sh`, `prova_vde.sh`, `prova_hw.sh`, `prova_sdauto.sh`, `prova_drvres.sh`, `prova_vdp_cmd.sh`, `prova_wrap.sh`.
