// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ramwd.h
// Watchdog della RAM in percentuale con risposta graduata (segnala, riduci, critico). La regola pura sta in vos_ramwd_rule.h.
// Qui: misura della RAM interna, allarme, passi di riduzione e "lavori in corso" (niente riavvio per RAM durante aggiornamenti,
// caricamenti, formattazione della microSD).
#pragma once
#include <stdint.h>
#include "vos_ramwd_rule.h"
#include "vos_heapbuf.h"

// Lavori che non vanno interrotti da un riavvio per RAM. Contatore (un lavoro con inizio e fine) o scadenza (un caricamento a pezzi: ogni pezzo la rinnova).
void ramBusyInc();
void ramBusyDec();
void ramBusyTouch(uint32_t holdSec);   // "c'e un lavoro in corso per altri holdSec secondi"
bool ramBusy();
struct RamBusy {                       // da mettere all'inizio di un lavoro: finisce da solo all'uscita
  RamBusy() { ramBusyInc(); }
  ~RamBusy() { ramBusyDec(); }
  RamBusy(const RamBusy&) = delete;
  RamBusy& operator=(const RamBusy&) = delete;
};

bool        ramWdTick();               // una volta al secondo (dal task "wd"); true = il riavvio per RAM e ammesso adesso
RamLvl      ramWdLevel();              // livello in vigore
int         ramWdUsed();               // % di RAM interna usata all'ultimo giro (-1 prima del primo)
const char* ramWdOff();                // cosa e stato spento dai passi di riduzione ("mesh, mqtt"), "" se niente
bool        ramWdPoca();               // la RAM e al livello "riduci" o peggio: chi puo, risparmia memoria
