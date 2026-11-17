// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_rotte.h
// Quali percorsi del server HTTPS si registrano subito e quali dopo la guida di prima configurazione.
// Senza dipendenze (si prova sul computer). Ogni percorso registrato costa circa 230 byte di RAM: sulla C3, durante la
// guida, ne servono solo una ventina; gli altri si registrano a guida finita (webRegistraRotteRestanti, vos_web.cpp).
#pragma once
#include <stdint.h>
#include <string.h>

enum RotteModo : uint8_t {
  ROTTE_TUTTE = 0,   // avvio normale: tutti i percorsi insieme
  ROTTE_GUIDA,       // finestra di prima configurazione: solo quelli della guida
  ROTTE_RESTANTI     // a guida finita: tutti gli altri
};

#define RG_GET  1
#define RG_POST 2

struct RottaGuida {
  const char* percorso;
  uint8_t     metodi;      // RG_GET, RG_POST o tutti e due
};

// Percorsi che la pagina chiama dalla schermata di accesso fino alla fine della guida (letti da web/src/js_accesso.js,
// js_base.js, js_paesi.js, js_guida.js, js_home.js e dal collegamento "Note legali" della schermata di accesso).
static const RottaGuida ROTTE_DELLA_GUIDA[] = {
  {"/",               RG_GET},                  // pagina (vos_web.cpp)
  {"/api/common",     RG_GET},                  // paesi e regole radio (vos_web.cpp)
  {"/api/auth",       RG_GET},                  // stato dell'accesso (vos_web.cpp)
  {"/api/license",    RG_GET},                  // note legali, pubbliche (vos_web.cpp)
  {"/api/langs",      RG_GET},                  // elenco lingue (vos_web.cpp)
  {"/api/langfile",   RG_GET},                  // file di una lingua (vos_web.cpp)
  {"/api/lang",       RG_POST},                 // scelta della lingua (vos_web.cpp)
  {"/api/login/start", RG_POST},                // accesso (vos_web_auth.cpp)
  {"/api/login",      RG_POST},
  {"/api/login/mfa",  RG_POST},
  {"/api/firstpass",  RG_POST},                 // prima password dell'amministratore (vos_web_auth.cpp)
  {"/api/logout",     RG_POST},
  {"/api/status",     RG_GET},                  // aggiornamento della pagina (vos_web_auth.cpp)
  {"/api/settings",   RG_GET},                  // nome della scheda per la guida (vos_web_auth.cpp)
  {"/api/ap",         RG_GET},                  // hotspot: nome e password (vos_web_dev.cpp)
  {"/api/wifi/scan",  RG_GET | RG_POST},        // ricerca delle reti (vos_web_dev.cpp)
  {"/api/wifi/test",  RG_GET | RG_POST},        // prova della Wi-Fi di casa (vos_web_dev.cpp)
  {"/api/wifi/save",  RG_POST},                 // salvataggio della Wi-Fi di casa (vos_web_dev.cpp)
  {"/api/system",     RG_POST},                 // nome della scheda (vos_web_dev.cpp)
  {"/api/region",     RG_GET | RG_POST},        // paese, antenna, fuso (vos_web_sys.cpp)
  {"/api/time/set",   RG_POST},                 // ora a mano (vos_web_sys.cpp)
  {"/api/wps",        RG_GET | RG_POST},        // WPS (vos_web_net.cpp)
  {"/api/setup/done", RG_POST},                 // fine della guida (vos_web_net.cpp)
  {"/api/fs/up",      RG_POST},                 // aggiunta di una lingua (vos_web_files.cpp)
};

static inline bool rottaDellaGuida(const char* percorso, bool post) {
  const uint8_t m = post ? RG_POST : RG_GET;
  for (const RottaGuida& r : ROTTE_DELLA_GUIDA) {
    if ((r.metodi & m) && strcmp(r.percorso, percorso) == 0) return true;
  }
  return false;
}

// Va registrato adesso questo percorso, nel modo corrente?
static inline bool rottaDaRegistrare(RotteModo modo, const char* percorso, bool post) {
  if (modo == ROTTE_TUTTE) return true;
  const bool guida = rottaDellaGuida(percorso, post);
  return modo == ROTTE_GUIDA ? guida : !guida;
}
