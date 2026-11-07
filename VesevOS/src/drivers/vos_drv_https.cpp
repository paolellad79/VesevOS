// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_https.cpp
#include "vos_drv_https.h"
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include "vos_drv_time.h"

namespace {

// Legge il corpo fino alla fine. Un byte in piu del previsto vuol dire che il file non entra.
DrvHttpsEsito leggiCorpo(esp_http_client_handle_t c, char* buf, size_t cap, size_t& n, uint32_t tempoMs) {
  uint32_t inizio = drvMillis();
  while (true) {
    if (n >= cap - 1) {
      char resto;
      return esp_http_client_read(c, &resto, 1) > 0 ? HTTPS_E_TROPPO : HTTPS_OK;
    }
    if (drvMillis() - inizio > tempoMs) return HTTPS_E_TEMPO;
    int letti = esp_http_client_read(c, buf + n, (int)(cap - 1 - n));
    if (letti < 0) return HTTPS_E_TEMPO;
    if (letti == 0) return esp_http_client_is_complete_data_received(c) ? HTTPS_OK : HTTPS_E_TEMPO;
    n += (size_t)letti;
  }
}

// Legge il corpo a blocchi e li passa a chi li aspetta.
DrvHttpsEsito leggiBlocchi(esp_http_client_handle_t c, uint8_t* buf, size_t cap, DrvHttpsBlocco ricevi, void* ctx, uint32_t tetto, uint32_t tempoMs) {
  uint32_t inizio = drvMillis();
  uint32_t totale = 0;
  while (true) {
    if (drvMillis() - inizio > tempoMs) return HTTPS_E_TEMPO;
    int letti = esp_http_client_read(c, (char*)buf, (int)cap);
    if (letti < 0) return HTTPS_E_TEMPO;
    if (letti == 0) return esp_http_client_is_complete_data_received(c) ? HTTPS_OK : HTTPS_E_TEMPO;
    totale += (uint32_t)letti;
    if (totale > tetto) return HTTPS_E_TROPPO;
    if (!ricevi(ctx, buf, (size_t)letti)) return HTTPS_E_SCRITTURA;
  }
}

esp_http_client_handle_t nuovoClient(const char* url) {
  esp_http_client_config_t conf = {};
  conf.url = url;
  conf.crt_bundle_attach = esp_crt_bundle_attach;
  conf.timeout_ms = 15000;
  conf.buffer_size = 2048;                  // alcune righe di intestazione di GitHub sono lunghe
  conf.buffer_size_tx = 1024;
  conf.disable_auto_redirect = true;
  conf.keep_alive_enable = false;
  return esp_http_client_init(&conf);
}

}  // namespace

DrvHttpsEsito drvHttpsScarica(const char* url, char* buf, size_t cap, size_t& n, uint32_t tempoMs, int& statoHttp) {
  n = 0;
  statoHttp = 0;
  if (cap) buf[0] = 0;
  esp_http_client_handle_t c = nuovoClient(url);
  if (!c) return HTTPS_E_MEMORIA;

  DrvHttpsEsito esito = HTTPS_E_COLLEGAMENTO;
  if (esp_http_client_open(c, 0) == ESP_OK) {
    int64_t annunciati = esp_http_client_fetch_headers(c);
    statoHttp = esp_http_client_get_status_code(c);
    if (statoHttp != 200) esito = HTTPS_E_RISPOSTA;
    else if (annunciati >= (int64_t)cap) esito = HTTPS_E_TROPPO;
    else esito = leggiCorpo(c, buf, cap, n, tempoMs);
    esp_http_client_close(c);
  }
  esp_http_client_cleanup(c);
  if (esito != HTTPS_OK) n = 0;
  if (cap) buf[n] = 0;
  return esito;
}

DrvHttpsEsito drvHttpsBlocchi(const char* url, uint8_t* buf, size_t cap, DrvHttpsBlocco ricevi, void* ctx, uint32_t tetto, uint32_t tempoMs, int& statoHttp) {
  statoHttp = 0;
  esp_http_client_handle_t c = nuovoClient(url);
  if (!c) return HTTPS_E_MEMORIA;

  DrvHttpsEsito esito = HTTPS_E_COLLEGAMENTO;
  if (esp_http_client_open(c, 0) == ESP_OK) {
    int64_t annunciati = esp_http_client_fetch_headers(c);
    statoHttp = esp_http_client_get_status_code(c);
    if (statoHttp != 200) esito = HTTPS_E_RISPOSTA;
    else if (annunciati > (int64_t)tetto) esito = HTTPS_E_TROPPO;
    else esito = leggiBlocchi(c, buf, cap, ricevi, ctx, tetto, tempoMs);
    esp_http_client_close(c);
  }
  esp_http_client_cleanup(c);
  return esito;
}
