// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_errore.cpp
// Stato "lingua in errore" in RAM (pochi byte) e il suo unico avviso. Vedi vos_i18n_int.h per le funzioni interne.
// L'avviso (registro, allarme giallo) NON si da dentro tr(): tr() puo essere chiamata mentre l'audit o il registro tengono un blocco,
// quindi qui si alza solo un segno e lo da langTick(), chiamato dal ciclo principale (come risorseTick).
#include "vos_i18n.h"
#include "vos_i18n_int.h"
#include "vos_i18n_salute_rule.h"
#include "vos_audit.h"
#include "vos_log.h"
#include "vos_lock.h"
#include <atomic>
#include <stdio.h>
#include <string.h>

namespace {

std::atomic<uint8_t> g_guasto{0};          // LangGuasto; 0 = la lingua non e in errore
std::atomic<bool> g_sostituzione{false};   // un caricamento sta sostituendo il file in uso: nessun guasto da segnalare
std::atomic<bool> g_avvisoDa{false};       // da dare l'avviso (una volta per episodio)
std::atomic<bool> g_puliscoDa{false};      // da togliere l'allarme dall'audit
char g_code[LANG_CODE_BUF] = "";           // lingua in errore (protetta da mux)

VosMutex& mux() {
  static VosMutex m = [] { VosMutex v; v.init(); return v; }();
  return m;
}

const char* testoMotivo(int g) {
  switch (g) {
    case LG_NON_SI_APRE: return "il file non c'e piu";
    case LG_DIMENSIONE: return "la dimensione e cambiata";
    case LG_INTESTAZIONE: return "l'intestazione non e valida";
    case LG_POSIZIONE: return "una voce punta fuori dal file";
    case LG_LETTURE: return "errori di lettura ripetuti";
  }
  return "errore";
}

}  // namespace

bool langInErrore() { return g_guasto.load() != 0; }

bool langErrore(char* code, size_t cap, int& motivo) {
  motivo = g_guasto.load();
  if (!motivo) {
    if (cap) code[0] = 0;
    return false;
  }
  VosLock l(mux());
  snprintf(code, cap, "%s", g_code);
  return true;
}

bool langErroreJson(HeapBuf& j) {
  char code[LANG_CODE_BUF];
  int m = 0;
  if (!langErrore(code, sizeof code, m)) return j.add("{\"k\":0}");
  return j.addf("{\"k\":1,\"c\":\"%s\",\"m\":%d}", code, m);
}

void langErroreImposta(const char* code, int guasto) {
  if (!code || guasto == LG_NESSUNO || strcmp(code, "it") == 0) return;     // l'italiano e nel firmware: non puo essere in errore
  if (g_sostituzione.load()) return;
  VosLock l(mux());
  if (g_guasto.load() != 0) return;                                          // gia in errore: un solo avviso
  snprintf(g_code, sizeof g_code, "%s", code);
  g_guasto = (uint8_t)guasto;
  g_avvisoDa = true;
}

void langErroreAzzera() {
  VosLock l(mux());
  if (g_guasto.exchange(0) != 0) g_puliscoDa = true;
  g_code[0] = 0;
}

void langSostituzione(bool inCorso) { g_sostituzione = inCorso; }

void langTick() {
  bool errore = g_guasto.load() != 0;
  if (g_avvisoDa.exchange(false) && errore) {
    char code[LANG_CODE_BUF];
    int m = 0;
    langErrore(code, sizeof code, m);
    vlog("LANG: file della lingua %s in errore (%s): uso l'italiano", code, testoMotivo(m));
    auditEvent(AUD_YELLOW, "langerr", trf("Il file della lingua %s non e valido: ricaricalo oppure usa l'italiano", code).c_str());
  }
  if (g_puliscoDa.exchange(false) && g_guasto.load() == 0) auditClear("langerr");
}
