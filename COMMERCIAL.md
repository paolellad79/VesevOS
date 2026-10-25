# Licenza commerciale di VesevOS

VesevOS e disponibile con **due licenze**, a tua scelta:

1. **GNU GPL v3 o successiva** (file `LICENSE`): gratuita. Puoi usare, studiare e modificare VesevOS
   anche in azienda, ma se distribuisci il programma (anche modificato, anche dentro un prodotto
   o un firmware su una scheda venduta) devi pubblicare il tuo codice sorgente completo con la
   stessa licenza GPL.
2. **Licenza commerciale** (a pagamento, su richiesta): se vuoi usare VesevOS in un prodotto
   chiuso, senza pubblicare il tuo codice, serve un accordo scritto con l'autore.

## Come ottenere la licenza commerciale

Scrivi all'indirizzo indicato in `NOTICE.txt` (oppure apri una segnalazione nel repository). Indica: nome dell'azienda, prodotto, quantita
prevista.

## Per chi contribuisce con codice

Per poter continuare a offrire la doppia licenza, i contributi di terzi vengono accettati solo
se l'autore del contributo concede all'autore del progetto il diritto di distribuirlo anche con
licenza commerciale. Vedi `CONTRIBUTING.md`.

## Librerie di terzi

VesevOS e compilato con ESP-IDF 5.5.5 e usa componenti con licenze proprie (elenco completo con versioni in
`NOTICE.txt` e `SBOM.spdx.json`):

- Apache-2.0: ESP-IDF, Mbed TLS (scelta Apache-2.0 tra le due licenze offerte), NimBLE, esp-nimble-cpp, espressif/mdns.
- MIT: PsychicHttp (copia modificata), ArduinoJson, FreeRTOS, esp_littlefs, Berry, posix_tz_db (da verificare).
- BSD-3-Clause: lwIP, LittleFS. Libreria C (newlib): licenze miste di tipo BSD (da verificare).
- Dati: elenco dei certificati radice di Mozilla (di solito MPL-2.0, da verificare), Natural Earth e IANA tz (pubblico dominio).

Restano sotto la loro licenza e non fanno parte della licenza commerciale di VesevOS. Chi distribuisce un firmware
compilato deve conservare gli avvisi di copyright e i testi delle licenze (nel firmware: comando `license`).

## Il prodotto non include codice LGPL

Il firmware non usa piu il core Arduino-ESP32 (LGPL-2.1) e non contiene altro codice LGPL. Per questo non serve piu
il "kit di ricollegamento" (file oggetto, istruzioni di link) che serviva per la LGPL. Con la licenza commerciale
restano da rispettare: la licenza commerciale stessa (o la GPL-3.0-or-later per chi sceglie la GPL) e gli avvisi
dei componenti Apache-2.0, MIT, BSD-3-Clause e MPL-2.0 elencati sopra. Se in futuro si aggiunge un componente
LGPL o GPL, questo punto va riesaminato prima del rilascio.

## Da far verificare a un professionista

Questa e una valutazione preliminare, non e consulenza legale; le norme possono essere cambiate. Prima di vendere
con la licenza commerciale conviene far controllare da un legale:

1. la compatibilita tra GPL-3.0-or-later / licenza commerciale e i componenti Apache-2.0 (Apache-2.0 con GPL-3.0 e
   in genere compatibile, ma la doppia licenza va confermata);
2. la licenza dei dati del bundle dei certificati radice (Mozilla, MPL-2.0?) e di `posix_tz_db` (MIT?), non confermate
   nei sorgenti locali;
3. le licenze miste della libreria C (newlib);
4. marchi, marcatura CE (radio, EN 18031), Cyber Resilience Act, esportazione e privacy per il prodotto finito.

## Titolare e contatto
Titolare dei diritti: Domenico Paolella - paolellad79@gmail.com
