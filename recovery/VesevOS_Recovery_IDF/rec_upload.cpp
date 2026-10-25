// VesevOS Recovery - POST /upload: riceve il .bin (multipart) e lo passa a RecImage (controlli e scrittura in app0, vedi rec_image.h).
// Serve il token del login (X-T) oppure, se VesevOS ha lasciato un ordine di modo 2, il gettone (X-G) dal suo stesso indirizzo.
#include "rec_web.h"
#include "rec_auth.h"
#include "rec_boot.h"
#include "rec_image.h"
#include "rec_led.h"
#include "rec_multipart.h"
#include "rec_order.h"
#include "rec_util.h"
#include <stdio.h>
#include <string.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Valore di boundary= nell'intestazione Content-Type (con o senza apici).
static std::string boundaryOf(const std::string& ct) {
  size_t p = ct.find("boundary=");
  if (p == std::string::npos) return "";
  std::string b = ct.substr(p + 9);
  size_t e = b.find(';');
  if (e != std::string::npos) b.resize(e);
  b = trimStr(b);
  if (b.size() >= 2 && b.front() == '"' && b.back() == '"') b = b.substr(1, b.size() - 2);
  return b;
}

esp_err_t recUploadHandler(httpd_req_t* r) {
  std::string want = trimStr(recReqHeader(r, "X-Sha"));
  for (char& c : want) c = (char)tolower((unsigned char)c);
  bool byGrant = false;
  bool authed = recAuthedReq(r, &byGrant);
  std::string denyMsg;                 // perche e stato rifiutato (solo se non autorizzato)
  RecImage img(want, byGrant ? recOrder().size : 0);
  RecMultipart mp;
  bool formOk = mp.begin(boundaryOf(recReqHeader(r, "Content-Type")), &img);
  if (authed) {
    recWebTouch();
    printf("[recovery] ricevo il file (%u byte)%s\n", (unsigned)r->content_len, byGrant ? ", con il permesso di VesevOS" : "");
  } else { denyMsg = recDenyText(r); img.fail(RES_CANCEL, "Non autorizzato"); }

  // Si legge sempre tutto il corpo (anche dopo un errore): cosi il browser riceve la risposta e non un collegamento interrotto.
  static uint8_t buf[4096];
  size_t left = r->content_len;
  bool broken = false;
  int waits = 0;
  while (left > 0) {
    int n = httpd_req_recv(r, (char*)buf, left < sizeof buf ? left : sizeof buf);
    if (n == HTTPD_SOCK_ERR_TIMEOUT && ++waits < 4) continue;        // al massimo 4 attese di 15 s
    if (n <= 0) { broken = true; break; }
    waits = 0;
    left -= n;
    if (authed && formOk && !img.failed() && !mp.feed(buf, n)) img.fail(RES_WRITE, "Modulo di caricamento non valido");
  }
  if (authed && !img.failed() && (broken || !mp.finished())) img.fail(RES_CANCEL, broken ? "Caricamento interrotto" : "Caricamento non riuscito");
  if (broken) return ESP_FAIL;

  if (!authed) return recSendJson(r, 401, recJsonErr(denyMsg));
  if (!img.done()) return recSendJson(r, 400, recJsonErr(img.err()));
  stayFlag(true, false);
  if (byGrant) { recGrantConsume(); recResult(RES_OK, "caricato dal browser"); }
  recLedSet(LS_DONE);
  recSendJson(r, 200, "{\"ok\":true,\"sha\":\"" + img.sha() + "\"}");
  printf("[recovery] firmware installato: riavvio\n");
  vTaskDelay(pdMS_TO_TICKS(600));
  esp_restart();
  return ESP_OK;
}
