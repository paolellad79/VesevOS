// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_http_redirect_srv.cpp
// Risposte del server HTTP minimo. Il filtro IP (fwAllow) si applica prima di rispondere, come nel server completo.
// Limiti (driver vos_drv_tcpsrv): una connessione alla volta, al massimo 1 KB di richiesta, 3 secondi per riceverla, nessun keep-alive.
#include "vos_http_redirect_srv.h"
#include "vos_redirect_plan.h"
#include "../drivers/vos_drv_tcpsrv.h"
#include "../core/vos_fw.h"
#include "../core/vos_i18n.h"
#include <stdio.h>
#include <string.h>

static DrvTcpMini g_srv;

static const char* reason(int code) {
  switch (code) {
    case 307: return "Temporary Redirect";
    case 400: return "Bad Request";
    case 405: return "Method Not Allowed";
    default:  return "URI Too Long";           // 414
  }
}

// Scrive la risposta in out. 307: solo Location, senza corpo. Errori: testo breve (senza corpo per HEAD). Ritorna i byte scritti.
static size_t buildReply(char* out, size_t cap, int code, const char* url, const char* text, bool head) {
  const char* allow = code == 405 ? "Allow: GET, HEAD\r\n" : "";
  int n;
  if (code == 307)
    n = snprintf(out, cap, "HTTP/1.1 307 %s\r\nLocation: %s\r\nCache-Control: no-store\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", reason(code), url);
  else
    n = snprintf(out, cap, "HTTP/1.1 %d %s\r\n%sContent-Type: text/plain; charset=utf-8\r\nX-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\n"
                           "Content-Length: %u\r\nConnection: close\r\n\r\n%s", code, reason(code), allow, (unsigned)strlen(text), head ? "" : text);
  if (n > 0 && (size_t)n < cap) return (size_t)n;
  static const char TOO_LONG[] = "HTTP/1.1 414 URI Too Long\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";   // la risposta non entra: la piu corta possibile
  memcpy(out, TOO_LONG, sizeof TOO_LONG - 1);
  return sizeof TOO_LONG - 1;
}

// Legge la riga di richiesta "METODO PERCORSO HTTP/x.y" (modifica in: la spezza in pezzi chiusi con 0) e prepara la risposta.
static size_t answer(uint32_t ip, char* in, size_t n, char* out, size_t cap) {
  if (!fwAllow(ip, false)) { fwNoteRejected(ip); return 0; }       // chiude senza rispondere, come il server completo
  char* eol = strpbrk(in, "\r\n");
  if (!eol) return buildReply(out, cap, 414, "", tr("Indirizzo troppo lungo"), false);   // 1 KB senza fine riga
  *eol = 0;
  char* target = strchr(in, ' ');
  char* version = target ? strchr(target + 1, ' ') : nullptr;
  if (!version || strncmp(version + 1, "HTTP/", 5) != 0) return buildReply(out, cap, 400, "", tr("Richiesta non valida"), false);
  *target++ = 0;
  *version = 0;
  if (!*target) return buildReply(out, cap, 400, "", tr("Richiesta non valida"), false);
  bool methodOk = strcmp(in, "GET") == 0 || strcmp(in, "HEAD") == 0;
  bool head = strcmp(in, "HEAD") == 0;
  char* query = strchr(target, '?');
  if (query) *query++ = 0; else query = (char*)"";
  char url[300];
  const char* text = "";
  int code = webx::redirectPlan(methodOk, target, query, url, sizeof url, &text);
  return buildReply(out, cap, code, url, text, head);
}

bool httpRedirectSrvStart(uint16_t port) { return g_srv.begin(port, answer, "httpmin"); }
