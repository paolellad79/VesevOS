// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_prove.cpp
// Prove dalla pagina: impulso (si spegne da solo) e lampeggio (solo led e pin: il rele non si fa lampeggiare, si consuma).
// Spostate qui da vos_io.cpp per tenerlo sotto le 500 righe: stesso comportamento.
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_time.h"

static bool eUscita(const IoDev& d) { return d.cls == IC_RELE || d.cls == IC_LED || d.cls == IC_PIN; }

bool ioPulse(const char* name, uint32_t ms, int role, ErrBuf& err) {
  IoDev* d = ioDevPerNome(name);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (ms < 100 || ms > 5000) { err = tr("Valori non validi"); return false; }
  if (!eUscita(*d)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (!ioSet(name, 1, role, err)) return false;
  d->offAt = drvMillis() + ms;
  if (!d->offAt) d->offAt = 1;
  return true;
}

bool ioBlink(const char* name, uint32_t sec, int role, ErrBuf& err) {
  IoDev* d = ioDevPerNome(name);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (d->cls == IC_RELE) { err = tr("Il rele non si fa lampeggiare"); return false; }
  if (!eUscita(*d)) { err = tr("Questa periferica non e un'uscita"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (sec < 1 || sec > 10) { err = tr("Valori non validi"); return false; }
  d->offAt = 0;
  d->blinkEnd = drvMillis() + sec * 1000UL;
  if (!d->blinkEnd) d->blinkEnd = 1;
  d->blinkNext = drvMillis();
  return true;
}
