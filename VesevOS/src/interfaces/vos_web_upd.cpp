// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_upd.cpp
// Aggiornamento tramite recovery: la pagina chiede, VesevOS scrive l'ordine (core/vos_updorder) e riavvia nel recovery.
// Solo Amministratore. Nessuna connessione verso l'esterno parte da qui: e il recovery a scaricare, e solo se l'amministratore lo ordina.
// Il gettone del modo 2 compare soltanto nella risposta a chi ha chiesto: mai nel registro, nell'audit o nella shell.
#include "vos_web_int.h"

// "confirm=1" e la conferma esplicita della pagina (dopo l'avviso): senza, nessun ordine
static bool confirmed(Req* r) { return Pis(r, "confirm", "1"); }

void routesUpd(PsychicHttpServer* S, bool sec) {
  // ultimo esito e dati utili alla pagina (sola lettura). Chi lo legge lo "vede": non e piu nuovo.
  add(S, sec, "/api/updorder", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const UpdOutcome& o = updOutcome();
    HeapBuf j(640);
    char t[64]; snprintf(t, sizeof t, "{\"present\":%d,\"res\":%d,\"fresh\":%s", drvRecoveryPresent() ? 1 : 0, (int)o.res, o.fresh ? "true" : "false");
    j.add(t);
    jsonStr(j, "text", o.res ? updResText(o.res) : "");
    jsonStr(j, "msg", o.msg);
    jsonStr(j, "ver", o.ver);
    jsonStr(j, "host", cfg.hostname.c_str());
    jsonStr(j, "prefix", "https://github.com/paolellad79/VesevOS/releases/");
    j.add('}');
    updOutcomeSeen();
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });

  // MODO 2: file dal computer, senza secondo login. Risponde con l'indirizzo del recovery (con il gettone nel fragment) e riavvia.
  add(S, sec, "/api/updorder/file", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!confirmed(r)) return ko(s, tr("Serve la conferma"), "bad_request");
    char tok[33]; ErrBuf e;
    if (!updOrderFile(c.ip, tok, e)) return ko(s, e, "state");
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: riavvio nel recovery per caricare un file (%s)", w);                   // il gettone non va mai nel registro
    auditEvent(AUD_INFO, "", trf("aggiornamento tramite recovery (file dal computer) richiesto da %s", w).c_str());
    char ip[16]; netIpTo(ip, sizeof ip);
    HeapBuf j(256);
    j.add("{\"ok\":true,\"min\":15");
    char u[128];
    snprintf(u, sizeof u, "http://%s/#g=%s", cfg.hostname.c_str(), tok);
    jsonStr(j, "url", u);
    snprintf(u, sizeof u, "http://%s/#g=%s", ip, tok);
    jsonStr(j, "urlIp", u);
    j.add('}');
    memset(tok, 0, sizeof tok); memset(u, 0, sizeof u);
    s->addHeader("Cache-Control", "no-store");
    esp_err_t res = sendJson(s, j);
    sysRebootLater("recovery per aggiornamento", 800);
    return res;
  });

  // MODO 1: il recovery scarica da un indirizzo HTTPS consentito e controlla lunghezza e SHA-256.
  add(S, sec, "/api/updorder/net", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!confirmed(r)) return ko(s, tr("Serve la conferma"), "bad_request");
    long size = has(r, "size") ? Pint(r, "size") : 0;
    if (size < 0) return ko(s, tr("Dimensione non valida"));
    std::string_view url = Pt(r, "url"), sha = Pt(r, "sha"), ver = Pt(r, "ver");
    if (url.size() > 199 || sha.size() > 64 || ver.size() > 23) return ko(s, tr("Valori troppo lunghi"), "toobig");
    char u[200], h[65], v[24]; ErrBuf e;
    snprintf(u, sizeof u, "%.*s", (int)url.size(), url.data());
    snprintf(h, sizeof h, "%.*s", (int)sha.size(), sha.data());
    snprintf(v, sizeof v, "%.*s", (int)ver.size(), ver.data());
    if (!updOrderNet(u, h, v, (uint32_t)size, e)) return ko(s, e);
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: riavvio nel recovery per scaricare l'aggiornamento (%s)", w);          // l'indirizzo non va nel registro
    auditEvent(AUD_INFO, "", trf("aggiornamento tramite recovery (rete) richiesto da %s: versione %s, SHA-256 %.12s", w, v[0] ? v : "?", h).c_str());
    char ip[16], body[64]; netIpTo(ip, sizeof ip);
    snprintf(body, sizeof body, "{\"ok\":true,\"ip\":\"%s\"}", ip);
    esp_err_t res = sendJson(s, body);
    sysRebootLater("recovery per aggiornamento", 800);
    return res;
  });
}
