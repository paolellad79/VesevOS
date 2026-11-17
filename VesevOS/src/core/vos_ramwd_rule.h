// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ramwd_rule.h
// Regola del watchdog della RAM in PERCENTUALE, senza dipendenze (cosi si prova sul computer).
// Misura: RAM INTERNA usata (la PSRAM non conta). Livelli: segnala (75%), riduci (85%, mai oltre critico-2), critico (cfg.wdRamPerc, di fabbrica 93).
// Un solo passo al secondo: ramWdStep() decide, chi la chiama esegue (log, allarme, spegnimenti, riavvio).
#pragma once
#include <stdint.h>

enum RamLvl : uint8_t { RAML_OK = 0, RAML_SEGNALA = 1, RAML_RIDUCI = 2, RAML_CRITICO = 3 };

static const uint8_t  RAMWD_SEGNALA   = 75;    // regola dell'utente: con carico mai oltre il 75%
static const uint8_t  RAMWD_RIDUCI    = 85;
static const uint8_t  RAMWD_CRIT_DEF  = 93;    // equivale ai 20 KB fissi di prima sulle schede buone
static const uint8_t  RAMWD_CRIT_MIN  = 80;
static const uint8_t  RAMWD_CRIT_MAX  = 99;
static const uint8_t  RAMWD_ISTERESI  = 3;     // per scendere di livello servono 3 punti in meno
static const uint32_t RAMWD_SU_S      = 60;    // sopra soglia per tanti secondi prima di salire di livello
static const uint32_t RAMWD_GIU_S     = 30;    // sotto soglia-isteresi per tanti secondi prima di scendere
static const uint32_t RAMWD_PASSO_S   = 300;   // tra un passo "riduci" e il successivo (a livello riduci)
static const uint32_t RAMWD_PASSO_CRIT_S = 30; // idem a livello critico: si fanno tutti i passi presto
static const uint32_t RAMWD_RIAVVIO_S = 300;   // critico da tanti secondi, passi finiti: il riavvio e ammesso
static const uint32_t RAMWD_NORMALE_S = 120;   // dopo tanti secondi di vita, con RAM sotto il 75%, la scheda "era normale"
static const uint8_t  RAMWD_PASSI     = 5;     // passi di riduzione (vedi vos_ramwd.cpp)

static inline uint8_t ramWdClampCrit(int v) {
  return (uint8_t)(v < RAMWD_CRIT_MIN ? RAMWD_CRIT_MIN : v > RAMWD_CRIT_MAX ? RAMWD_CRIT_MAX : v);
}

// Percentuale di RAM usata (0-100) dato totale e libera interni
static inline int ramUsedPerc(uint32_t total, uint32_t freeB) {
  if (!total) return 0;
  if (freeB > total) freeB = total;
  return (int)(100 - (100ULL * freeB) / total);
}

// Valore equivalente alla vecchia soglia fissa in KB (chiave `ramkb`): percentuale usata con la RAM libera = kb, arrotondata
static inline uint8_t ramPercDaKb(uint32_t kb, uint32_t total) {
  if (!total) return RAMWD_CRIT_DEF;
  uint64_t freeB = (uint64_t)kb * 1024ULL;
  if (freeB > total) freeB = total;
  uint32_t usedX100 = (uint32_t)(10000ULL - (10000ULL * freeB) / total);   // percentuale con due decimali
  return ramWdClampCrit((int)((usedX100 + 50) / 100));
}

// soglie dei quattro livelli: [0] non usata, [1] segnala, [2] riduci, [3] critico
static inline void ramWdSoglie(uint8_t crit, uint8_t out[4]) {
  crit = ramWdClampCrit(crit);
  out[0] = 0;
  out[1] = RAMWD_SEGNALA;
  out[2] = (uint8_t)(crit - 2 < RAMWD_RIDUCI ? crit - 2 : RAMWD_RIDUCI);
  out[3] = crit;
}

