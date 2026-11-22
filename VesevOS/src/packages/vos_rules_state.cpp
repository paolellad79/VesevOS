// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules_state.cpp: testo delle regole e stato per la pagina (vedi vos_rules.cpp).
#include "vos_rules_int.h"
#include "../drivers/vos_drv_fs.h"
#include "../core/vos_strbuf.h"

bool rulesText(HeapBuf& out) {
  VosFile f = drvFsOpen(RULES_FILE, "r");
  if (!f) return true;
  uint8_t b[128];
  while (f.available()) {
    size_t n = f.read(b, sizeof b);
    if (n == 0) break;
    if (!out.add((const char*)b, n)) { f.close(); return false; }
  }
  f.close();
  return true;
}

// "q" = epoca dell'ultima esecuzione (0 = mai): la pagina la formatta. Una voce per regola (al piu ~77 caratteri): un solo blocco di heap della misura giusta. Se non bastasse ritorna "[]" mai un JSON tagliato.
std::string rulesStatusJson() {
  ensureMx();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  StrText t(8 + (size_t)g_n * 80);
  StrBuf& j = t.buf();
  j.add('[');
  for (int i = 0; i < g_n; i++) {
    if (i) j.add(',');
    bool run = false;
    for (int k = 0; k < MAX_RUNS; k++) if (g_run[k].act && g_run[k].rule == i) run = true;
    j.addf("{\"i\":%d,\"q\":%lu,\"n\":%lu,\"off\":%s,\"run\":%s}", i, (unsigned long)g_r[i].lastEpoch, (unsigned long)g_r[i].nfire,
           g_r[i].off ? "true" : "false", run ? "true" : "false");
  }
  j.add(']');
  xSemaphoreGive(g_mx);
  return t.truncated() ? std::string("[]") : t.take();
}
