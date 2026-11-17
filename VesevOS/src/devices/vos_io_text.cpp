// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_text.cpp
// Elenco delle periferiche a classi come JSON (pagina) e come testo (shell), scritto in un HeapBuf del chiamante: niente String, e se manca
// la memoria si risponde false (mai un JSON o un testo troncato). Spostato da vos_io.cpp (file sotto le 500 righe).
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_strbuf.h"
#include <freertos/FreeRTOS.h>

namespace {
struct TextLock {                                    // come IoLock(300 ms): se il blocco non arriva si legge lo stesso (solo lettura)
  bool held;
  TextLock() : held(ioLockTake(pdMS_TO_TICKS(300))) {}
  ~TextLock() { if (held) ioLockGive(); }
};
bool isOutCls(uint8_t c) { return c == IC_RELE || c == IC_LED || c == IC_PIN; }
bool pwmFree(HeapBuf& j) {                           // canali PWM liberi: la pagina mostra il tipo PWM solo se il campo c'e
#if VOS_WITH_PWM
  return j.addf(",\"pwm\":%d", ioPwmFree());
#else
  return true;
#endif
}
bool cntFree(HeapBuf& j) {                           // unita del contatore libere: la pagina mostra il tipo solo se il campo c'e
#if VOS_WITH_CONTATORE
  return j.addf(",\"cnt\":%d", ioCntFree());
#else
  return true;
#endif
}
bool rgbFree(HeapBuf& j) {                           // canali dei LED a colori a mano liberi: la pagina mostra il tipo solo se il campo c'e
#if VOS_WITH_LED_RGB_A_MANO
  return j.addf(",\"rgb\":%d", ioLedFree());
#else
  return true;
#endif
}
}

bool ioListJson(HeapBuf& j) {
  TextLock L;
  bool ok = j.add("{\"max\":") && j.addUint(IO_MAX) && j.add(VOS_WITH_SD ? ",\"sdp\":[" VOS_SD_DEF_LIST "]" : "") && pwmFree(j) && cntFree(j) && rgbFree(j) && j.add(",\"io\":[");
  bool have; int32_t t100; uint32_t pPa; ioI2cRead(have, t100, pPa);
  for (int i = 0; ok; i++) {
    const IoDev* d = ioDevAt(i);
    if (!d) break;
    int v = 0; ioValueAt(i, v);
    char x[200]; StrBuf sb(x, sizeof x);
    sb.addf("{\"n\":\"%s\",\"c\":\"%s\",\"g\":%d,\"f\":%d,\"v\":%d,\"out\":%d", d->name, ioClsName(d->cls), (int)d->gpio, (int)d->flags, v, isOutCls(d->cls) ? 1 : 0);
    if (d->cls == IC_I2C) sb.addf(",\"g2\":%d,\"a\":%d,\"d\":\"bmp280\",\"p\":%u,\"ok\":%d", (int)d->gpio2, (int)d->addr, (unsigned)pPa, have ? 1 : 0);
    if (d->cls == IC_SD) sb.addf(",\"g2\":%d,\"g3\":%d,\"g4\":%d,\"k\":%u", (int)d->gpio2, (int)d->addr, (int)d->drv, (unsigned)d->khz);
    if (d->cls == IC_PWM) sb.addf(",\"hz\":%u", (unsigned)d->khz);              // v = duty 0-100
    if (d->cls == IC_CNT) sb.addf(",\"mhz\":%u,\"e\":%d,\"ns\":%u", (unsigned)d->cMhz, (int)d->addr, (unsigned)d->khz);   // v = impulsi
    if (d->cls == IC_LEDRGB) sb.addf(",\"col\":%u,\"lum\":%u,\"ord\":\"%s\"", (unsigned)d->cPrev, (unsigned)d->addr, ioLedOrdNome(d->drv));   // v = acceso 0/1
    sb.add('}');
    ok = (i == 0 || j.add(',')) && j.add(x);
  }
  return ok && j.add("]}");
}

bool ioText(HeapBuf& t) {
  TextLock L;
  if (!ioCount()) return t.add(tr("Nessuna periferica. Aggiungi: io add <classe> <pin> [nome] [inv|up|down]")) && t.add('\n');
  bool have; int32_t t100; uint32_t pPa; ioI2cRead(have, t100, pPa);
  for (int i = 0; ; i++) {
    const IoDev* d = ioDevAt(i);
    if (!d) return true;
    int v = 0; ioValueAt(i, v);
    char x[200]; StrBuf sb(x, sizeof x);
    sb.addf("%-12s%-10s", d->name, ioClsName(d->cls));
    if (d->cls == IC_SD) sb.addf("CS GPIO%d MOSI GPIO%d SCK GPIO%d MISO GPIO%d  %u kHz  %s\n", (int)d->gpio, (int)d->gpio2, (int)d->addr, (int)d->drv, (unsigned)d->khz, v ? tr("montata") : tr("non montata"));
    else if (d->cls == IC_I2C && have) sb.addf("SDA GPIO%d SCL GPIO%d  0x%x  %.1f C  %.1f hPa\n", (int)d->gpio, (int)d->gpio2, (unsigned)d->addr, t100 / 100.0f, pPa / 100.0f);
    else if (d->cls == IC_I2C) sb.addf("SDA GPIO%d SCL GPIO%d  0x%x  %s\n", (int)d->gpio, (int)d->gpio2, (unsigned)d->addr, tr("non risponde"));
    else if (d->cls == IC_PWM) sb.addf("GPIO%d  %u Hz  %d%%\n", (int)d->gpio, (unsigned)d->khz, v);
    else if (d->cls == IC_LEDRGB) sb.addf("GPIO%d  %s  #%06x  %u  %s\n", (int)d->gpio, v ? tr("acceso") : tr("spento"), (unsigned)d->cPrev, (unsigned)d->addr, ioLedOrdNome(d->drv));
    else if (d->cls == IC_CNT) sb.addf("GPIO%d  %d  %u.%03u Hz\n", (int)d->gpio, v, (unsigned)(d->cMhz / 1000), (unsigned)(d->cMhz % 1000));
    else {
      sb.addf("GPIO%d  ", (int)d->gpio);
      if (d->cls == IC_ADC) sb.addf("%d", v); else sb.add(v ? tr("acceso") : tr("spento"));
      if (d->flags & IOF_INV) sb.add("  inv");
      if (d->flags & IOF_UP) sb.add("  up");
      if (d->flags & IOF_DOWN) sb.add("  down");
      sb.add('\n');
    }
    if (!t.add(x)) return false;
  }
}