// livello "grezzo": il piu alto la cui soglia e raggiunta
static inline RamLvl ramWdGrezzo(int used, const uint8_t sg[4]) {
  if (used >= sg[3]) return RAML_CRITICO;
  if (used >= sg[2]) return RAML_RIDUCI;
  if (used >= sg[1]) return RAML_SEGNALA;
  return RAML_OK;
}

struct RamWdCtx {
  RamLvl   lvl = RAML_OK;
  uint32_t su = 0, giu = 0;        // da quando (tempo+1) il livello grezzo e sopra / sotto quello in vigore; 0 = no
  uint32_t ultimoPasso = 0;        // tempo+1 dell'ultimo passo di riduzione; 0 = nessuno
  uint8_t  passi = 0;              // passi fatti
  uint32_t critDa = 0;             // tempo+1 da cui e critico senza interruzioni (e senza lavori in corso)
  bool     normale = false;        // dopo l'avvio la RAM e stata sotto il 75%: se oggi e critica, e SCESA
};

struct RamWdIn {
  int      used;                   // % usata (RAM interna)
  uint8_t  crit;                   // soglia critica scelta dall'utente
  uint32_t nowSec;                 // secondi dall'avvio
  bool     abilitato;              // cfg.wdRam
  bool     occupato;               // aggiornamento, caricamento, formattazione in corso
  bool     guida;                  // prima configurazione non finita
  bool     minima;                 // scheda con RAM strutturalmente bassa: mai riavvio per RAM
};

struct RamWdOut {
  bool    cambiato = false;        // il livello e cambiato
  bool    passo = false;           // fai il passo di riduzione numero nPasso
  uint8_t nPasso = 0;
  bool    riavvia = false;         // riavvio ammesso (passa comunque dall'antiloop del watchdog)
};

static inline RamWdOut ramWdStep(RamWdCtx& c, const RamWdIn& in) {
  RamWdOut o;
  if (!in.abilitato) { c = RamWdCtx(); return o; }
  uint32_t t = in.nowSec + 1;
  uint8_t sg[4]; ramWdSoglie(in.crit, sg);
  RamLvl raw = ramWdGrezzo(in.used, sg);
  if (raw > c.lvl) {
    c.giu = 0;
    if (!c.su) c.su = t;
    else if (t - c.su >= RAMWD_SU_S) { c.lvl = raw; c.su = 0; o.cambiato = true; }
  } else if (raw < c.lvl && in.used < sg[c.lvl] - RAMWD_ISTERESI) {
    c.su = 0;
    if (!c.giu) c.giu = t;
    else if (t - c.giu >= RAMWD_GIU_S) { c.lvl = raw; c.giu = 0; o.cambiato = true; }
  } else {
    c.su = 0; c.giu = 0;
  }
  if (o.cambiato && c.lvl == RAML_OK) { c.passi = 0; c.ultimoPasso = 0; }
  if (c.lvl == RAML_OK && in.nowSec >= RAMWD_NORMALE_S) c.normale = true;
  if (c.lvl >= RAML_RIDUCI && c.passi < RAMWD_PASSI) {
    uint32_t gap = c.lvl == RAML_CRITICO ? RAMWD_PASSO_CRIT_S : RAMWD_PASSO_S;
    if (!c.ultimoPasso || t - c.ultimoPasso >= gap) { o.passo = true; o.nPasso = c.passi; c.passi++; c.ultimoPasso = t; }
  }
  if (c.lvl != RAML_CRITICO || in.occupato || in.guida) c.critDa = 0;   // il conto riparte dopo i lavori in corso
  else if (!c.critDa) c.critDa = t;
  if (c.lvl == RAML_CRITICO && c.critDa && t - c.critDa >= RAMWD_RIAVVIO_S && c.passi >= RAMWD_PASSI &&
      c.normale && !in.minima && !in.occupato && !in.guida) {
    o.riavvia = true;
    c.critDa = t;                                                      // un solo riavvio per volta
  }
  return o;
}
