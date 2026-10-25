// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sess.cpp
// Tabella delle sessioni della shell (vedi vos_shell_sess.h). Il blocco dura solo il tempo di una copia.
#include "vos_shell_sess.h"
#include "../core/vos_lock.h"
#include "../drivers/vos_drv_time.h"
#include <string.h>

namespace {

struct Slot {
  uint32_t  id = SESS_NONE;              // SESS_NONE = posto libero
  uint32_t  last = 0;                    // drvMillis dell'ultimo uso
  ShellSess s;
};

Slot g_slot[SESS_MAX];

VosMutex& mux() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

bool expired(const Slot& e, uint32_t now) { return (uint32_t)(now - e.last) > SESS_IDLE_MS; }

// Il posto della sessione id, o nullptr. Una sessione scaduta si libera e non conta.
Slot* find(uint32_t id, uint32_t now) {
  for (Slot& e : g_slot) {
    if (e.id != id) continue;
    if (expired(e, now)) { e = Slot(); return nullptr; }
    return &e;
  }
  return nullptr;
}

}  // namespace

void shellSessLoad(uint32_t id, ShellSess& s) {
  s = ShellSess();
  if (id == SESS_NONE) return;
  VosLock lk(mux());
  Slot* e = find(id, (uint32_t)drvMillis());
  if (e) s = e->s;
}

void shellSessStore(uint32_t id, const ShellSess& s) {
  if (id == SESS_NONE) return;
  VosLock lk(mux());
  uint32_t now = (uint32_t)drvMillis();
  Slot* e = find(id, now);
  if (!e) {                                                      // posto libero, altrimenti quello usato meno di recente
    for (Slot& c : g_slot) if (c.id == SESS_NONE) { e = &c; break; }
    if (!e) {
      e = &g_slot[0];
      for (Slot& c : g_slot) if ((uint32_t)(now - c.last) > (uint32_t)(now - e->last)) e = &c;
    }
  }
  e->id = id;
  e->last = now;
  e->s = s;
}

void shellSessDrop(uint32_t id) {
  if (id == SESS_NONE) return;
  VosLock lk(mux());
  for (Slot& e : g_slot) if (e.id == id) e = Slot();
}
