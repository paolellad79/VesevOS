// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_files.cpp
// Percorsi del server web: rete tra schede, Bluetooth, file. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"

// {"ok":true,"key":"..."} (con ok = true) oppure {"key":"..."}: la chiave della rete tra schede (64 cifre esadecimali, niente da proteggere)
static esp_err_t keyJson(Res* s, bool withOk = true) {
  char j[112];
  snprintf(j, sizeof j, withOk ? "{\"ok\":true,\"key\":\"%s\"}" : "{\"key\":\"%s\"}", cfg.meshKey.c_str());
  return sendJson(s, j);
}

// stato del Bluetooth per la pagina
static esp_err_t bleReply(Res* s) {
  HeapBuf j(BLE_JSON_MAX);
  if (!bleJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
  return sendJson(s, j);
}

void routesFiles(PsychicHttpServer* S, bool sec) {
  // ---- rete tra schede (ESP-NOW) ----
  add(S, sec, "/api/mesh", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJsonBuf(s, meshJsonStd()); });
  add(S, sec, "/api/mesh", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int role = (int)Pint(r, "role");
    if (role < 0 || role > 2) return ko(s, tr("Ruolo non valido"));
    cfg.meshRole = role; cfg.meshAuto = Pis(r, "auto", "1");
    if (Pis(r, "newkey", "1")) { char nk[MESH_KEY_CHARS + 1]; meshNewKey(nk); cfg.meshKey = nk; }
    else if (*Pc(r, "key")) {                                           // chiave scelta: 64 cifre esadecimali, in minuscolo
      std::string_view k = Pt(r, "key");
      if (k.size() != 64) return ko(s, tr("La chiave deve avere 64 cifre esadecimali"));
      char key[65];
      for (size_t i = 0; i < 64; i++) key[i] = (char)tolower((unsigned char)k[i]);
      key[64] = 0;
      cfg.meshKey = key;
    }
    if (has(r, "ch")) { int ch = (int)Pint(r, "ch"); if (!regionChannelOk(ch)) return ko(s, tr("Canale non ammesso nel paese scelto")); cfg.meshCh = ch; }
    cfgSave();
    if (meshRunning()) { ErrBuf e; serviceStart("mesh", e); }
    return keyJson(s);
  });
  // la chiave e un segreto: solo POST e solo admin, mai in una GET (regole API 3-4); la visione resta nel registro
  add(S, sec, "/api/mesh/key", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { vlog("MESH: chiave mostrata a %s", IpText(c.ip).s); return keyJson(s, false); });
  add(S, sec, "/api/mesh/run", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (Pis(r, "a", "start")) { if (!serviceStart("mesh", e)) return ko(s, e); }
    else if (Pis(r, "a", "restart")) { if (!serviceRestart("mesh", e)) return ko(s, e); }
    else if (Pis(r, "a", "stop")) serviceStop("mesh", e);
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/mesh/send", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    bool okk = *Pc(r, "cmd") ? meshSendCmd(Pc(r, "to"), Pc(r, "cmd"), e) : meshSendText(Pc(r, "to"), Pc(r, "text"), e);
    return okk ? ok(s) : ko(s, e);
  });

  // ---- Bluetooth (resta acceso finche non si spegne; "min" = limite di tempo facoltativo) ----
  add(S, sec, "/api/ble", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return bleReply(s); });
  add(S, sec, "/api/ble", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    uint32_t min = has(r, "min") ? (uint32_t)std::max(0L, Pint(r, "min")) : 0;
    if (Pis(r, "a", "start")) { if (!bleUserOn(min, e)) return ko(s, e); }
    else if (Pis(r, "a", "restart")) { bleSetLimit(min); if (!serviceRestart("ble", e)) return ko(s, e); }
    else bleUserOff();
    return bleReply(s);
  });

  // ---- file (memoria interna) ----
  add(S, sec, "/api/fs/list", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(FS_JSON_MAX);                     // il JSON si scrive qui e si invia senza copiarlo in una String
    if (!fsListBuild(has(r, "path") ? std::string_view(Pc(r, "path")) : std::string_view("/"), j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/fs/get", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char p[FS_PATH_MAX];
    if (!fsCleanTo(Pc(r, "path"), p, sizeof p) || fsProtected(p) || !drvFsExists(p)) return notFound(s);
    return sendFile(s, p, "application/octet-stream", true);
  });
  add(S, sec, "/api/fs/text", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf t(fsTextMax()); ErrBuf err;
    if (!fsReadText(Pc(r, "path"), t, err)) return sendText(s, 404, "text/plain; charset=utf-8", err.c_str(), err.length());
    return sendText(s, 200, "text/plain; charset=utf-8", t.c_str(), t.length());
  });
  add(S, sec, "/api/fs/mkdir", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return fsMkdir(Pc(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return fsRemove(Pc(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/ren", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return fsRename(Pc(r, "from"), Pc(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return fsWriteText(Pc(r, "path"), Pc(r, "text"), e, Pis(r, "new", "1")) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/copy", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return fsCopy(Pc(r, "from"), Pc(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/dirs", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(FS_DIRS_MAX);
    if (!fsDirsBuild(j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });

  // caricamento file: controllo d'accesso fatto anche durante l'invio dei dati
  PsychicUploadHandler* up = new PsychicUploadHandler();
  up->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) return ESP_FAIL;
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return ESP_FAIL;
    if (!sec && g_tlsUp && cfg.https && !netOnAp(ip)) return ESP_FAIL;
    if (index == 0) {
      g_upOk = false; g_upErr.clear();
      char dir[FS_PATH_MAX], raw[2 * FS_PATH_MAX], p[FS_PATH_MAX];
      qparamTo(r, "dir", dir, sizeof dir);
      if (!dir[0]) snprintf(dir, sizeof dir, "/");
      const char* name = textOf(filename);
      const char* sl = strrchr(name, '/'); if (sl) name = sl + 1;               // solo il nome, senza cartelle
      snprintf(raw, sizeof raw, "%s/%s", strcmp(dir, "/") == 0 ? "" : dir, name);
      if (!fsCleanTo(raw, p, sizeof p) || fsProtected(p)) { g_upErr = tr("Nome o percorso non valido"); return ESP_OK; }
      if (g_upFile) g_upFile.close();
      if (const char* full = fsRoomFor(p)) { g_upErr = full; return ESP_OK; }      // cartella piena (FS_DIR_MAX voci)
      char* ls = strrchr(p, '/');                                                // cartella che contiene il file: si crea se manca
      if (ls && ls != p) {
        *ls = 0;
        if (!drvFsExists(p)) { if (const char* full = fsRoomFor(p)) { g_upErr = full; return ESP_OK; } drvFsMkdir(p); }
        *ls = '/';
      }
      g_upFile = drvFsOpen(p, "w");
      if (!g_upFile) { g_upErr = tr("Impossibile creare il file"); return ESP_OK; }
      snprintf(g_upPath, sizeof g_upPath, "%s", p);
    }
    if (g_upFile) {
      if (g_upFile.write(data, len) != len) { g_upErr = tr("Spazio esaurito"); g_upFile.close(); drvFsRemove(g_upPath); return ESP_OK; }
      if (final) { g_upFile.close(); g_upOk = true; vlog("FILE: caricato %s", g_upPath); }
    }
    return ESP_OK;
  });
  up->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    return g_upOk ? ok(s) : ko(s, g_upErr.empty() ? tr("Caricamento non riuscito") : g_upErr.c_str());
  });
  S->on("/api/fs/up", HTTP_POST, up);

  // Portale automatico: in modo AP ogni indirizzo sconosciuto (controlli di Android, iPhone, Windows...) porta alla pagina.
  S->onNotFound([sec](Req* r, Res* s) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) { fwNoteRejected(ip); return ESP_FAIL; }
    if (!sec && netCaptive() && strncmp(r->pathCStr(), "/api/", 5) != 0) return s->redirect("http://192.168.4.1/");
    return notFound(s);
  });
}
