// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diag.cpp
#include "vos_diag.h"
#include "vos_diag_rule.h"
#include "vos_crash.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "../drivers/vos_drv_boot.h"
#include <stdio.h>

static const uint32_t BOOT_GIRI = 3;              // dopo un riavvio anomalo: 3 giri a partire dall'avvio del task LED
static const uint32_t PERSISTE_MS = 60000;        // errore che dura: un giro ogni 60 s

static DiagCodice g_boot = DIAG_NESSUNO;          // causa dell'ultimo riavvio
static volatile uint8_t g_persC = 0, g_persD = 0; // errore che dura adesso
static volatile uint8_t g_provaC = 0, g_provaD = 0;
static volatile bool    g_provaOn = false;

void diagBoot() {
  uint8_t c = 0, d = 0;
  crashPrevDiag(&c, &d);
  g_boot = diagDaReset(drvResetReason(), DiagCodice{c, d});
  diagCodiceAvvio() = g_boot;
  if (diagNessuno(g_boot)) return;
  const DiagVoce* v = diagVoce(g_boot);
  if (v) vlog("ATTENZIONE: DIAG: D%u%02u (%s), il LED lo mostra dopo il riavvio", (unsigned)v->classe, (unsigned)v->dett, tr(v->nome));
  else vlog("ATTENZIONE: DIAG: causa del riavvio sconosciuta (reset %d)", drvResetReason());
}

void diagSegnala(uint8_t classe, uint8_t dett) {
  DiagCodice c = diagNormalizza(DiagCodice{classe, dett});
  g_persC = c.classe;
  g_persD = c.dett;
}

void diagProva(uint8_t classe, uint8_t dett) {
  DiagCodice c = diagNormalizza(DiagCodice{classe, dett});
  g_provaC = c.classe;
  g_provaD = c.dett;
  g_provaOn = true;
}

// Un giro che si ripete a partire dal primo istante visto: *t0 = inizio (0 = non ancora visto).
static uint32_t tempoDa(uint32_t ms, uint32_t* t0) {
  if (!*t0) *t0 = ms ? ms : 1;
  return ms - *t0;
}

bool diagLedComanda(uint32_t ms, bool ledSpento, bool* acceso, uint8_t* r, uint8_t* g, uint8_t* b) {
  static uint32_t tProva = 0, tBoot = 0, tPers = 0;
  static bool bootFinito = false;
  uint8_t cr, cg, cb;
  DiagCodice cod = DIAG_NESSUNO;
  uint32_t el = 0;
  bool attivo = false;
  // 1) prova richiesta dall'utente (un giro)
  if (g_provaOn) {
    cod = DiagCodice{g_provaC, g_provaD};
    el = tempoDa(ms, &tProva);
    if (el >= diagDurataGiro(cod, true)) { g_provaOn = false; tProva = 0; }
    else attivo = true;
  }
  // 2) causa dell'ultimo riavvio: 3 giri
  if (!attivo && !bootFinito && diagMostra(g_boot, ledSpento)) {
    cod = g_boot;
    el = tempoDa(ms, &tBoot);
    if (el >= BOOT_GIRI * diagDurataGiro(cod, true)) bootFinito = true;
    else attivo = true;
  }
  // 3) errore che dura: un giro ogni 60 s
  if (!attivo) {
    DiagCodice p = DiagCodice{g_persC, g_persD};
    if (diagMostra(p, ledSpento)) {
      el = tempoDa(ms, &tPers) % PERSISTE_MS;
      if (el < diagDurataGiro(p, true)) { cod = p; attivo = true; }
    }
  }
  if (!attivo) return false;
  *acceso = diagAccesoA(cod, el);
  diagColore(cod, &cr, &cg, &cb);
  if (r) *r = cr;
  if (g) *g = cg;
  if (b) *b = cb;
  return true;
}

std::string diagTesto() {
  std::string o;
  char riga[160];
  if (diagNessuno(g_boot)) o += std::string(tr("Ultimo riavvio: nessun errore da mostrare")) + "\n";
  else if (diagRiservato(g_boot)) o += std::string(tr("Ultimo riavvio: causa sconosciuta (3 lampi lunghi)")) + "\n";
  else {
    const DiagVoce* v = diagVoce(g_boot);
    snprintf(riga, sizeof riga, tr("Ultimo riavvio: D%u%02u (%s)"), (unsigned)g_boot.classe, (unsigned)g_boot.dett, v ? tr(v->nome) : "?");
    o += riga;
    o += '\n';
  }
  if (g_persC) {
    DiagCodice p = DiagCodice{g_persC, g_persD};
    const DiagVoce* v = diagVoce(p);
    snprintf(riga, sizeof riga, tr("Errore che dura: D%u%02u (%s)"), (unsigned)p.classe, (unsigned)p.dett, v ? tr(v->nome) : "?");
    o += riga;
    o += '\n';
  }
  o += tr("Il LED lampeggia la classe, una pausa, poi il dettaglio. Codici:");
  o += '\n';
  int n = 0;
  const DiagVoce* t = diagTabella(&n);
  for (int i = 0; i < n; i++) {
    snprintf(riga, sizeof riga, "  D%u%02u  %u+%u  %s", (unsigned)t[i].classe, (unsigned)t[i].dett, (unsigned)t[i].classe, (unsigned)t[i].dett, tr(t[i].nome));
    o += riga;
    o += '\n';
  }
  o += tr("Codice sconosciuto: 3 lampi lunghi");
  o += '\n';
  return o;
}
