// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_where.cpp (vedi vos_prog_where.h)
#include "vos_prog_where.h"
#include <new>
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

// L'elenco dei programmi in corso sta sullo HEAP: 16 voci pesano oltre 2 KB e questo codice gira anche in task con poco stack (il panico del task
// "appsd" del 31/10/2026 veniva da li: 4 KB di stack, 2 KB di elenco e piu di 1 KB nei frame di progRunning).
class RunList {
 public:
  RunList() : r_(new (std::nothrow) ProgRun[PROG_RUN_MAX]), n_(r_ ? progRunning(r_, PROG_RUN_MAX, false) : 0) {}
  ~RunList() { delete[] r_; }
  RunList(const RunList&) = delete;
  RunList& operator=(const RunList&) = delete;
  bool ok() const { return r_ != nullptr; }
  int size() const { return n_; }
  const ProgRun& operator[](int i) const { return r_[i]; }
 private:
  ProgRun* r_;
  int n_;
};

bool inCorso(uint8_t kind, const char* name) {
  RunList run;
  for (int i = 0; i < run.size(); i++) if (run[i].kind == kind && !strcmp(run[i].name, name) && run[i].active) return true;
  return false;
}

// Il controllo "microSD sparita": ferma le app che la usavano. Lavora in un task a parte (il sondaggio della scheda ha poco stack).
volatile bool g_fermando = false;

constexpr uint32_t kStackBytes = 8192;             // misurato sul computer: elenco su heap, frame di progRunning circa 1,2 KB, resto di vlog/audit; il margine e controllato a ogni passaggio

void taskSdPersa(void*) {
  {
    RunList run;
    if (!run.ok()) vlog("APP: la microSD non c'e piu ma non riesco a controllare le app (memoria)");
    for (int i = 0; i < run.size(); i++) {
      const ProgRun& p = run[i];
      if (!p.active || !kindOk(p.kind) || progWhere(p.kind, p.name) != PW_SD) continue;
      ErrBuf e;
      progStop(p.name, e);
      vlog("APP: %s fermata: la microSD dei suoi dati non c'e piu", p.name);
      auditEvent(AUD_YELLOW, "appsd", trf("App %s fermata: la microSD dei suoi dati non c'e piu", p.name).c_str());
      EventBus::publish("app.sd", p.name, 0);
    }
  }                                                          // l'elenco si libera qui, prima di chiudere il task
  vlog("APP: controllo microSD finito, stack libero al minimo %u byte su %u", (unsigned)uxTaskGetStackHighWaterMark(nullptr), (unsigned)kStackBytes);
  g_fermando = false;
  vTaskDelete(nullptr);
}

#if VOS_WITH_SD
void sdPersa() {
  if (g_fermando) return;
  if (progSdAppCount() == 0) return;                         // nessuna app ha scelto la microSD: niente da fermare, e niente task
  g_fermando = true;
  if (!drvTaskCreate(taskSdPersa, "appsd", kStackBytes, nullptr, 1, nullptr, DRV_CORE_ANY)) {
    g_fermando = false;
    vlog("APP: la microSD non c'e piu ma non riesco a controllare le app (memoria)");
  }
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
