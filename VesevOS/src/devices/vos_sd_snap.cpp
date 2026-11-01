// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_snap.cpp: FOTOGRAFIA dello stato della microSD per il widget della Home. La scrive il task della microSD (ogni 2-3 s, a priorita
// bassa), la Home la legge senza toccare la scheda ne l'interprete: niente codice a ogni richiesta, niente attesa. Lo spazio (dimensione e usato)
// costa una lettura della FAT: si ricalcola appena la scheda si monta e poi al massimo una volta al minuto, e mai con la scheda in uso.
#include "vos_sd.h"

#if VOS_WITH_SD
#include <string.h>
#include "../drivers/vos_drv_time.h"

#define SNAP_SPACE_MS 60000UL

static SdVolume g_snap;
static bool g_have = false;
static volatile uint32_t g_seq = 0;            // dispari mentre si scrive: chi legge riprova
static bool g_wasMounted = false;
static uint32_t g_spaceAt = 0;

static void publish(const SdVolume& v, bool have) {
  __atomic_add_fetch(&g_seq, 1, __ATOMIC_ACQ_REL);
  g_snap = v;
  g_have = have;
  __atomic_add_fetch(&g_seq, 1, __ATOMIC_ACQ_REL);
}

bool sdSnapshot(SdVolume& v) {
  for (int i = 0; i < 4; i++) {
    uint32_t a = __atomic_load_n(&g_seq, __ATOMIC_ACQUIRE);
    if (a & 1) continue;
    v = g_snap;
    bool have = g_have;
    if (a == __atomic_load_n(&g_seq, __ATOMIC_ACQUIRE)) return have;
  }
  return false;                                // scrittura in corso: la Home riprova alla prossima richiesta
}

void sdSnapshotStep() {
  SdVolume v;
  memset(&v, 0, sizeof v);
  if (!sdConfigured()) { publish(v, false); g_wasMounted = false; return; }
  uint32_t now = (uint32_t)drvMillis();
  bool mounted = sdStateNow() == SD_ST_MOUNTED;
  bool due = mounted && (!g_wasMounted || now - g_spaceAt > SNAP_SPACE_MS);
  if (mounted && !due) {                       // stesso stato e spazio ancora valido
    SdVolume old;
    if (!sdSnapshot(old)) return;
    old.state = SD_ST_MOUNTED;
    publish(old, true);
    return;
  }
  if (mounted && sdInUse()) return;            // la FAT non si legge mentre un'altra operazione lavora: si riprova al prossimo giro
  if (!sdVolume(v)) { publish(v, false); g_wasMounted = false; return; }
  if (mounted) g_spaceAt = now;
  g_wasMounted = mounted;
  publish(v, true);
}
#endif
