// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_coproc.cpp
// Pagina Coprocessore (solo lettura, Operatore): cosa dichiara la scheda e causa dell'ultimo risveglio.
// Nessun programma si carica sul coprocessore: "prog" e sempre 0. La scheda dichiara (VOS_HAS_ULP, VOS_HAS_LP_CPU,
// VOS_RTC_MEM_BYTES), la pagina disegna: qui non si guarda il nome del chip.
#include "vos_web_int.h"
#include "../drivers/vos_drv_sys.h"

namespace {

esp_err_t sendCoproc(Res* s) {
  HeapBuf j(160);
  bool ok = j.addf("{\"ulp\":%d,\"lp\":%d,\"mem\":%d,\"prog\":0,\"wake\":%d}",
                   (int)VOS_HAS_ULP, (int)VOS_HAS_LP_CPU, (int)VOS_RTC_MEM_BYTES, drvWakeCause());
  return ok ? sendJson(s, j) : ko(s, tr("Memoria insufficiente"));
}

}  // namespace

void routesCoproc(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/coproc", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendCoproc(s); });
}
