// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_tema.cpp
// GET /tema.css (pubblica, serve alla schermata di accesso): il tema della pagina, /etc/tema.css, controllato con le regole di core/vos_tema_rule.h
// a ogni richiesta (file piccolo, al massimo 32 KB, letto a pezzi). Se il file manca, e vuoto o non e valido, la risposta e un commento: aspetto di
// fabbrica. Un file non valido non si invia MAI. L'intestazione X-VOS-Tema dice cosa e successo (ok, fabbrica, o il motivo del rifiuto).
#include <atomic>
#include "vos_web_int.h"
#include "../core/vos_tema_rule.h"
#include "../drivers/vos_drv_file.h"

namespace webx {

namespace {

const size_t TEMA_PEZZO = 1024;
const char   TEMA_FABBRICA[] = "/* nessun tema: aspetto di fabbrica */\n";

// Legge il file a pezzi e lo controlla. TEMA_VUOTO = manca o e vuoto. len = byte del file se e valido.
TemaEsito temaControllaFile(uint8_t* buf, size_t& len) {
  len = 0;
  VosFile f = drvFsOpen(TEMA_PERCORSO, "r");
  if (!f) return TEMA_VUOTO;
  TemaScan sc;
  temaScanAvvia(sc);
  for (;;) {
    size_t n = f.read(buf, TEMA_PEZZO);
    if (n == 0) break;
    if (temaScanDai(sc, (const char*)buf, n) != TEMA_OK) break;
  }
  TemaEsito e = temaScanFine(sc);
  if (e == TEMA_OK) len = sc.tot;
  return e;
}

esp_err_t inviaTema(Res* s, uint8_t* buf, size_t len) {
  VosFile f = drvFsOpen(TEMA_PERCORSO, "r");
  if (!f) return sendText(s, 200, "text/css; charset=utf-8", TEMA_FABBRICA);
  s->setCode(200);
  s->setContentType("text/css; charset=utf-8");
  s->addHeader("Cache-Control", "no-cache");
  s->addHeader("X-Content-Type-Options", "nosniff");
  s->addHeader("X-VOS-Tema", "ok");
  s->sendHeaders();
  esp_err_t err = ESP_OK;
  size_t resto = len;
  while (resto > 0 && err == ESP_OK) {
    size_t voglio = resto < TEMA_PEZZO ? resto : TEMA_PEZZO;
    size_t letti = f.read(buf, voglio);
    if (letti == 0) { err = ESP_FAIL; break; }      // il file e cambiato mentre lo inviavo: risposta troncata, il browser la richiede
    err = s->sendChunk(buf, letti);
    resto -= letti;
  }
  return err == ESP_OK ? s->finishChunking() : err;
}

}  // namespace

esp_err_t sendTema(Res* s) {
  uint8_t* buf = (uint8_t*)malloc(TEMA_PEZZO);
  if (!buf) return sendText(s, 200, "text/css; charset=utf-8", TEMA_FABBRICA);   // senza memoria: aspetto di fabbrica, la pagina funziona lo stesso
  size_t len = 0;
  TemaEsito e = temaControllaFile(buf, len);
  esp_err_t err;
  if (e == TEMA_OK) {
    err = inviaTema(s, buf, len);
  } else {
    static std::atomic<uint8_t> ultimoRifiuto{0};    // un solo messaggio nel registro per tipo di rifiuto, non uno a ogni pagina
    if (e != TEMA_VUOTO && ultimoRifiuto.exchange((uint8_t)e) != (uint8_t)e)
      vlog("ATTENZIONE: tema della pagina rifiutato (%s): resta l'aspetto di fabbrica", temaEsitoNome(e));
    if (e == TEMA_VUOTO) ultimoRifiuto = 0;
    s->addHeader("X-VOS-Tema", temaEsitoNome(e));
    err = sendText(s, 200, "text/css; charset=utf-8", TEMA_FABBRICA);
  }
  free(buf);
  return err;
}

}  // namespace webx
