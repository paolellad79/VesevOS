// VesevOS Recovery - server web (vedi rec_web.h).
#include "rec_web.h"
#include "rec_auth.h"
#include "rec_boot.h"
#include "rec_config.h"
#include "rec_util.h"
#include "rec_version.h"
#include "rec_order.h"
#include "recovery_page.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t g_lastAct = 0;

void recWebTouch() { g_lastAct = recMillis(); }
uint32_t recWebLastActivity() { return g_lastAct; }

std::string recReqHeader(httpd_req_t* r, const char* name) {
  size_t n = httpd_req_get_hdr_value_len(r, name);
  if (n == 0 || n > 600) return "";
  std::string v(n + 1, '\0');
  if (httpd_req_get_hdr_value_str(r, name, &v[0], n + 1) != ESP_OK) return "";
  v.resize(n);
  return v;
}

static const char* statusText(int code) {
  switch (code) {
    case 200: return "200 OK";
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 429: return "429 Too Many Requests";
    default:  return "500 Internal Server Error";
  }
}

static void securityHeaders(httpd_req_t* r) {
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
}

esp_err_t recSendJson(httpd_req_t* r, int status, const std::string& json) {
  securityHeaders(r);
  httpd_resp_set_status(r, statusText(status));
  httpd_resp_set_type(r, "application/json");
  return httpd_resp_send(r, json.c_str(), json.size());
}

// Corpo di una richiesta (moduli piccoli: utente, sfida, codice).
static std::string readBody(httpd_req_t* r) {
  std::string body;
  size_t left = r->content_len;
  if (left > 1024) left = 1024;
  char buf[256];
  while (left > 0) {
    int n = httpd_req_recv(r, buf, left < sizeof buf ? left : sizeof buf);
    if (n <= 0) break;
    body.append(buf, n);
    left -= n;
  }
  return body;
}

// IPv4 di chi ha fatto la richiesta (ordine di rete); 0 se non si sa.
static uint32_t clientIp(httpd_req_t* r) {
  struct sockaddr_in a;
  socklen_t l = sizeof a;
  if (getpeername(httpd_req_to_sockfd(r), (struct sockaddr*)&a, &l) != 0 || a.sin_family != AF_INET) return 0;
  return a.sin_addr.s_addr;
}

bool recAuthedReq(httpd_req_t* r, bool* byGrant) {
  if (byGrant) *byGrant = false;
  if (recAuthed(recReqHeader(r, "X-T"))) return true;
  std::string g = recReqHeader(r, "X-G");
  if (g.empty() || !recGrantCheck(g, clientIp(r))) return false;
  if (byGrant) *byGrant = true;
  return true;
}

static bool authed(httpd_req_t* r) { return recAuthedReq(r, nullptr); }

// ---- gestori ----
// Accesso negato: se il browser aveva mandato un gettone dice perche (scaduto, usato...), altrimenti chiede il login.
std::string recDenyText(httpd_req_t* r) {
  if (recReqHeader(r, "X-G").empty()) return "Serve il login (utente e password di un amministratore)";
  return recGrantDenyText();
}

static esp_err_t denied(httpd_req_t* r) { return recSendJson(r, 401, recJsonErr(recDenyText(r))); }

static esp_err_t hRoot(httpd_req_t* r) {
  recWebTouch();
  securityHeaders(r);
  httpd_resp_set_type(r, "text/html");
  return httpd_resp_send(r, REC_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t hVer(httpd_req_t* r) {
  RecReply a = recAuthVer();
  return recSendJson(r, a.status, a.json);
}

static esp_err_t hStart(httpd_req_t* r) {
  recWebTouch();
  RecReply a = recAuthStart(formGet(readBody(r), "u"));
  return recSendJson(r, a.status, a.json);
}

static esp_err_t hLogin(httpd_req_t* r) {
  recWebTouch();
  RecReply a = recAuthLogin(readBody(r));
  return recSendJson(r, a.status, a.json);
}

static esp_err_t hInfo(httpd_req_t* r) {
  readBody(r);
  if (!authed(r)) return denied(r);
  recWebTouch();
  char n[40], v[40];
  std::string a;
  if (app0Valid(n, sizeof n, v, sizeof v)) a = app0Fw() ? std::string(n) + " " + v : "NON e VesevOS (manca il marchio)";
  const esp_partition_t* p = recApp0();
  return recSendJson(r, 200, "{\"ok\":true,\"app\":\"" + a + "\",\"max\":" + std::to_string(p ? (unsigned long)p->size : 0UL) + "}");
}

static esp_err_t hBoot(httpd_req_t* r) {
  readBody(r);
  bool byGrant = false;
  if (!recAuthedReq(r, &byGrant)) return denied(r);
  char n[40], v[40];
  if (!app0Valid(n, sizeof n, v, sizeof v)) return recSendJson(r, 409, recJsonErr("Non c'e un firmware valido da avviare"));
  if (!app0Fw()) return recSendJson(r, 409, recJsonErr("Nello slot non c'e VesevOS (manca il marchio): carica il firmware"));
  recSendJson(r, 200, "{\"ok\":true}");
  // L'avvio consuma il gettone: dopo il riavvio non serve piu (l'ordine e gia stato cancellato dalla NVS) e cosi nessun'altra
  // richiesta puo usarlo nei 300 ms prima dell'avvio. Si consuma solo qui, dopo i controlli: se in app0 non c'e VesevOS il gettone
  // resta buono per caricare il firmware.
  if (byGrant) { recGrantConsume(); recResult(RES_CANCEL, "avvio senza aggiornare"); }
  vTaskDelay(pdMS_TO_TICKS(300));
  startApp();
  return ESP_OK;
}

// Rotta sconosciuta o metodo sbagliato: come prima, risposta JSON 404.
static esp_err_t hNotFound(httpd_req_t* r, httpd_err_code_t) {
  recSendJson(r, 404, recJsonErr("Non trovato"));
  return ESP_OK;
}

void recWebStart() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 8192;
  cfg.max_uri_handlers = 8;
  cfg.max_open_sockets = 4;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 15;
  cfg.send_wait_timeout = 15;
  httpd_handle_t srv = nullptr;
  if (httpd_start(&srv, &cfg) != ESP_OK) { printf("[recovery] ERRORE: il server web non parte\n"); return; }
  const httpd_uri_t routes[] = {
    {"/",          HTTP_GET,  hRoot,            nullptr},
    {"/api/ver",   HTTP_GET,  hVer,             nullptr},
    {"/api/start", HTTP_POST, hStart,           nullptr},
    {"/api/login", HTTP_POST, hLogin,           nullptr},
    {"/api/info",  HTTP_POST, hInfo,            nullptr},
    {"/api/boot",  HTTP_POST, hBoot,            nullptr},
    {"/upload",    HTTP_POST, recUploadHandler, nullptr},
  };
  for (const httpd_uri_t& u : routes) httpd_register_uri_handler(srv, &u);
  httpd_register_err_handler(srv, HTTPD_404_NOT_FOUND, hNotFound);
  httpd_register_err_handler(srv, HTTPD_405_METHOD_NOT_ALLOWED, hNotFound);
  recWebTouch();
}
