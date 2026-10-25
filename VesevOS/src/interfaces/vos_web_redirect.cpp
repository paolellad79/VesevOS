// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_redirect.cpp
// Reindirizzamento da HTTP a HTTPS (impostazione "redirect", predefinita spenta).
// Conta solo se HTTP e HTTPS sono accesi e HTTPS e partito. Chi e collegato all'hotspot non e toccato:
// il portale automatico e la guida iniziale restano come prima.
// Sulla porta HTTP non si serve nulla: risposta 307 (temporanea) senza corpo, solo per GET e HEAD; per gli altri metodi un errore breve.
// Si usa 307 e non 308: il 308 e permanente e i browser lo ricordano anche se poi si spegne il flag o l'HTTPS non risponde.
// Il 307 mantiene il metodo, non viene memorizzato; si aggiunge anche Cache-Control: no-store. Nessun cookie.
// L'indirizzo di destinazione nasce dalle impostazioni della scheda (nome o IP) e dalla porta HTTPS,
// MAI dall'intestazione Host della richiesta (altrimenti chiunque potrebbe farsi reindirizzare dove vuole).
// Le regole stanno in redirectPlan() (vos_redirect_plan.cpp), usata anche dal server minimo della porta HTTP.
#include "vos_web_int.h"
#include "vos_redirect_plan.h"

namespace webx {

bool redirectActive(uint32_t ip) {
  return cfg.httpRedirect && cfg.httpOn && cfg.https && g_tlsUp && !netOnAp(ip);
}

esp_err_t redirectReply(Req* r, Res* s) {
  s->addHeader("Cache-Control", "no-store");
  http_method m = r->method();
  char url[300];
  const char* text = "";
  int code = redirectPlan(m == HTTP_GET || m == HTTP_HEAD, r->pathCStr(), r->queryCStr(), url, sizeof url, &text);
  if (code != 307) {
    if (code == 405) s->addHeader("Allow", "GET, HEAD");
    return sendText(s, code, "text/plain; charset=utf-8", text);
  }
  s->setCode(307);
  s->addHeader("Location", url);
  s->setContent((const uint8_t*)"", 0);
  return s->send();
}

// Percorso sconosciuto (o metodo non previsto) sul server HTTP: reindirizza o risponde "non trovato".
esp_err_t httpNotFound(Req* r, Res* s) {
  uint32_t ip = ipOf(r);
  if (!fwAllow(ip, false)) { fwNoteRejected(ip); return ESP_FAIL; }
  if (redirectActive(ip)) return redirectReply(r, s);
  return notFound(s);
}

}  // namespace webx
