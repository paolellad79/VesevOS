// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_sd.cpp
// Percorsi del server web per la scheda microSD: stato e azioni. I permessi li decide sdAct (stato ed elenco: Operatore; il resto: Admin).
// Scritto senza il tipo String di Arduino (i buffer sono di char), come tutto il codice nuovo.
#include "vos_web_int.h"
#include "../devices/vos_sd.h"
#if VOS_WITH_SD

void routesSd(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/sd", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char* out = (char*)malloc(SD_OUT_MAX);
    if (!out) return ko(s, "memoria");
    sdStatusJson(out, SD_OUT_MAX);
    esp_err_t e = sendJson(s, out); free(out); return e;
  });
  add(S, sec, "/api/sd/act", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {   // a=state|mount|unmount|test|ls|read|write|append|rm|mkdir, arg=...
    char* out = (char*)malloc(SD_OUT_MAX);
    if (!out) return ko(s, "memoria");
    char err[SD_ERR_MAX];
    bool okk = sdAct(Pc(r, "a"), Pc(r, "arg"), c.role, out, SD_OUT_MAX, err, sizeof err);
    esp_err_t e = okk ? (out[0] ? sendJson(s, out) : ok(s)) : ko(s, err);
    free(out); return e;
  });
}

#endif
