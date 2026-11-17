// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_contatore.cpp
// Classe "contatore" delle periferiche a classi (vedi vos_io.h): usa il driver generico drivers/vos_drv_contatore (blocco PCNT).
// Il controllo dei pin (vietati, occupati, doppi) resta uno solo, in vos_io: qui si gestiscono solo fronte, filtro, unita e frequenza.
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_time.h"
#include <freertos/FreeRTOS.h>

#if VOS_WITH_CONTATORE
static_assert(DRV_CONT_FILTRO_MAX == 10000, "adeguare il testo di tr() sul filtro del contatore");

namespace {
struct Lock {                                         // lo stesso blocco di vos_io (ricorsivo): come in vos_io_pwm.cpp
  bool held;
  Lock() : held(ioLockTake(pdMS_TO_TICKS(500))) {}
  ~Lock() { if (held) ioLockGive(); }
};

const char* errOf(int code) {
  if (code == DRV_CONT_E_UNITA) return tr("Contatori di impulsi finiti");
  if (code == DRV_CONT_E_FILTRO) return tr("Filtro del contatore: da 0 a 10000 ns");
  return tr("Il contatore di impulsi non si e aperto");
}

uint8_t pullOf(const IoDev& d) {
  return (d.flags & IOF_UP) ? DRV_CONT_PULL_SU : (d.flags & IOF_DOWN) ? DRV_CONT_PULL_GIU : DRV_CONT_PULL_NESSUNO;
}

void zero(IoDev& d) {
  d.cPrev = 0;
  d.cMhz = 0;
  d.cT = drvMillis();
}
}  // namespace

bool ioCntCfgOk(unsigned fronte, unsigned filtroNs) {
  return fronte <= DRV_CONT_FRONTE_ENTRAMBI && filtroNs <= DRV_CONT_FILTRO_MAX;
}

const char* ioCntOpen(IoDev& d) {
  if (d.cnt) return nullptr;
  int h = drvContatoreApri(d.gpio, d.addr, d.khz, pullOf(d));
  if (h < 0) return errOf(h);
  d.cnt = (uint8_t)(h + 1);
  zero(d);
  return nullptr;
}

void ioCntClose(IoDev& d) {
  if (!d.cnt) return;
  drvContatoreChiudi(d.cnt - 1);
  d.cnt = 0;
}

const char* ioCntStart(IoDev& d, const char* opt) {
  int fronte = ioOptNum(opt, "fronte=", DRV_CONT_FRONTE_SALITA), filtro = ioOptNum(opt, "filtro=", 0);
  if (fronte < 0 || fronte > DRV_CONT_FRONTE_ENTRAMBI) return tr("Fronte del contatore: 0 salita, 1 discesa, 2 entrambi");
  if (filtro < 0 || filtro > DRV_CONT_FILTRO_MAX) return tr("Filtro del contatore: da 0 a 10000 ns");
  d.addr = (uint8_t)fronte;
  d.khz = (uint16_t)filtro;
  d.gpio2 = -1;
  return ioCntOpen(d);
}

int ioCntRead(const IoDev& d) {
  return d.cnt ? (int)(drvContatoreLeggi(d.cnt - 1) & 0x7FFFFFFFu) : 0;
}

// Frequenza = impulsi / tempo trascorso dall'ultima stima. Si aspetta almeno 1 s, e 4 s se gli impulsi sono pochi (piu preciso a bassa frequenza).
void ioCntTick(IoDev& d, uint32_t now) {
  if (d.cls != IC_CNT || !d.cnt) return;
  uint32_t dt = now - d.cT;
  if (dt < 1000) return;
  uint32_t cur = drvContatoreLeggi(d.cnt - 1), delta = cur - d.cPrev;
  if (delta < 20 && dt < 4000) return;
  d.cMhz = (uint32_t)((uint64_t)delta * 1000000ULL / dt);
  d.cPrev = cur;
  d.cT = now;
}

int ioCntFree() { return drvContatoreLiberi(); }

bool ioCntReset(const char* name, int role, ErrBuf& err) {
  Lock L;
  IoDev* d = ioFindDev(name);
  if (!d || d->cls != IC_CNT) { err = tr("Periferica sconosciuta"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (d->cnt) drvContatoreAzzera(d->cnt - 1);
  zero(*d);
  return true;
}

bool ioCntGet(const char* name, int& count, int& mhz) {
  Lock L;
  const IoDev* d = ioFindDev(name);
  if (!d || d->cls != IC_CNT) return false;
  count = ioCntRead(*d);
  mhz = (int)d->cMhz;
  return true;
}

bool ioCntAct(const char* name, const char* act, HeapBuf& out, int role, ErrBuf& err) {
  if (!strcmp(act, "reset")) {
    if (!ioCntReset(name, role, err)) return false;
  } else if (strcmp(act, "get")) {
    err = tr("Azione non valida");
    return false;
  }
  int n, mhz;
  return ioCntGet(name, n, mhz) && out.addf("{\"v\":%d,\"mhz\":%d}", n, mhz);
}
#endif
