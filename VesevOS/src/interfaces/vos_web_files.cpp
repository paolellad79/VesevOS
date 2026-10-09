// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_files.cpp
// Percorsi del server web: rete tra schede, Bluetooth, file. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"

void routesFiles(PsychicHttpServer* S, bool sec) {
  // ---- rete tra schede (ESP-NOW) ----
  add(S, sec, "/api/mesh", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, meshJson()); });
  add(S, sec, "/api/mesh", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int role = P(r, "role").toInt();
    if (role < 0 || role > 2) return ko(s, tr("Ruolo non valido"));
    cfg.meshRole = role; cfg.meshAuto = P(r, "auto") == "1";
    if (P(r, "newkey") == "1") cfg.meshKey = meshNewKey();
    else if (P(r, "key").length()) { String k = P(r, "key"); k.trim(); k.toLowerCase(); if (k.length() != 64) return ko(s, tr("La chiave deve avere 64 cifre esadecimali")); cfg.meshKey = k; }
    if (has(r, "ch")) { int ch = P(r, "ch").toInt(); if (!regionChannelOk(ch)) return ko(s, tr("Canale non ammesso nel paese scelto")); cfg.meshCh = ch; }
    cfgSave();
    if (meshRunning()) { String e; serviceStart("mesh", e); }
    return sendJson(s, "{\"ok\":true,\"key\":\"" + cfg.meshKey + "\"}");
  });
  // la chiave e un segreto: solo POST e solo admin, mai in una GET (regole API 3-4); la visione resta nel registro
  add(S, sec, "/api/mesh/key", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { vlog("MESH: chiave mostrata a %s", ipToStr(c.ip).c_str()); return sendJson(s, "{\"key\":\"" + cfg.meshKey + "\"}"); });
  add(S, sec, "/api/mesh/run", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String a = P(r, "a"), e;
    if (a == "start") { if (!serviceStart("mesh", e)) return ko(s, e); }
    else if (a == "restart") { if (!serviceRestart("mesh", e)) return ko(s, e); }
    else if (a == "stop") serviceStop("mesh", e);
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/mesh/send", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    bool okk = P(r, "cmd").length() ? meshSendCmd(P(r, "to"), P(r, "cmd"), e) : meshSendText(P(r, "to"), P(r, "text"), e);
    return okk ? ok(s) : ko(s, e);
  });

  // ---- Bluetooth (resta acceso finche non si spegne; "min" = limite di tempo facoltativo) ----
  add(S, sec, "/api/ble", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, bleJson()); });
  add(S, sec, "/api/ble", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    bleSetLimit(has(r, "min") ? (uint32_t)max(0L, P(r, "min").toInt()) : 0);
    if (P(r, "a") == "start") { if (!serviceStart("ble", e)) return ko(s, e); }
    else if (P(r, "a") == "restart") { if (!serviceRestart("ble", e)) return ko(s, e); }
    else serviceStop("ble", e);
    return sendJson(s, bleJson());
  });

  // ---- file (memoria interna) ----
  add(S, sec, "/api/fs/list", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, fsListJson(has(r, "path") ? P(r, "path") : String("/"))); });
  add(S, sec, "/api/fs/get", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String p = fsClean(P(r, "path"));
    if (p.length() == 0 || fsProtected(p) || !drvFsExists(p)) return notFound(s);
    PsychicFileResponse f(s, drvFsHandle(), p, "application/octet-stream", true);
    return f.send();
  });
  add(S, sec, "/api/fs/text", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String t, err;
    if (!fsReadText(P(r, "path"), t, err)) return sendText(s, 404, "text/plain; charset=utf-8", err);
    return sendText(s, 200, "text/plain; charset=utf-8", t);
  });
  add(S, sec, "/api/fs/mkdir", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsMkdir(P(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsRemove(P(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/ren", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsRename(P(r, "from"), P(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsWriteText(P(r, "path"), P(r, "text"), e, P(r, "new") == "1") ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/copy", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsCopy(P(r, "from"), P(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/dirs", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, fsDirsJson()); });

  // caricamento file: controllo d'accesso fatto anche durante l'invio dei dati
  PsychicUploadHandler* up = new PsychicUploadHandler();
  up->onUpload([sec](Req* r, const String& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) return ESP_FAIL;
    int u = authSessionUser(token(r));
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return ESP_FAIL;
    if (!sec && g_tlsUp && cfg.https && !netOnAp(ip)) return ESP_FAIL;
    if (index == 0) {
      g_upOk = false; g_upErr = "";
      String dir = qparam(r, "dir"); if (!dir.length()) dir = "/";
      String name = filename; int sl = name.lastIndexOf('/'); if (sl >= 0) name = name.substring(sl + 1);
      String p = fsClean((dir == "/" ? String("") : dir) + "/" + name);
      if (p.length() == 0 || fsProtected(p)) { g_upErr = tr("Nome o percorso non valido"); return ESP_OK; }
      if (g_upFile) g_upFile.close();
      { int ls = p.lastIndexOf('/'); if (ls > 0) { String d = p.substring(0, ls); if (!drvFsExists(d)) drvFsMkdir(d); } }
      g_upFile = drvFsOpen(p, "w");
      if (!g_upFile) { g_upErr = tr("Impossibile creare il file"); return ESP_OK; }
      g_upPath = p;
    }
    if (g_upFile) {
      if (g_upFile.write(data, len) != len) { g_upErr = tr("Spazio esaurito"); g_upFile.close(); drvFsRemove(g_upPath); return ESP_OK; }
      if (final) { g_upFile.close(); g_upOk = true; vlog("FILE: caricato %s", g_upPath.c_str()); }
    }
    return ESP_OK;
  });
  up->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = authSessionUser(token(r));
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    return g_upOk ? ok(s) : ko(s, g_upErr.length() ? g_upErr : String(tr("Caricamento non riuscito")));
  });
  S->on("/api/fs/up", HTTP_POST, up);

  // Portale automatico: in modo AP ogni indirizzo sconosciuto (controlli di Android, iPhone, Windows...) porta alla pagina.
  S->onNotFound([sec](Req* r, Res* s) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) { fwNoteRejected(ip); return ESP_FAIL; }
    if (!sec && netCaptive() && !String(r->pathCStr()).startsWith("/api/")) return s->redirect("http://192.168.4.1/");
    return notFound(s);
  });
}
