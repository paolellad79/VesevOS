// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_tocco.cpp
// Vedi vos_risorse_tocco.h. Il gancio gira dentro la scrittura di un file: resta breve (un confronto di prefisso e, solo per i file di
// /usr/share/vesevos, un avviso). Stack: circa 200 byte (un buffer di testo di 120 byte e il nome del file di 40).
#include "vos_risorse_tocco.h"
#include <stdio.h>
#include <string.h>
#include "vos_risorse_tocco_rule.h"
#include "vos_audit.h"
#include "vos_i18n.h"
#include "vos_lock.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_task.h"

namespace {

const int  ATTORI = 4;
const int  CHI_MAX = 22;

struct Attore {
  void* task;
  char  chi[CHI_MAX];
};

Attore g_att[ATTORI];
int    g_prossimo = 0;
void*  g_nostroTask[2] = {nullptr, nullptr};     // task dentro RisToccoNostro (al massimo due: web HTTP e HTTPS)

VosMutex& mux() {
  static VosMutex m = [] { VosMutex v; v.init(); return v; }();
  return m;
}

void chiE(char* out, size_t n) {
  void* me = drvTaskSelf();
  VosLock l(mux());
  for (int i = 0; i < ATTORI; i++) {
    if (g_att[i].task == me && g_att[i].chi[0]) { snprintf(out, n, "%s", g_att[i].chi); return; }
  }
  snprintf(out, n, "%s", "sistema");
}

bool nostro() {
  void* me = drvTaskSelf();
  VosLock l(mux());
  return g_nostroTask[0] == me || g_nostroTask[1] == me;
}

// Il gancio. Prima il controllo veloce (quasi tutte le scritture sono fuori da questa cartella).
void gancio(char azione, const char* p, const char* p2) {
  if (!risToccoDaSegnalare(azione, p, p2)) return;
  if (nostro()) return;
  char chi[CHI_MAX];
  chiE(chi, sizeof chi);
  char nome[40];
  bool destinazioneVera = azione == 'n' && p2 && risToccoDentro(p2) && !risToccoTemporaneo(p2);     // il file che prende il posto, se e qui dentro
  risToccoNome(destinazioneVera ? p2 : p, nome, sizeof nome);
  const char* fatto = azione == 'r' ? tr("tolto") : azione == 'n' ? tr("rinominato o sostituito") : azione == 'm' ? tr("cartella creata")
                    : azione == 'd' ? tr("cartella tolta") : tr("scritto");
  auditEvent(AUD_YELLOW, "risman", trf("file di risorse %s: %s da %s. Se non sei stato tu, scaricalo di nuovo: Sicurezza > Aggiornamenti", nome, fatto, chi).c_str());
}

}  // namespace

void risToccoInit() { drvFsGancioScrittura(gancio); }

void risAttore(const char* chi) {
  void* me = drvTaskSelf();
  VosLock l(mux());
  int i = 0;
  for (; i < ATTORI; i++) if (g_att[i].task == me) break;
  if (i == ATTORI) {
    i = g_prossimo;
    g_prossimo = (g_prossimo + 1) % ATTORI;
    g_att[i].task = me;
  }
  snprintf(g_att[i].chi, CHI_MAX, "%s", chi ? chi : "");
}

RisToccoNostro::RisToccoNostro() {
  void* me = drvTaskSelf();
  VosLock l(mux());
  for (int i = 0; i < 2; i++) if (!g_nostroTask[i]) { g_nostroTask[i] = me; return; }
}

RisToccoNostro::~RisToccoNostro() {
  void* me = drvTaskSelf();
  VosLock l(mux());
  for (int i = 0; i < 2; i++) if (g_nostroTask[i] == me) { g_nostroTask[i] = nullptr; return; }
}

void risToccoCaricato(const char* nome) {
  char chi[CHI_MAX];
  chiE(chi, sizeof chi);
  char n[40];
  risToccoNome(nome, n, sizeof n);
  auditEvent(AUD_INFO, "", trf("file di risorse %s sostituito da %s (SHA-256 controllato)", n, chi).c_str());
}
