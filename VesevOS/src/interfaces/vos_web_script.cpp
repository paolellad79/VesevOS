// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_script.cpp
// Percorsi del server web: pagina Script (elenco, stato e uscita; avvio e stop). Solo Admin.
// Il testo degli script si legge e si salva con i percorsi dei file (/api/fs/text, /api/fs/save) nella cartella /scripts.
#include "vos_web_int.h"
#include "../packages/vos_berry.h"

void routesScript(PsychicHttpServer* S, bool sec) {
  // elenco degli script, stato (in corso?) e ultima uscita: un solo pezzo, la pagina lo rilegge ogni ~1,5 s mentre e aperta
  add(S, sec, "/api/script", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, berryStateJson()); });
  add(S, sec, "/api/script/run", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (berryRunning()) return ko(s, tr("Berry e occupato con un altro script"), "state");
    String e;
    if (!berryStart(P(r, "name"), e)) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/script/stop", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!berryRunning()) return ko(s, tr("Nessuno script in corso"), "state");
    return berryStop(e) ? ok(s) : ko(s, e, "state");
  });
}
