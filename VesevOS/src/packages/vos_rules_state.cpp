// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules_state.cpp: testo delle regole e stato per la pagina (vedi vos_rules.cpp).
#include "vos_rules_int.h"
#include "../drivers/vos_drv_fs.h"

String rulesText() {
  File f = drvFsOpen(RULES_FILE, "r");
  if (!f) return "";
  String s = f.readString(); f.close();
  return s;
}

String rulesStatusJson() {
  String j = "[";
  ensureMx();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < g_n; i++) {
    if (i) j += ",";
    String last = "";
    if (g_r[i].lastEpoch) {
      struct tm tmv; localtime_r(&g_r[i].lastEpoch, &tmv);
      char b[24]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
      last = b;
    }
    bool run = false;
    for (int k = 0; k < MAX_RUNS; k++) if (g_run[k].act && g_run[k].rule == i) run = true;
    j += "{\"i\":" + String(i) + ",\"last\":\"" + last + "\",\"n\":" + String((unsigned long)g_r[i].nfire) +
         ",\"off\":" + (g_r[i].off ? "true" : "false") + ",\"run\":" + (run ? "true" : "false") + "}";
  }
  xSemaphoreGive(g_mx);
  return j + "]";
}
