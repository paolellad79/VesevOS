// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_contatore.h
// STRATO 1 (driver): unico file che usa il contatore di impulsi (PCNT, driver/pulse_cnt.h). Una unita per ogni contatore,
// un solo pin di ingresso, conteggio a 32 bit (il driver di IDF somma ogni giro dei 16 bit: accum_count). Non usa gli
// interrupt di "step" (errata H2 PCNT-249). Il controllo dei pin (vietati, occupati) NON e qui: lo fa il registro dei pin.
// Esiste solo sui chip con PCNT (SOC_PCNT_SUPPORTED); il chip senza (C3) non ha questo codice e il suo file di scheda tiene il flag a 0.
// Le funzioni non usate non entrano nel binario (il linker le scarta).
#pragma once
#include <stdint.h>

#define DRV_CONT_FILTRO_MAX 10000    // ns: sotto il massimo di tutti i chip (1023 cicli APB: 12,7 us a 80 MHz)

#define DRV_CONT_FRONTE_SALITA   0
#define DRV_CONT_FRONTE_DISCESA  1
#define DRV_CONT_FRONTE_ENTRAMBI 2

#define DRV_CONT_PULL_NESSUNO 0
#define DRV_CONT_PULL_SU      1
#define DRV_CONT_PULL_GIU     2

// Codici di errore di drvContatoreApri (negativi). I testi li scrive chi chiama, con tr().
#define DRV_CONT_E_UNITA  (-1)    // nessuna unita libera
#define DRV_CONT_E_FILTRO (-2)    // filtro fuori misura
#define DRV_CONT_E_HW     (-3)    // il blocco ha rifiutato la configurazione

// Prenota una unita sul pin e comincia a contare da 0. filtroNs 0 = nessun filtro. Ritorna il numero dell'unita (>= 0) o un codice DRV_CONT_E_*.
int drvContatoreApri(int gpio, uint8_t fronte, uint16_t filtroNs, uint8_t pull);

uint32_t drvContatoreLeggi(int h);     // impulsi contati (torna a 0 dopo 2^32)
void     drvContatoreAzzera(int h);
void     drvContatoreChiudi(int h);    // ferma e libera l'unita

int drvContatoreLiberi();              // unita ancora libere e totali del chip
int drvContatoreTotali();
