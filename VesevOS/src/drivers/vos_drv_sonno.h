// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sonno.h
// STRATO 1 (driver): sonno leggero e profondo con risveglio a tempo e, se armato, dal tasto di ripresa.
// Prima di chiamarle il chiamante spegne Wi-Fi e Bluetooth (le radio non reggono il sonno, guida ESP-IDF sleep_modes) e scrive i dati in sospeso.
#pragma once
#include <stdint.h>

// La ripresa si presenta al sonno quando viene preparata e si ritira quando viene tolta: pin < 0 = nessun tasto. alto = risveglia a livello alto.
void drvSonnoRipresa(int pin, bool alto);
bool drvSonnoRipresaPresente();     // c'e un tasto di ripresa? E' tutto quello che lo standby vuole sapere: il piedino non lo vede


// Com'e andato un sonno leggero (per il registro: chi lo chiede scrive la riga). codice: 0 ok, 1 rifiutato (un risveglio era gia attivo), 2 troppo breve, 3 altro errore.
// livelloPrima: livello del tasto di ripresa prima di dormire (0, 1, -1 se non c'e). causa: SONNO_CAUSA_*. durataMs: da quando e partito a quando e tornato.
enum DrvSonnoCausa { SONNO_CAUSA_NESSUNA = 0, SONNO_CAUSA_TIMER, SONNO_CAUSA_PIEDINO, SONNO_CAUSA_ALTRA };
struct DrvSonnoEsito { int codice; int livelloPrima; int causa; uint32_t durataMs; };

bool drvSonnoRipresaPremuta();      // il tasto di ripresa e GIA al livello di risveglio adesso? (false anche se il tasto non c'e)

// Sonno leggero: la CPU si ferma e riparte da dove era. sec = 0 senza timer. esito dice com'e andato.
// Ritorna true se ha dormito, false se il sonno e stato rifiutato o non e partito (guarda esito.codice).
bool drvSonnoLeggero(uint32_t sec, DrvSonnoEsito& esito);

// Sonno profondo: non ritorna. La scheda riparte da capo dal tempo o dal tasto. sec = 0 senza timer.
// conRipresa = false: ignora il tasto di ripresa (il vecchio suspend/poweroff si riaccende solo con il tempo o con RESET).
void drvSonnoProfondo(uint32_t sec, bool conRipresa = true);

bool drvSonnoRisvegliatoDaTimer();   // l'ultimo risveglio dal sonno profondo e stato il timer?
