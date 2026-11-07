// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_eventi.cpp
// Pagina Eventi di sicurezza (solo Amministratore): lettura e svuotamento del registro in RAM.
#include "vos_web_int.h"
#include "../core/vos_eventi.h"

void routesEventi(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/eventi", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(10240);
    if (!eventiJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
    eventiBlocchiVisti();                          // l'amministratore li ha davanti: la campanella si spegne
    return sendJson(s, j);
  });

  add(S, sec, "/api/eventi/svuota", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!Pis(r, "confirm", "1")) return ko(s, tr("Serve la conferma"), "bad_request");
    char chi[96]; whoTo(c, chi, sizeof chi);
    eventiSvuota();
    vlog("SICUREZZA: eventi di sicurezza svuotati (%s)", chi);
    return ok(s);
  });
}
