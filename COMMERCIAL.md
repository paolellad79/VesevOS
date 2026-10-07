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

VesevOS usa librerie con licenze proprie (elenco completo in `NOTICE.txt` e `SBOM.spdx.json`):
PsychicHttp e ArduinoJson (MIT), ESP-IDF, Mbed TLS e NimBLE (Apache-2.0), FreeRTOS (MIT), lwIP e LittleFS
(BSD-3-Clause), core Arduino-ESP32 (LGPL-2.1 o successiva). Restano sotto la loro licenza e non fanno parte
della licenza commerciale di VesevOS. Chi distribuisce un firmware compilato deve rispettarle.

## Kit LGPL (per chi vende prodotti chiusi con la licenza commerciale)

Il core Arduino-ESP32 e LGPL-2.1: chi riceve il prodotto deve poter ricollegare il programma con una versione
modificata del core. Con la licenza commerciale si consegna, a richiesta del cliente:

1. i file oggetto (`.o`) o l'archivio (`.a`) del programma VesevOS compilato, **senza** il codice sorgente di VesevOS;
2. il codice sorgente del core Arduino-ESP32 usato (versione esatta) e le istruzioni per compilarlo;
3. le istruzioni per ricollegare (link) e caricare il firmware sulla scheda (Arduino IDE o `arduino-cli`);
4. i testi delle licenze (`license lgpl21` e gli altri in `licenses/`).

Il kit si prepara con la stessa versione di Arduino IDE e del core usata per il prodotto; va conservato
per tutto il periodo in cui il prodotto e in vendita. Non e consulenza legale: verifica con un esperto.

## Titolare e contatto
Titolare dei diritti: Domenico Paolella - paolellad79@gmail.com
