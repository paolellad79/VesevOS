// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_files.cpp
// Percorsi del server web: rete tra schede, Bluetooth, file. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "vos_web_pezzi.h"
#include "../core/vos_ramwd.h"
#include "../core/vos_paths.h"
#include "../packages/vos_pacchetto_nome.h"
#include "../core/vos_i18n_carico.h"

// Il percorso definitivo di un pacchetto che ne sostituisce uno installato; intanto si scrive in "<percorso>.tmp~". Vuoto negli altri casi.
static char g_upFinale[sizeof g_upPath];

// Un file di lingua in /lang (LANGF_VLD o LANGF_VLP): si scrive in "<percorso>.tmp~", si controlla e poi prende il posto del vecchio.
static int  g_upLang = LANGF_FUORI;
static uint32_t g_upPos = 0;                                  // a pezzi: byte accettati finora
static bool g_upUltimo = false, g_upFuori = false, g_upTroppo = false;   // a pezzi: il pezzo in arrivo e l'ultimo / fuori posto / troppo grande
static char g_upLangCode[LANG_CODE_BUF];

// {"ok":true,"key":"..."} (con ok = true) oppure {"key":"..."}: la chiave della rete tra schede (64 cifre esadecimali, niente da proteggere)
static esp_err_t keyJson(Res* s, bool withOk = true) {
  char j[112];
  snprintf(j, sizeof j, withOk ? "{\"ok\":true,\"key\":\"%s\"}" : "{\"key\":\"%s\"}", cfg.meshKey.c_str());
  return sendJson(s, j);
}

// Controllo d'accesso del caricamento: vale anche durante l'invio dei dati.
static bool upAccesso(Req* r, bool sec) {
  if (!fwAllow(ipOf(r), false)) return false;
  if (csrfBlocked(r)) return false;                                          // caricamento partito da un altro sito
  int u = userOf(r);
  if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return false;
  if (!sec && g_tlsUp && cfg.https) return false;
  ramBusyTouch(60);                                                           // caricamento in corso: niente riavvio per RAM
  return true;
}

// Inizio di un file: controlla nome e cartella e apre la destinazione. Se qualcosa non va lascia il motivo in g_upErr e g_upFile chiuso.
static void upApri(Req* r, const char* filename) {
  g_upOk = false; g_upErr.clear(); g_upFinale[0] = 0; g_upLang = LANGF_FUORI;
  char dir[FS_PATH_MAX + 1], raw[2 * FS_PATH_MAX + 2], p[FS_PATH_MAX + 1];       // p: fsCleanTo vuole almeno FS_PATH_MAX + 1 byte, altrimenti rifiuta sempre
  qparamTo(r, "dir", dir, sizeof dir);
  if (!dir[0]) snprintf(dir, sizeof dir, "/");
  const char* name = filename;
  const char* sl = strrchr(name, '/'); if (sl) name = sl + 1;                   // solo il nome, senza cartelle
  snprintf(raw, sizeof raw, "%s/%s", strcmp(dir, "/") == 0 ? "" : dir, name);
  if (!fsCleanTo(raw, p, sizeof p) || fsProtected(p)) { g_upErr = tr("Nome o percorso non valido"); return; }
  if (g_upFile) g_upFile.close();
  g_upLang = langFileKind(p, g_upLangCode, sizeof g_upLangCode);
  if (g_upLang == LANGF_NO) { g_upLang = LANGF_FUORI; g_upErr = tr("In /lang si caricano solo file .vld e .vlp"); return; }
  if (const char* full = fsRoomFor(p)) { g_upErr = full; return; }              // cartella piena (FS_DIR_MAX voci)
  char* ls = strrchr(p, '/');                                                   // cartella che contiene il file: si crea se manca
  if (ls && ls != p) {
    *ls = 0;
    if (!drvFsExists(p)) { if (const char* full = fsRoomFor(p)) { g_upErr = full; return; } drvFsMkdir(p); }
    *ls = '/';
  }
  if (g_upLang != LANGF_FUORI || pacchettoDaProteggere(p)) snprintf(g_upFinale, sizeof g_upFinale, "%s", p);    // quello installato resta intatto fino al controllo
  snprintf(g_upPath, sizeof g_upPath, g_upFinale[0] ? "%s.tmp~" : "%s", p);
  g_upFile = drvFsOpen(g_upPath, "w");
  if (!g_upFile) g_upErr = tr("Impossibile creare il file");
}

// Scrive un blocco; pos = byte del file gia scritti prima di questo blocco.
static void upScrivi(uint64_t pos, const uint8_t* data, size_t len, bool final) {
  if (!g_upFile) return;
  if (g_upLang != LANGF_FUORI && pos + len > langFileMax(g_upLang)) {
    g_upErr = tr("File di lingua troppo grande"); g_upFile.close(); drvFsRemove(g_upPath); return;
  }
  if (g_upFile.write(data, len) != len) { g_upErr = tr("Spazio esaurito"); g_upFile.close(); drvFsRemove(g_upPath); return; }
  if (final) { g_upFile.close(); g_upOk = true; vlog("FILE: caricato %s", g_upPath); }
}

