// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_smista.cpp
// Rotta unica: add() non crea piu un oggetto per percorso, ma scrive una voce nella tabella compatta (vos_web_tabella.h).
// Al server si danno solo due percorsi jolly (uno per GET, uno per POST) con una funzione di confronto che guarda la tabella:
// un percorso o un metodo che non c'e non corrisponde, quindi il server risponde 404 come prima (e non carica il corpo).
// Controlli di sicurezza identici a prima: filtro IP, richiesta da un altro sito, accesso, ruolo minimo.
// Un solo server (l'HTTPS): la tabella e unica e c.secure vale sempre quello del primo server registrato.
#include "vos_web_int.h"
#include "vos_web_tabella.h"
#include "../core/vos_apwin.h"
#include "../core/vos_azioni.h"
#include "../drivers/vos_drv_time.h"

#define ROTTE_MAX 208   // 177 percorsi oggi (checkapi): margine per quelli nuovi

namespace webx {

RotteModo g_rotteModo = ROTTE_TUTTE;
int g_rotteConta = 0;                            // percorsi davvero registrati nel giro in corso (per il registro)
bool rottaSi(const char* path, int method) { return rottaDaRegistrare(g_rotteModo, path, method == HTTP_POST); }

static TabellaRotte<Handler, ROTTE_MAX> g_tab;
static PsychicHttpServer* g_srv = nullptr;       // server a cui sono agganciati i due percorsi jolly
static bool g_secure = true;

static uint8_t bitDi(int method) { return method == HTTP_POST ? RG_POST : RG_GET; }

// Funzioni di confronto del server (stessa firma di httpd_uri_match_func_t): len e la lunghezza senza "?...".
static bool corrispondeGet(const char*, const char* uri, size_t len) { return g_tab.cerca(uri, len, RG_GET) != nullptr; }
static bool corrispondePost(const char*, const char* uri, size_t len) { return g_tab.cerca(uri, len, RG_POST) != nullptr; }

static esp_err_t smista(Req* r, Res* s, int method) {
  const char* path = r->pathCStr();
  const TabellaRotte<Handler, ROTTE_MAX>::Voce* v = g_tab.cerca(path, strlen(path), bitDi(method));
  if (!v) return HTTPD_404_NOT_FOUND;              // non succede: il confronto ha gia detto si
  const uint8_t lvl = v->livello;
  Ctx c; c.ip = ipOf(r); c.user = -1; c.role = 0; c.secure = g_secure;
  g_lastReq = drvMillis();
  if (!fwAllow(c.ip, false)) { fwNoteRejected(c.ip); return ESP_FAIL; }     // chiude senza rispondere
  if (method != HTTP_GET && csrfBlocked(r)) return sendJson(s, "{\"ok\":false,\"err\":\"richiesta da un altro sito\",\"code\":\"csrf\"}", 403);   // POST e simili: solo dalla nostra pagina
  if (lvl > L_PUB) {
    char tk[AUTH_TOKEN_BUF]; tokenTo(r, tk, sizeof tk);
    int u = authSessionUser(tk);
    if (u < 0) { authNoteDenied(c.ip); return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\",\"code\":\"unauthorized\"}", 401); }
    c.user = u; c.role = cfg.users[u].role;
    apWinAttivita();                                // guida da finire: ogni richiesta con accesso rinnova la finestra del tasto BOOT
    authSessionNote(tk, c.ip, g_secure, method == HTTP_POST, r->pathCStr());   // per "chi e collegato" (who)
    if (c.role + 1 < lvl) return ko(s, tr("Il tuo ruolo non permette questa operazione"), "forbidden");   // HTTP 403
  }
  if (method == HTTP_POST) { char w[96]; whoTo(c, w, sizeof w); cfgSetOrigin(w); }
  AzioneDa da(AO_PAGINA);                          // le azioni fatte da questa richiesta portano l'origine "pagina"
  return v->funzione(r, s, c);
}
static esp_err_t smistaGet(Req* r, Res* s) { return smista(r, s, HTTP_GET); }
static esp_err_t smistaPost(Req* r, Res* s) { return smista(r, s, HTTP_POST); }

// Una volta sola per server, PRIMA di start(): i due percorsi jolly.
void rotteAgganciaServer(PsychicHttpServer* srv, bool secure) {
  if (g_srv) {
    if (g_srv != srv) vlog("WEB: ERRORE tabella dei percorsi gia legata a un altro server");
    return;
  }
  g_srv = srv;
  g_secure = secure;
  srv->on("*", HTTP_GET, smistaGet)->setURIMatchFunction(corrispondeGet);
  srv->on("*", HTTP_POST, smistaPost)->setURIMatchFunction(corrispondePost);
}

// Registra un percorso con livello di accesso. secure = server HTTPS.
void add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h) {
  if (!rottaSi(path, method)) return;              // percorso rimandato (finestra della guida) o gia registrato
  rotteAgganciaServer(srv, secure);
  if (!g_tab.aggiungi(path, bitDi(method), lvl, h)) { vlog("WEB: ERRORE tabella dei percorsi piena (%d), manca %s", (int)ROTTE_MAX, path); return; }
  g_rotteConta++;
}

int rotteInTabella() { return g_tab.conta(); }

}  // namespace webx
