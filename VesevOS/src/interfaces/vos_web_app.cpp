// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_app.cpp
// Percorsi del server web: pagina App (elenco, controllo, avvio, stop, valori dei controlli). Il testo dell'app si legge e si salva con i
// percorsi dei file (/api/fs/text, /api/fs/save) nella cartella /apps (solo Admin).
// Operatore: vede e usa solo le app valide con who=oper (avvia, ferma, comanda i controlli); il resto e solo Admin.
#include "vos_web_int.h"
#include "../packages/vos_app.h"

void routesApp(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/apps", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, appListJson(c.role)); });
  add(S, sec, "/api/app", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, appInfoJson(P(r, "name"), c.role)); });
  // valori dei controlli, uscita ed errore: la pagina lo rilegge ogni ~1 s mentre l'app e aperta
  add(S, sec, "/api/app/state", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, appStateJson(P(r, "name"), c.role)); });
  add(S, sec, "/api/app/start", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!appStart(P(r, "name"), e, c.role, "pagina")) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/app/stop", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!appStop(P(r, "name"), e, c.role)) return ko(s, e, "state");
    return ok(s);
  });
  add(S, sec, "/api/app/set", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!appSet(P(r, "name"), P(r, "id"), P(r, "v"), e, c.role)) return ko(s, e);
    return ok(s);
  });
}
