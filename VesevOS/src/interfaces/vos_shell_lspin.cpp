// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_lspin.cpp
// Comando "lspin": come "lspci" di Debian, ma per i pin. Sola lettura, per l'Operatore. Stessi dati della pagina Pin (registro dei pin, periferiche, driver).
//
//   lspin               una riga per pin occupato: pin  periferica  segnale  (tipo)
//   lspin -v            piu dettagli: livello letto adesso, funzioni possibili, nota
//   lspin -vv           anche i pin liberi, con le funzioni possibili
//   lspin -s <pin>      solo quel pin (libero o no), con i dettagli
//   lspin -k <nome>     solo i pin di una periferica o di un driver
// "pins" resta come alias del vecchio elenco. Non cambia nessun pin: il livello si legge soltanto.
#include "vos_shell_int.h"
#include <stdlib.h>
#include "../devices/vos_io.h"
#include "../devices/vos_pins.h"
#include "../drivers/vos_drv_gpio.h"
#if VOS_WITH_ELF
#include "../devices/vos_drvres.h"
#endif

namespace {

// Che cos'e chi usa il pin: sistema (riservato), periferica a classi (rele, sd, i2c...), driver vde o altro.
const char* tipoDi(const PinInfo* p) {
  if (p->fixed) return tr("riservato (sistema)");
  if (ioHas(p->owner)) return ioClassName(p->owner);
#if VOS_WITH_ELF
  DrvRes r;
  if (drvresFind(p->owner, r)) return "vde";
#endif
  return tr("altro");
}

const PinInfo* trova(int g) {
  for (int i = 0; i < pinCount(); i++) if (pinAt(i)->gpio == g) return pinAt(i);
  return nullptr;
}

// Le funzioni che il pin puo avere: sull'S3 le digitali passano dalla matrice dei pin; l'ADC dipende dal pin.
void funzioni(int g, char* out, size_t cap) {
  snprintf(out, cap, "GPIO, I2C, SPI, UART, PWM, I2S%s", g >= VOS_ADC_FIRST && g <= VOS_ADC_LAST ? ", ADC1" : g >= VOS_ADC2_FIRST && g <= VOS_ADC2_LAST ? ", ADC2" : "");
}

void riga(OutSink& o, int g, int v) {
  const PinInfo* p = trova(g);
  char f[48]; funzioni(g, f, sizeof f);
  if (!p) {
    if (v >= 2) o.printf("%-4d %-12s %-6s (%s)  %s: %s\r\n", g, "-", "-", tr("nessun uso"), tr("funzioni"), f);
    return;
  }
  if (p->fixed) o.printf("%-4d %-12s %-6s (%s)\r\n", g, "-", p->sig[0] ? p->sig : "-", tipoDi(p));
  else o.printf("%-4d %-12s %-6s (%s)\r\n", g, p->owner, p->sig[0] ? p->sig : "-", tipoDi(p));
  if (v >= 1) {
    o.printf("       %s: %d   %s: %s\r\n", tr("livello"), drvGpioRead(g) ? 1 : 0, tr("funzioni"), f);
    if (p->note && p->note[0]) o.printf("       %s\r\n", tr(p->note));
  }
}

}  // namespace

void shx::shLspin(const char* line, OutSink& o) {
  int v = 0, solo = -1;
  char nome[40] = "";
  for (int i = 1; i < 8; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w == "-v") v = v < 1 ? 1 : v;
    else if (w == "-vv") v = 2;
    else if (w == "-s") { Word n = argAt(line, ++i); solo = n.empty() ? -2 : atoi(n.c_str()); }
    else if (w == "-k") { Word n = argAt(line, ++i); snprintf(nome, sizeof nome, "%s", n.c_str()); if (n.empty()) solo = -2; }
    else { solo = -2; break; }
  }
  if (solo == -2 || (solo >= 0 && !VOS_GPIO_EXISTS(solo))) { o.println(tr("Uso: lspin [-v|-vv] [-s pin] [-k nome]")); return; }
  if (solo >= 0) { riga(o, solo, v < 1 ? 1 : v); return; }
  for (int g = 0; g <= VOS_GPIO_MAX; g++) {
    if (!VOS_GPIO_EXISTS(g)) continue;
    const PinInfo* p = trova(g);
    if (nome[0] && (!p || strcmp(p->owner, nome))) continue;
    if (!p && v < 2) continue;                                // senza -vv solo i pin occupati
    riga(o, g, v);
  }
}
