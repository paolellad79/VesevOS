// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_eventi.cpp
// Registro circolare degli eventi di sicurezza. Il blocco e l'ultimo della catena: chi lo prende puo avere gia quello dell'autenticazione,
// e qui dentro non si chiama nient'altro che prenda blocchi (niente registro, niente tr(), niente allocazioni).
// Questo file sta sotto vos_fw: non deve includere vos_time, vos_util o altro che arrivi fino a lui (lo controlla checklayers).
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <new>
#include "vos_eventi.h"
#include "vos_heapbuf.h"
#include "vos_lock.h"
#include "../drivers/vos_drv_time.h"

namespace {

constexpr uint32_t UNIONE_MS = 600000UL;           // voci uguali entro 10 minuti si sommano
constexpr uint32_t GIORNO_S = 86400UL;
constexpr size_t NOME_BUF = 21;                    // come il nome di un utente
constexpr time_t ORA_MINIMA = 1700000000;          // stesso confine di timeValid(): prima di novembre 2023 l'ora non e stata impostata

struct Voce {
  uint32_t epoca;                                  // ultimo evento; 0 = ora non valida in quel momento
  uint32_t ms;                                     // ultimo evento, in millisecondi dall'avvio
  uint32_t ip;
  uint16_t volte;                                  // 0 = voce vuota
  uint8_t tipo;
  uint8_t canale;
  char nome[NOME_BUF];
};

VosMutex s_blocco;
const bool s_bloccoPronto = (s_blocco.init(), true);    // si crea prima dei task, come il blocco dell'autenticazione
Voce s_voci[EVENTI_VOCI];
uint8_t s_prossima = 0;
uint32_t s_totali[EV_TIPI];
uint32_t s_blocchiNuovi = 0;

Voce* cercaUguale(EventoTipo tipo, EventoCanale canale, uint32_t ip, const char* nome, uint32_t adesso) {
  for (Voce& v : s_voci) {
    if (!v.volte || v.tipo != tipo || v.canale != canale || v.ip != ip) continue;
    if (strcmp(v.nome, nome) != 0 || adesso - v.ms > UNIONE_MS) continue;
    return &v;
  }
  return nullptr;
}

uint32_t epocaValida() {
  time_t adesso = time(nullptr);
  return adesso > ORA_MINIMA ? (uint32_t)adesso : 0;
}

void testoIp(uint32_t ip, char* out, size_t n) {
  out[0] = 0;
  if (ip) snprintf(out, n, "%u.%u.%u.%u", (unsigned)((ip >> 24) & 255), (unsigned)((ip >> 16) & 255), (unsigned)((ip >> 8) & 255), (unsigned)(ip & 255));
}

}  // namespace

void eventoNota(EventoTipo tipo, EventoCanale canale, uint32_t ip, const char* nome) {
  if (tipo >= EV_TIPI) return;
  if (!nome) nome = "";
  uint32_t adesso = drvMillis();
  uint32_t epoca = epocaValida();
  VosLock chiuso(s_blocco);
  s_totali[tipo]++;
  if (tipo == EV_BLOCCO) s_blocchiNuovi++;
  Voce* v = cercaUguale(tipo, canale, ip, nome, adesso);
  if (!v) {
    v = &s_voci[s_prossima];
    s_prossima = (uint8_t)((s_prossima + 1) % EVENTI_VOCI);
    memset(v, 0, sizeof(Voce));
    v->tipo = tipo; v->canale = canale; v->ip = ip;
    snprintf(v->nome, sizeof v->nome, "%s", nome);
  }
  if (v->volte < UINT16_MAX) v->volte++;
  v->epoca = epoca; v->ms = adesso;
}

bool eventiJson(HeapBuf& j) {
  // Prima la copia, poi il testo: la formattazione puo chiedere memoria e non deve tenere fermo chi registra un errore.
  Voce* copia = new (std::nothrow) Voce[EVENTI_VOCI];
  if (!copia) return false;
  uint32_t totali[EV_TIPI];
  {
    VosLock chiuso(s_blocco);
    memcpy(copia, s_voci, sizeof s_voci);
    memcpy(totali, s_totali, sizeof s_totali);
  }
  uint32_t adesso = drvMillis();
  uint32_t epoca = epocaValida(), ultime24 = 0;
  bool ora = epoca != 0;
  char ip[16];
  j.add("{\"ok\":true,\"lista\":[");
  bool primo = true;
  for (int i = 0; i < EVENTI_VOCI; i++) {
    const Voce& v = copia[i];
    if (!v.volte) continue;
    if (ora && v.epoca && epoca - v.epoca < GIORNO_S) ultime24 += v.volte;
    testoIp(v.ip, ip, sizeof ip);
    j.addf("%s{\"k\":%u,\"c\":%u,\"i\":\"%s\",\"u\":\"", primo ? "" : ",", (unsigned)v.tipo, (unsigned)v.canale, ip);
    j.addJson(v.nome);
    j.addf("\",\"n\":%u,\"q\":%lu,\"s\":%lu}", (unsigned)v.volte, (unsigned long)v.epoca, (unsigned long)((adesso - v.ms) / 1000UL));
    primo = false;
  }
  delete[] copia;
  j.addf("],\"ora\":%s,\"ult24\":%lu,\"tot\":[", ora ? "true" : "false", (unsigned long)ultime24);
  for (int t = 0; t < EV_TIPI; t++) j.addf("%s%lu", t ? "," : "", (unsigned long)totali[t]);
  j.add("]}");
  return !j.oom() && !j.over();
}

void eventiSvuota() {
  VosLock chiuso(s_blocco);
  memset(s_voci, 0, sizeof s_voci);
  memset(s_totali, 0, sizeof s_totali);
  s_prossima = 0;
  s_blocchiNuovi = 0;
}

uint32_t eventiBlocchiNuovi() {
  VosLock chiuso(s_blocco);
  return s_blocchiNuovi;
}

void eventiBlocchiVisti() {
  VosLock chiuso(s_blocco);
  s_blocchiNuovi = 0;
}