// Risposta alla fine di un caricamento (per intero o ultimo pezzo): controlla il file e lo mette al suo posto.
static esp_err_t upEsito(Res* s) {
  if (g_upOk && g_upLang != LANGF_FUORI) {
    ErrBuf e;
    g_upOk = false;
    if (!langFileCaricato(g_upPath, g_upFinale, g_upLang, g_upLangCode, e)) return ko(s, e);
    return ok(s);
  }
  if (g_upOk) {
    ErrBuf e, avviso;
    if (!pacchettoCaricato(g_upPath, g_upFinale[0] ? g_upFinale : g_upPath, e, avviso)) { g_upOk = false; return ko(s, e); }
    return okAvviso(s, avviso);
  }
  if (g_upFinale[0]) {                                                         // caricamento interrotto: il temporaneo non serve piu
    if (g_upFile) g_upFile.close();
    drvFsRemove(g_upPath);
  }
  return ko(s, g_upErr.empty() ? tr("Caricamento non riuscito") : g_upErr.c_str());
}

static bool upAdmin(Req* r) {
  int u = userOf(r);
  return u >= 0 && cfg.users[u].role >= ROLE_ADMIN;
}

static void routeCaricamento(PsychicHttpServer* S, bool sec) {
  // caricamento file in un solo invio (multipart)
  PsychicUploadHandler* up = new PsychicUploadHandler();
  up->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    if (!upAccesso(r, sec)) return ESP_FAIL;
    if (index == 0) upApri(r, textOf(filename));
    upScrivi(index, data, len, final);
    return ESP_OK;
  });
  up->onRequest([](Req* r, Res* s) -> esp_err_t {
    if (!upAdmin(r)) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    return upEsito(s);
  });
  S->on("/api/fs/up", HTTP_POST, up);

  // caricamento a pezzi (vedi vos_web_pezzi.h): ?dir=<cartella>&n=<nome>&o=<byte gia inviati>&e=<1 se ultimo>
  PsychicUploadHandler* pz = new PsychicUploadHandler();
  pz->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    (void)filename;
    if (!upAccesso(r, sec)) return ESP_FAIL;
    if (index == 0) {
      PezzoQ q = pezzoQuery(r);
      g_upUltimo = q.ultimo; g_upFuori = false; g_upTroppo = q.troppo;
      if (!q.troppo && q.off == 0) {
        char n[FS_PATH_MAX + 1];
        qparamTo(r, "n", n, sizeof n);
        upApri(r, n);
        g_upPos = 0;
      } else if (!q.troppo && (!g_upFile || q.off != g_upPos)) g_upFuori = true;     // non e il pezzo che si aspetta la scheda
    }
    if (g_upFuori || g_upTroppo || !g_upFile) return ESP_OK;                          // da scartare, o errore gia deciso: i dati si leggono e si buttano
    upScrivi(g_upPos, data, len, final && g_upUltimo);
    if (g_upFile || g_upOk) g_upPos += (uint32_t)len;                                 // accettato
    return ESP_OK;
  });
  pz->onRequest([](Req* r, Res* s) -> esp_err_t {
    if (!upAdmin(r)) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    if (g_upFuori) return pezzoFuoriPosto(s, g_upFile ? g_upPos : 0);
    if (g_upTroppo) return ko(s, tr("Pezzo del file troppo grande"), "toobig");
    if (g_upFile && g_upErr.empty() && !g_upOk) return pezzoAccettato(s, g_upPos);       // pezzo accettato, il file non e finito
    return upEsito(s);
  });
  S->on("/api/fs/pezzo", HTTP_POST, pz);
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

  // ---- file (memoria interna) ----
  add(S, sec, "/api/fs/list", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(FS_JSON_MAX);                     // il JSON si scrive qui e si invia senza copiarlo in una String
    if (!fsListBuild(has(r, "path") ? std::string_view(Pc(r, "path")) : std::string_view("/"), j, c.role < ROLE_ADMIN)) return ko(s, tr("Memoria insufficiente"));   // l'Operatore non vede chiavi, certificati e backup
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/fs/get", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char p[FS_PATH_MAX + 1];                    // fsCleanTo vuole almeno FS_PATH_MAX + 1 byte, altrimenti rifiuta sempre
    if (!fsCleanTo(Pc(r, "path"), p, sizeof p) || fsProtected(p) || !drvFsExists(p)) return notFound(s);
    if (c.role < ROLE_ADMIN && pathIsSecret(p)) return notFound(s);          // chiavi, certificati e backup: solo l'Admin
    return sendFile(s, p, "application/octet-stream", true);
  });
  add(S, sec, "/api/fs/text", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf t(fsTextMax()); ErrBuf err;
    char pt[FS_PATH_MAX + 1];
    if (c.role < ROLE_ADMIN && fsCleanTo(Pc(r, "path"), pt, sizeof pt) && pathIsSecret(pt)) return sendText(s, 404, "text/plain; charset=utf-8", tr("File non trovato"));   // solo l'Admin
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

  if (rottaSi("/api/fs/up", HTTP_POST)) routeCaricamento(S, sec);

  // Portale automatico: in modo AP ogni indirizzo sconosciuto (controlli di Android, iPhone, Windows...) porta alla pagina.
  if (g_rotteModo != ROTTE_RESTANTI) S->onNotFound([](Req* r, Res* s) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) { fwNoteRejected(ip); return ESP_FAIL; }
    return notFound(s);
  });
}
