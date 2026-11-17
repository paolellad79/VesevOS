// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_ledrgb.cpp
// Classe "led_rgb" delle periferiche a classi (vedi vos_io.h): un LED a colori WS2812 collegato dall'utente a un pin (un LED = un pixel).
// Usa il driver generico drivers/vos_drv_led (canali RMT). Il controllo dei pin (vietati, occupati, doppi) resta uno solo, in vos_io:
// qui si gestiscono solo colore, luminosita, acceso e ordine dei byte. Senza i modi speciali del LED di bordo (stato di rete, trigger).
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_led.h"
#include <freertos/FreeRTOS.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if VOS_WITH_LED_RGB_A_MANO
static_assert(DRV_LED_ORD_MAX == 5, "adeguare l'elenco ORD in vos_io_ledrgb.cpp");

namespace {
struct Lock {                                         // lo stesso blocco di vos_io (ricorsivo): come in vos_io_pwm.cpp
  bool held;
  Lock() : held(ioLockTake(pdMS_TO_TICKS(500))) {}
  ~Lock() { if (held) ioLockGive(); }
};

// Nell'ordine dei numeri VOS_LED_ORD_* di boards/vos_board.h
const char* const ORD[] = { "grb", "rgb", "brg", "gbr", "rbg", "bgr" };

const char* errOf(int code) {
  if (code == DRV_LED_E_CANALI) return tr("Canali dei LED a colori finiti");
  return tr("Il LED a colori non si e aperto");
}

bool ordOf(const char* s, uint8_t& out) {
  for (uint8_t i = 0; i <= DRV_LED_ORD_MAX; i++) {
    if (!strncmp(s, ORD[i], 3) && !isalpha((unsigned char)s[3])) { out = i; return true; }
  }
  return false;
}

// Manda al LED il colore con la luminosita; spento = nero. Si chiama solo con il LED aperto.
void scrivi(const IoDev& d) {
  if (!d.led) return;
  uint32_t c = d.on ? d.cPrev : 0;
  uint16_t lum = d.addr;
  uint8_t r = (uint8_t)(((c >> 16) & 255) * lum / 255);
  uint8_t g = (uint8_t)(((c >> 8) & 255) * lum / 255);
  uint8_t b = (uint8_t)((c & 255) * lum / 255);
  drvLedScrivi(d.led - 1, r, g, b);
}

// Colore "RRGGBB": esattamente 6 cifre esadecimali (il testo finisce dopo di esse o alla virgola)
bool hexColor(const char* s, uint32_t& out) {
  uint32_t v = 0;
  for (int i = 0; i < 6; i++) {
    if (!isxdigit((unsigned char)s[i])) return false;
    v = (v << 4) | (uint32_t)(isdigit((unsigned char)s[i]) ? s[i] - '0' : (tolower((unsigned char)s[i]) - 'a' + 10));
  }
  if (s[6] != 0 && s[6] != ',') return false;
  out = v;
  return true;
}
}  // namespace

const char* ioLedOrdNome(uint8_t ordine) { return ordine <= DRV_LED_ORD_MAX ? ORD[ordine] : ORD[0]; }

const char* ioLedOpen(IoDev& d) {
  if (d.led) return nullptr;
  int h = drvLedApri(d.gpio, d.drv);
  if (h < 0) return errOf(h);
  d.led = (uint8_t)(h + 1);
  scrivi(d);                                          // com'era prima del riavvio (spento = nero: il LED non resta in uno stato a caso)
  return nullptr;
}

void ioLedClose(IoDev& d) {
  if (!d.led) return;
  drvLedScrivi(d.led - 1, 0, 0, 0);
  drvLedChiudi(d.led - 1);
  d.led = 0;
}

void ioLedOff(IoDev& d) {
  d.on = false;
  scrivi(d);
}

const char* ioLedStart(IoDev& d, const char* opt) {
  uint8_t ord = VOS_LED_ORDER;                        // di fabbrica quello della scheda
  const char* q = strstr(opt, "ordine=");
  if (q && !ordOf(q + 7, ord)) return tr("Ordine dei byte: grb, rgb, brg, gbr, rbg o bgr");
  d.gpio2 = -1;
  d.on = false;
  d.cPrev = 0xFFFFFF;                                 // bianco
  d.addr = 128;                                       // luminosita meta
  d.drv = ord;
  return ioLedOpen(d);
}

bool ioLedCfgOk(unsigned colore, unsigned lum, unsigned ordine) {
  return colore <= 0xFFFFFFu && lum <= 255 && ordine <= DRV_LED_ORD_MAX;
}

void ioLedCarica(IoDev& d, unsigned flags, unsigned colore, unsigned lum, unsigned ordine) {
  d.gpio2 = -1;
  d.on = (flags & IOF_ACCESO) != 0;
  d.flags = (uint8_t)(flags & ~(unsigned)IOF_ACCESO);
  d.cPrev = colore;
  d.addr = (uint8_t)lum;
  d.drv = (uint8_t)ordine;
}

int ioLedFree() { return drvLedLiberi(); }

// Cambia i valori e li salva; se io.cfg non si scrive il LED torna com'era e si risponde l'errore.
static bool applica(IoDev& d, bool on, uint32_t col, uint8_t lum, uint8_t ord, ErrBuf& err) {
  IoDev prima = d;
  d.on = on;
  d.cPrev = col;
  d.addr = lum;
  d.drv = ord;
  if (d.led) drvLedOrdine(d.led - 1, ord);
  const char* why = ioSaveCfg();
  if (why) {
    d = prima;
    if (d.led) drvLedOrdine(d.led - 1, d.drv);
    err = why;
    return false;
  }
  scrivi(d);
  return true;
}

bool ioLedSetOn(IoDev& d, bool on, int role, ErrBuf& err) {
  Lock L;
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (on == d.on) return true;
  return applica(d, on, d.cPrev, d.addr, d.drv, err);
}

bool ioLedAct(const char* name, const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  Lock L;
  IoDev* d = ioFindDev(name);
  if (!d || d->cls != IC_LEDRGB) { err = tr("Periferica sconosciuta"); return false; }
  if (!strcmp(act, "colore")) {                       // arg: "RRGGBB" oppure "RRGGBB,luminosita" oppure ",luminosita"
    if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    uint32_t col = d->cPrev;
    uint8_t lum = d->addr;
    if (arg[0] != ',' && !hexColor(arg, col)) { err = tr("Colore: 6 cifre esadecimali, per esempio ff0000"); return false; }
    const char* comma = strchr(arg, ',');
    if (comma) {
      char* e;
      long v = strtol(comma + 1, &e, 10);
      if (e == comma + 1 || *e || v < 0 || v > 255) { err = tr("Luminosita: da 0 a 255"); return false; }
      lum = (uint8_t)v;
    }
    if (!applica(*d, d->on, col, lum, d->drv, err)) return false;
  } else if (!strcmp(act, "ordine")) {
    if (role < ROLE_ADMIN) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    uint8_t ord;
    if (!ordOf(arg, ord)) { err = tr("Ordine dei byte: grb, rgb, brg, gbr, rbg o bgr"); return false; }
    if (!applica(*d, d->on, d->cPrev, d->addr, ord, err)) return false;
  } else {
    err = tr("Azione non valida");
    return false;
  }
  return out.addf("{\"v\":%d,\"col\":%u,\"lum\":%u,\"ord\":\"%s\"}", d->on ? 1 : 0, (unsigned)d->cPrev, (unsigned)d->addr, ioLedOrdNome(d->drv));
}
#endif
