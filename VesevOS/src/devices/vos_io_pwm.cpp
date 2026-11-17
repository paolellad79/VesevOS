// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_pwm.cpp
// Classe "pwm" delle periferiche a classi (vedi vos_io.h): usa il driver generico drivers/vos_drv_pwm. Il controllo dei pin
// (vietati, occupati, doppi) resta uno solo, in vos_io: qui si gestiscono solo frequenza, duty e canale.
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include <freertos/FreeRTOS.h>
#include <stdlib.h>
#include <string.h>

#if VOS_WITH_PWM
static_assert(DRV_PWM_HZ_MIN == 10 && DRV_PWM_HZ_MAX == 40000, "adeguare il testo di tr() sulla frequenza PWM");

namespace {
struct Lock {                                         // lo stesso blocco di vos_io (ricorsivo): come in vos_io_text.cpp
  bool held;
  Lock() : held(ioLockTake(pdMS_TO_TICKS(500))) {}
  ~Lock() { if (held) ioLockGive(); }
};

const char* errOf(int code) {
  if (code == DRV_PWM_E_CANALI) return tr("Canali PWM finiti");
  if (code == DRV_PWM_E_TIMER) return tr("Troppe frequenze PWM diverse: usane una gia in uso");
  return tr("Questo chip non sa fare quella frequenza PWM");
}

bool hzOk(int hz) { return hz >= DRV_PWM_HZ_MIN && hz <= DRV_PWM_HZ_MAX; }
}  // namespace

const char* ioPwmOpen(IoDev& d) {
  if (d.pwm) return nullptr;
  int h = drvPwmOpen(d.gpio, d.khz, 0, false);
  if (h < 0) return errOf(h);
  d.pwm = (uint8_t)(h + 1);
  drvPwmSet(h, d.addr, 100);
  return nullptr;
}

void ioPwmClose(IoDev& d) {
  if (!d.pwm) return;
  drvPwmClose(d.pwm - 1);
  d.pwm = 0;
}

void ioPwmOff(IoDev& d) {
  d.addr = 0;
  if (d.pwm) drvPwmSet(d.pwm - 1, 0, 100);
}

const char* ioPwmStart(IoDev& d, const char* opt) {
  int hz = ioOptNum(opt, "hz=", 1000), duty = ioOptNum(opt, "duty=", 0);
  if (!hzOk(hz)) return tr("Frequenza PWM: da 10 a 40000 Hz");
  if (duty < 0 || duty > 100) return tr("Duty PWM: da 0 a 100");
  d.khz = (uint16_t)hz;
  d.addr = (uint8_t)duty;
  d.gpio2 = -1;
  return ioPwmOpen(d);
}

int ioPwmFree() { return drvPwmFree(); }

bool ioPwmSet(const char* name, int hz, int pct, int role, ErrBuf& err) {
  Lock L;
  IoDev* d = ioFindDev(name);
  if (!d || d->cls != IC_PWM) { err = tr("Periferica sconosciuta"); return false; }
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (pct > 100) { err = tr("Duty PWM: da 0 a 100"); return false; }
  if (hz && hz != d->khz) {
    if (role < ROLE_ADMIN) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    if (!hzOk(hz)) { err = tr("Frequenza PWM: da 10 a 40000 Hz"); return false; }
    uint16_t old = d->khz;
    ioPwmClose(*d);
    d->khz = (uint16_t)hz;
    const char* why = ioPwmOpen(*d);
    if (!why) why = ioSaveCfg();
    if (why) {                                       // si torna alla frequenza di prima
      ioPwmClose(*d);
      d->khz = old;
      ioPwmOpen(*d);
      err = why;
      return false;
    }
  }
  if (pct >= 0) {
    d->addr = (uint8_t)pct;
    if (d->pwm) drvPwmSet(d->pwm - 1, d->addr, 100);
  }
  return true;
}

bool ioPwmGet(const char* name, int& hz, int& pct) {
  Lock L;
  const IoDev* d = ioFindDev(name);
  if (!d || d->cls != IC_PWM) return false;
  hz = d->khz;
  pct = d->addr;
  return true;
}

bool ioPwmAct(const char* name, const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  if (!strcmp(act, "set")) {
    const char* comma = strchr(arg, ',');
    if (!ioPwmSet(name, comma ? atoi(comma + 1) : 0, atoi(arg), role, err)) return false;
  } else if (strcmp(act, "get")) {
    err = tr("Azione non valida");
    return false;
  }
  int hz, pct;
  return ioPwmGet(name, hz, pct) && out.addf("{\"v\":%d,\"hz\":%d}", pct, hz);
}
#endif
