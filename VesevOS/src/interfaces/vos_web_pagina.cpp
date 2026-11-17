// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_pagina.cpp
// La pagina vera sta nel file /usr/share/vesevos/html.vrs (non e piu dentro il firmware, piano 15/11/2026, passo 5). Qui si controlla il file e si invia al
// browser a pezzi piccoli. Se il file manca o non e valido, "/" risponde con la pagina di emergenza (vos_web_emergenza.cpp): un file non valido
// non si invia MAI. Lo stesso se il file della lingua in uso e in errore (langInErrore, core/vos_i18n.h): cosi chi ha il file rotto non resta con testi sbagliati. Una sola pagina alla volta (la seconda richiesta riceve 503 con "Retry-After"): il buffer e allocato per richiesta e sulla C3
// (senza PSRAM) non se ne vogliono due insieme; aspettare terrebbe fermo il task del server.
#include <atomic>
#include "vos_web_int.h"
#include "../core/vos_risorse.h"

namespace webx {

namespace {

const size_t PAGINA_PEZZO = 2048;           // buffer di lettura e dimensione di ogni pezzo inviato (la C3 non ha PSRAM)
const char   PAGINA_NOME[] = "html";

std::atomic<bool> g_verificata{false};      // lo SHA-256 completo e gia stato controllato dopo l'avvio (si ricorda in RAM)
std::atomic<bool> g_inUso{false};           // una pagina e in invio

// Il file e valido? Controllo veloce dell'intestazione a ogni richiesta; SHA-256 completo solo la prima volta dopo l'avvio (o dopo un
// caricamento nuovo). Se vero, f resta aperto sul primo byte dei dati e info dice lunghezza e codifica.
bool paginaPrincipaleOk(VosFile& f, RisInfo& info) {
  if (risApri(PAGINA_NOME, f, info) != RIS_OK) {
    g_verificata = false;
    return false;
  }
  if (g_verificata) return true;
  f.close();
  if (risControlla(PAGINA_NOME) != RIS_OK) return false;
  if (risApri(PAGINA_NOME, f, info) != RIS_OK) return false;
  g_verificata = true;
  vlog("PAGINA: file /usr/share/vesevos/html.vrs controllato (SHA-256 giusto)");
  return true;
}

// Libera g_inUso in ogni uscita.
struct PosoPagina {
  bool preso;
  PosoPagina() {
    bool libero = false;
    preso = g_inUso.compare_exchange_strong(libero, true);
  }
  ~PosoPagina() {
    if (preso) g_inUso = false;
  }
};

esp_err_t inviaPagina(Res* s, VosFile& f, const RisInfo& info) {
  uint8_t* buf = (uint8_t*)malloc(PAGINA_PEZZO);
  if (!buf) return ko(s, tr("Memoria insufficiente"), "off");
  s->setCode(200);
  s->setContentType("text/html; charset=utf-8");
  if (info.codifica == 1) s->addHeader("Content-Encoding", "gzip");
  s->addHeader("Cache-Control", "no-store");
  s->sendHeaders();
  esp_err_t err = ESP_OK;
  size_t resto = info.lunghezza;
  while (resto > 0 && err == ESP_OK) {
    size_t voglio = resto < PAGINA_PEZZO ? resto : PAGINA_PEZZO;
    size_t letti = f.read(buf, voglio);
    if (letti == 0) {                            // il file e finito troppo presto: la risposta resta troncata e si richiede il controllo
      err = ESP_FAIL;
      break;
    }
    err = s->sendChunk(buf, letti);
    resto -= letti;
  }
  free(buf);
  if (err == ESP_OK) return s->finishChunking();
  g_verificata = false;
  return err;
}

}  // namespace

// Dopo un caricamento riuscito di un file .vrs: il prossimo "/" ricontrolla il file per intero.
void paginaEsitoAzzera() { g_verificata = false; }

esp_err_t sendPage(Res* s) {
  PosoPagina poso;
  if (!poso.preso) {
    s->addHeader("Retry-After", "2");
    return sendText(s, 503, "text/plain; charset=utf-8", tr("Pagina in invio a un altro dispositivo: riprova tra un attimo"));
  }
  VosFile f;
  RisInfo info;
  if (!paginaPrincipaleOk(f, info)) return sendEmergenza(s);
  if (langInErrore()) {                          // il file della lingua in uso non e valido: stessa pagina di emergenza, con l'avviso sulla lingua
    f.close();
    return sendEmergenza(s, true);
  }
  esp_err_t err = inviaPagina(s, f, info);
  f.close();
  return err;
}

}  // namespace webx
