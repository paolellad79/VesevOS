// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_led.h
// STRATO 1 (driver): LED RGB WS2812 con il periferico RMT di ESP-IDF (codice nostro, nessun componente esterno).
// Due usi, stessi tempi e stesso codice di invio:
//  1) LED di bordo: drvLedWrite, UN solo pin (cambiabile), va chiamato da UN solo task (quello del LED). Ordine dei byte = VOS_LED_ORDER della scheda.
//  2) LED a mano dell'utente (un LED = un pixel): drvLedApri / drvLedScrivi / drvLedChiudi, un elenco di canali RMT. Ogni LED ha il suo pin e il suo
//     ordine dei byte. Chi chiama serializza gli accessi (vos_io usa il suo blocco). Il controllo dei pin NON e qui: lo fa il registro dei pin.
#pragma once
#include <stdint.h>

void drvLedWrite(int gpio, uint8_t r, uint8_t g, uint8_t b);   // colore 0..255 per canale (l'ordine dei fili del WS2812 e gestito qui)
bool drvLedAbsent();   // true se il LED non si apre dopo i tentativi (il pin non esiste o manca): non si riprova piu

// Codici di errore di drvLedApri (numeri negativi). I testi li scrive chi chiama, con tr().
#define DRV_LED_E_CANALI (-1)    // nessun canale RMT libero
#define DRV_LED_E_HW     (-2)    // il periferico ha rifiutato la configurazione (pin, memoria)

// Ordine dei byte: i numeri sono VOS_LED_ORD_* di boards/vos_board.h (0 = GRB ... 5 = BGR).
#define DRV_LED_ORD_MAX 5

// Apre un canale sul pin con l'ordine dei byte dato. Ritorna il numero del canale (>= 0) oppure un codice DRV_LED_E_*. La linea resta bassa.
int  drvLedApri(int gpio, uint8_t ordine);
void drvLedScrivi(int h, uint8_t r, uint8_t g, uint8_t b);     // colore 0..255 per canale (la luminosita la applica chi chiama)
void drvLedOrdine(int h, uint8_t ordine);                      // cambia l'ordine dei byte di un canale aperto
void drvLedChiudi(int h);                                      // libera il canale (il pin resta basso)
int  drvLedLiberi();                                           // canali RMT di trasmissione ancora liberi per i LED a mano
