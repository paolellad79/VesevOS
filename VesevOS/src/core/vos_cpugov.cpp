// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpugov.cpp (governor: vedi vos_cpugov.h)
#include <string.h>
#include "vos_cpugov.h"

bool cpuGovParamsValid(int up, int down, int hold) {
  return up >= 50 && up <= 95 && down >= 5 && down <= 50 && down < up - 10 && hold >= 2 && hold <= 120;
}

const char* cpuGovName(CpuGov gov) {
  return gov == GOV_CONSERVATIVE ? "conservative" : gov == GOV_SCHEDUTIL ? "schedutil" : "ondemand";
}

bool cpuGovParse(const char* text, CpuGov& gov) {
  if (!text) return false;
  if (!strcmp(text, "ondemand"))     { gov = GOV_ONDEMAND; return true; }
  if (!strcmp(text, "conservative")) { gov = GOV_CONSERVATIVE; return true; }
  if (!strcmp(text, "schedutil"))    { gov = GOV_SCHEDUTIL; return true; }
  return false;
}

uint32_t cpuGovEquiv240(uint32_t busyPct, uint32_t curMhz) {
  if (busyPct > 100) busyPct = 100;
  return (busyPct * curMhz + 120) / 240;
}

// Il primo livello valido che arriva a mhz (arrotonda sempre per eccesso).
static uint32_t levelAtLeast(uint32_t mhz) { return mhz <= 80 ? 80 : mhz <= 160 ? 160 : 240; }
static uint32_t levelUp(uint32_t cap)      { return cap < 160 ? 160 : 240; }
static uint32_t levelDown(uint32_t cap)    { return cap > 160 ? 160 : 80; }

static void setCap(CpuGovState& st, uint32_t cap, CpuGovWhy why, uint32_t now) {
  st.cap = cap; st.why = why; st.lastChange = now; st.below = false;
}

// ondemand: sopra la soglia di salita va subito al massimo; sotto, sceglie il livello a cui il lavoro di adesso
// peserebbe (soglia - 10)%. schedutil: il livello che copre il lavoro di adesso piu un quarto di margine.
static uint32_t levelWanted(CpuGov gov, const CpuGovParams& p, const CpuGovInput& in) {
  uint32_t u = cpuGovEquiv240(in.busyPct, in.curMhz);
  if (gov == GOV_SCHEDUTIL) return levelAtLeast(u * 3);             // 1,25 x 240 / 100 = 3 MHz per ogni % di lavoro
  if (in.busyPct > p.up) return 240;
  return levelAtLeast(u * 240 / (p.up - 10));
}

// Sale subito (salvo il minimo fra due cambi); scende solo dopo hold secondi di fila a livello piu basso, al piu alto visto nel frattempo.
static uint32_t followWanted(const CpuGovParams& p, CpuGovState& st, uint32_t want, uint32_t now) {
  bool gapOk = now - st.lastChange >= CPUGOV_MIN_GAP_SEC;
  if (want >= st.cap) {
    st.below = false;
    if (want > st.cap && gapOk) setCap(st, want, GW_UP, now);
    return st.cap;
  }
  if (!st.below) { st.below = true; st.belowSince = now; st.peak = want; }
  else if (want > st.peak) st.peak = want;
  if (now - st.belowSince >= p.hold && gapOk) setCap(st, st.peak, GW_DOWN, now);
  return st.cap;
}

// conservative: un gradino alla volta, su sopra up, giu sotto down dopo hold secondi.
static uint32_t stepWanted(const CpuGovParams& p, CpuGovState& st, const CpuGovInput& in) {
  uint32_t now = in.nowSec;
  bool gapOk = now - st.lastChange >= CPUGOV_MIN_GAP_SEC;
  if (in.busyPct > p.up) {
    st.below = false;
    if (st.cap < 240 && gapOk) setCap(st, levelUp(st.cap), GW_UP, now);
  } else if (in.busyPct < p.down && st.cap > 80) {
    if (!st.below) { st.below = true; st.belowSince = now; }
    if (now - st.belowSince >= p.hold && gapOk) setCap(st, levelDown(st.cap), GW_DOWN, now);
  } else {
    st.below = false;
  }
  return st.cap;
}

uint32_t cpuGovDecide(CpuGov gov, const CpuGovParams& p, CpuGovState& st, const CpuGovInput& in) {
  uint32_t now = in.nowSec;
  if (in.boost) {                                    // prenotazione: massimo subito, senza aspettare
    if (st.cap != 240) setCap(st, 240, GW_BOOST, now);
    else if (st.why != GW_BOOST) st.why = GW_BOOST;
    st.below = false;
    return st.cap;
  }
  if (st.why == GW_BOOST) st.why = GW_NONE;          // la prenotazione e finita
  if (st.cap == 0) {                                 // prima decisione: nessun tempo da aspettare
    uint32_t first = gov == GOV_CONSERVATIVE ? 160 : levelWanted(gov, p, in);
    setCap(st, first, GW_NONE, now);
    return st.cap;
  }
  if (gov == GOV_CONSERVATIVE) return stepWanted(p, st, in);
  return followWanted(p, st, levelWanted(gov, p, in), now);
}
