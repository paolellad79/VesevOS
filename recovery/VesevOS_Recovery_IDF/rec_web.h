// VesevOS Recovery - server web (esp_http_server). Rotte uguali alla 0.4.4:
//   GET /  pagina | GET /api/ver | POST /api/start | /api/login | /api/info | /api/boot | /upload
#pragma once
#include <stdint.h>
#include <string>
#include <esp_http_server.h>

void recWebStart();
uint32_t recWebLastActivity();          // recMillis() dell'ultimo accesso utile
void recWebTouch();

// Aiuti per i gestori delle rotte (usati anche da rec_upload.cpp).
std::string recReqHeader(httpd_req_t* r, const char* name);
// Richiesta autorizzata? Con il token del login (X-T) oppure, in modo 2, con il gettone di VesevOS (X-G) e l'IP giusto. *byGrant dice quale.
bool recAuthedReq(httpd_req_t* r, bool* byGrant);
// Frase per la pagina quando la richiesta non e autorizzata (manca il login, oppure il gettone e scaduto, usato, annullato o sbagliato).
std::string recDenyText(httpd_req_t* r);
esp_err_t recSendJson(httpd_req_t* r, int status, const std::string& json);
esp_err_t recUploadHandler(httpd_req_t* r);      // POST /upload (rec_upload.cpp)
