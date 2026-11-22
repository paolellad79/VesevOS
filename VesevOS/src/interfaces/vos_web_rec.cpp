// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_rec.cpp
// Pagina Registratore: stato e punti per il grafico (Operatore), impostazioni e cancellazione dei dati (Admin).
// Il file CSV si scarica con la rotta dei file, /api/fs/get?path=/var/lib/rec/<nome>.csv (o /media/sd/rec/...), Operatore.
#include "vos_web_int.h"
#if VOS_WITH_REC
#include "../packages/vos_rec.h"
#include "../packages/vos_rec_file.h"

namespace {

esp_err_t sendRec(Res* s, bool conPunti) {
  HeapBuf j(REC_JSON_MAX);
  if (!recJson(j, conPunti)) return ko(s, tr("Memoria insufficiente"));
  return sendJson(s, j);
}

// Parte dalle impostazioni attuali e cambia solo cio che la richiesta porta.
void leggi(Req* r, RecSettings& v) {
  recSettingsGet(v);
  if (has(r, "on")) v.on = Pis(r, "on", "1");
  if (has(r, "sec")) { long n = Pint(r, "sec"); v.sec = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
  if (has(r, "where")) { long n = Pint(r, "where"); v.where = (uint8_t)(n < 0 || n > 1 ? 9 : n); }
  if (has(r, "maxkb")) { long n = Pint(r, "maxkb"); v.maxKb = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
  if (has(r, "nome")) {
    const char* n = Pc(r, "nome");
    if (strlen(n) > REC_NOME_MAX) snprintf(v.nome, sizeof v.nome, "!");      // troppo lungo: il controllo di recSet lo rifiuta
    else snprintf(v.nome, sizeof v.nome, "%s", n);
  }
  if (has(r, "limite")) { long n = Pint(r, "limite"); v.limite = (uint8_t)(n < 0 || n > 1 ? 9 : n); }
  if (has(r, "azioni")) { long n = Pint(r, "azioni"); v.azioni = (uint16_t)(n < 0 || n > 0x7FF ? 0xFFFF : n); }
  if (has(r, "evmax")) { long n = Pint(r, "evmax"); v.evMax = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
  if (has(r, "sect")) { long n = Pint(r, "sect"); v.secT = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
  if (has(r, "secr")) { long n = Pint(r, "secr"); v.secR = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
  if (has(r, "t")) v.t = Pis(r, "t", "1");
  if (has(r, "r")) v.r = Pis(r, "r", "1");
  for (int i = 0; i < REC_DEV_MAX; i++) {
    char key[8];
    snprintf(key, sizeof key, "dev%d", i + 1);
    char ks[8];
    snprintf(ks, sizeof ks, "sec%d", i + 1);
    if (has(r, ks)) { long n = Pint(r, ks); v.secDev[i] = (uint16_t)(n < 0 ? 0 : (n > 65535 ? 65535 : n)); }
    if (!has(r, key)) continue;
    const char* n = Pc(r, key);
    if (strlen(n) > 15) { v.dev[i][0] = '#'; v.dev[i][1] = 0; continue; }      // troppo lungo: il controllo di recSet lo rifiuta
    snprintf(v.dev[i], sizeof v.dev[i], "%s", n);
  }
}

}  // namespace

void routesRec(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/rec", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendRec(s, true); });
  add(S, sec, "/api/rec/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    RecSettings v;
    leggi(r, v);
    ErrBuf e;
    recSet(v, e);
    if (e.length()) return ko(s, e);
    cfgSetOrigin("registratore");
    cfgSave();
    return sendRec(s, false);
  });
  // Rotta POST /api/rec/clear (Admin) tolta il 19/11: la pagina cancella i file scelti nella scheda "Dati salvati".
  // Commentata per ora, non registrata. Per rimetterla: add(S, sec, <percorso>, HTTP_POST, L_ADMIN, ...) che chiama
  // recClear(e) e risponde ko(s, e) se e.length() altrimenti ok(s). La shell usa ancora `rec clear` (recClear).
}
#endif
