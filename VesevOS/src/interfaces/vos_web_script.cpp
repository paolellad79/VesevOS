// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_script.cpp
// Percorsi del server web: pagina Script (elenco, stato e uscita; avvio e stop). Solo Admin.
// Il testo degli script si legge e si salva con i percorsi dei file (/api/fs/text, /api/fs/save) nella cartella /sbin.
#include "vos_web_int.h"
#include "../packages/vos_berry.h"
#include "../packages/vos_berry_api.h"

void routesScript(PsychicHttpServer* S, bool sec) {
  // elenco delle funzioni Berry (nome, gruppo, ruolo): la pagina Script lo usa per il prompt delle AI. Sola lettura, da Operatore
  add(S, sec, "/api/script/api", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(BERRY_API_MAX);
    if (!berryApiJson(j)) return ko(s, tr("Elenco delle funzioni non disponibile"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });

  // elenco degli script, stato (in corso?) e ultima uscita: un solo pezzo, la pagina lo rilegge ogni ~1,5 s mentre e aperta
  add(S, sec, "/api/script", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(BERRY_STATE_MAX);                                     // el JSON si scrive qui (heap) e si invia senza copiarlo in una String
    if (!berryStateJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/script/run", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (berryRunning()) return ko(s, tr("Un altro script e gia in esecuzione"), "state");
    ErrBuf e;
    if (!berryStart(Pc(r, "name"), e)) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/script/stop", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!berryRunning()) return ko(s, tr("Nessuno script in corso"), "state");
    return berryStop(e) ? ok(s) : ko(s, e, "state");
  });
}
