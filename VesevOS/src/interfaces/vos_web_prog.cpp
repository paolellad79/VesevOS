// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_prog.cpp
// Percorsi del server web per i PROGRAMMI in corso (script, app Berry, app ELF) e per cio che li governa: i sottosistemi vdb e vde (interruttori),
// le Funzionalita (Sistema) e l'avvio automatico (pannello Avvio). Tutto passa dalle funzioni di packages/vos_prog.h e vos_func.h, le stesse
// della shell. Operatore: legge. Admin: ferma, avvia, riavvia, accende e spegne, concede e toglie.
#include "vos_web_int.h"
#include "../packages/vos_prog.h"
#include "../packages/vos_func.h"
#include "../packages/vos_prog_data.h"
#include "../packages/vos_prog_where.h"

namespace {

uint8_t kindOf(const char* k) { return k[0] == 's' ? PK_SCRIPT : k[0] == 'e' ? PK_APP_E : PK_APP_B; }

esp_err_t progsReply(Res* s) {
  HeapBuf j(PROG_JSON_MAX);
  j.add("{\"ok\":true,");
  if (!progJson(j)) return ko(s, tr("Memoria insufficiente"));
  j.add('}');
  if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"));
  return sendJson(s, j);
}

esp_err_t progAction(Req* r, Res* s, Ctx& c) {
  ErrBuf e;
  const char* name = Pc(r, "name");
  if (Pis(r, "act", "stop")) return progStop(name, e) ? ok(s) : ko(s, e);
  if (Pis(r, "act", "start")) return progStart(kindOf(Pc(r, "k")), name, c.role, "pagina", e) ? ok(s) : ko(s, e);
  if (Pis(r, "act", "restart")) return progRestart(kindOf(Pc(r, "k")), name, c.role, e) ? ok(s) : ko(s, e);
  return ko(s, tr("Azione sconosciuta"));
}

esp_err_t subSwitch(Req* r, Res* s) {
  const char* id = progSubService(Pc(r, "id"));
  if (!id) return ko(s, tr("Servizio sconosciuto"));
  ErrBuf e;
  if (Pis(r, "on", "1")) { if (serviceState(id) != SVC_ON && !serviceStart(id, e)) return ko(s, e); }
  else serviceStop(id, e);
  return progsReply(s);
}

}  // namespace

void routesProg(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/progs", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return progsReply(s); });
  add(S, sec, "/api/progs", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return progAction(r, s, c); });
  add(S, sec, "/api/sub", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return subSwitch(r, s); });
  add(S, sec, "/api/func", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(FUNC_JSON_MAX);
    if (!funcJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/func", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!funcSet(funcIndex(Pc(r, "id")), Pis(r, "on", "1"), e)) return ko(s, e);
    HeapBuf j(FUNC_JSON_MAX);
    if (!funcJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/progdata", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // dove le app tengono i dati: microSD utilizzabile e app su SD
    HeapBuf j(PROG_JSON_MAX);
    if (!progDataJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/progdata", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!progWhereSet(kindOf(Pc(r, "k")), Pc(r, "name"), Pis(r, "where", "sd") ? PW_SD : PW_FLASH, e)) return ko(s, e);
    HeapBuf j(PROG_JSON_MAX);
    if (!progDataJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/progboot", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(PROG_JSON_MAX);
    if (!progBootJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/progboot", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!progBootSet(Pc(r, "list"), e)) return ko(s, e);
    HeapBuf j(PROG_JSON_MAX);
    if (!progBootJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
}
