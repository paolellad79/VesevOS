// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_app.cpp
// Percorsi del server web: pagina App (elenco, controllo, avvio, stop, valori dei controlli). Il testo dell'app si legge e si salva con i
// percorsi dei file (/api/fs/text, /api/fs/save) nella cartella /apps (solo Admin).
// Operatore: vede e usa solo le app valide con who=oper (avvia, ferma, comanda i controlli); il resto e solo Admin.
#include "vos_web_int.h"
#include "../packages/vos_app.h"

void routesApp(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/apps", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(APP_LIST_MAX);                                        // il JSON si scrive qui (heap) e si invia senza copiarlo in una String
    if (!appListJson(j, c.role)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/app", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(APP_INFO_MAX);
    if (!appInfoJson(Pc(r, "name"), j, c.role)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  // valori dei controlli, uscita ed errore: la pagina lo rilegge ogni ~1 s mentre l'app e aperta
  add(S, sec, "/api/app/state", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(APP_STATE_MAX);
    if (!appStateJson(Pc(r, "name"), j, c.role)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/app/start", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!appStart(Pc(r, "name"), e, c.role, "pagina")) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/app/stop", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!appStop(Pc(r, "name"), e, c.role)) return ko(s, e, "state");
    return ok(s);
  });
  add(S, sec, "/api/app/set", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!appSet(Pc(r, "name"), Pc(r, "id"), Pc(r, "v"), e, c.role)) return ko(s, e);
    return ok(s);
  });
}
