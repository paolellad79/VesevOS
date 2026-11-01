// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_where.cpp (vedi vos_prog_where.h)
#include "vos_prog_where.h"
#include <string.h>
#include "vos_app.h"
#include "vos_prog.h"
#include "vos_prog_data.h"
#include "../core/vos_audit.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_task.h"
#include "../devices/vos_sd.h"
#if VOS_WITH_ELF
#include "vos_elf.h"
#include "vos_elf_file.h"
#include "vos_elf_quar.h"
#include "vos_elf_tab.h"
#endif

namespace {

bool kindOk(uint8_t kind) { return kind == PK_APP_B || kind == PK_APP_E; }

bool inCorso(uint8_t kind, const char* name) {
  ProgRun r[PROG_RUN_MAX];
  int n = progRunning(r, PROG_RUN_MAX, false);
  for (int i = 0; i < n; i++) if (r[i].kind == kind && !strcmp(r[i].name, name) && r[i].active) return true;
  return false;
}

// Il controllo "microSD sparita": ferma le app che la usavano. Lavora in un task a parte (il sondaggio della scheda ha poco stack).
volatile bool g_fermando = false;

void taskSdPersa(void*) {
  ProgRun* r = new ProgRun[PROG_RUN_MAX];                     // sullo heap: lo stack del task e piccolo
  int n = progRunning(r, PROG_RUN_MAX, false);
  for (int i = 0; i < n; i++) {
    if (!r[i].active || !kindOk(r[i].kind) || progWhere(r[i].kind, r[i].name) != PW_SD) continue;
    ErrBuf e;
    progStop(r[i].name, e);
    vlog("APP: %s fermata: la microSD dei suoi dati non c'e piu", r[i].name);
    auditEvent(AUD_YELLOW, "appsd", trf("App %s fermata: la microSD dei suoi dati non c'e piu", r[i].name).c_str());
    EventBus::publish("app.sd", r[i].name, 0);
  }
  delete[] r;
  g_fermando = false;
  vTaskDelete(nullptr);
}

#if VOS_WITH_SD
void sdPersa() {
  if (g_fermando) return;
  g_fermando = true;
  if (!drvTaskCreate(taskSdPersa, "appsd", 8192, nullptr, 1, nullptr, DRV_CORE_ANY)) g_fermando = false;
}
#endif

}  // namespace

bool progSaves(uint8_t kind, const char* name) {
  if (!kindOk(kind) || !name || !name[0]) return false;
  if (kind == PK_APP_B) return appSavesData(name) == 1;
#if VOS_WITH_ELF
  char path[64], nome[VOS_ELF_NOME_MAX + 1], use[VOS_ELF_USE_MAX];
  if (vos_elf_risolvi(name, path, sizeof path, nome) != 0) return false;
  bool dati = false;
  vos_elf_pkg_gruppi(path, use, sizeof use, &dati);
  return dati;
#else
  return false;
#endif
}

bool progWhereSet(uint8_t kind, const char* name, int where, ErrBuf& err) {
  err = "";
  if (!kindOk(kind) || !name || !name[0]) { err = tr("Nome non valido"); return false; }
  if (!progSaves(kind, name)) { err = tr("L'app non salva dati"); return false; }
  if (inCorso(kind, name)) { err = tr("Ferma l'app prima di cambiare dove salva i dati"); return false; }
  return progWhereStore(kind, name, where, err);
}

void progDataStart() {
#if VOS_WITH_SD
  sdSetLostHook(sdPersa);
#endif
}
