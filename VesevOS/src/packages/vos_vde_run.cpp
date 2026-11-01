// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vde_run.cpp (vedi vos_vde.h): comandi dei driver ELF e liste per il registro dei comandi e dei widget.
// Un comando = un avvio del modulo ELF del driver (vos_elf_run, quindi con tetti, supervisore e quarantena) e l'attesa della fine, al massimo 3 secondi.
// Il driver scrive con vos_print() in un buffer nostro: il firmware non fa girare altro codice. Un driver alla volta sul suo nome.
#include "vos_vde.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_elf.h"
#include "vos_elf_run.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_time.h"

#define CMD_OUT_MAX   1500
#define CMD_MS        3000
#define WIDGET_MS     600
#define ARG_PAROLE    6

namespace {

// Il processo `id` gira ancora? Se e finito, *ritorno e *uscita dicono come.
bool ancoraAttivo(int id, int* ritorno, int* uscita) {
  vos_elf_ps_t ps[VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI];
  int n = vos_elf_ps(ps, VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI);
  for (int i = 0; i < n; i++) {
    if (ps[i].id != id) continue;
    if (ps[i].attivo) return true;
    *ritorno = ps[i].ritorno; *uscita = ps[i].uscita;
    return false;
  }
  return false;                                                // gia scordato: finito da un pezzo
}

// Avvia il modulo del driver e aspetta. L'ELF (elf) passa al processo se parte. *ritorno = valore di main. false = non e partito o non risponde (err).
// *tenuto = true solo se il modulo non si e fermato: il buffer ob potrebbe ancora servirgli e chi chiama NON deve liberarlo.
bool esegui(const VdeInfo& v, uint8_t* elf, const char* sub, int ruolo, int argc, const char* const* args, uint32_t tempoMs,
            char* ob, int* ritorno, bool* tenuto, ErrBuf& err) {
  *tenuto = false;
  char use[VOS_PRIM_USE_MAX];
  snprintf(use, sizeof use, "%s %s", v.use, v.name);          // il driver puo sempre usare se stesso
  const char* argv[VOS_ELF_ARGV_MAX + 2];
  int n = 0;
  argv[n++] = v.name; argv[n++] = sub;
  for (int i = 0; i < argc && n < VOS_ELF_ARGV_MAX + 1; i++) argv[n++] = args[i];
  argv[n] = nullptr;
  vos_elf_run_par_t par = { ruolo, use, n, (char**)argv, tempoMs, ob, CMD_OUT_MAX + 1 };
  ob[0] = 0;
  int id = 0;
  int r = vos_elf_run(v.name, elf, v.elfLen, par, &id);
  if (r != VOS_ELF_OK) { free(elf); err = vos_elf_err_testo(r); return false; }
  uint32_t t0 = (uint32_t)drvMillis();
  int uscita = VOS_ELF_USCITA_NORMALE;
  *ritorno = 0;
  while (ancoraAttivo(id, ritorno, &uscita)) {
    if ((uint32_t)drvMillis() - t0 > tempoMs + 1000) {
      if (vos_elf_ferma(v.name) != VOS_ELF_OK) { *tenuto = true; err = tr("Il driver non risponde e non si ferma"); return false; }
      err = tr("Il driver ha superato il tempo massimo");
      return false;
    }
    drvDelayMs(40);
  }
  if (uscita == VOS_ELF_USCITA_SCADUTO) { err = tr("Il driver ha superato il tempo massimo"); return false; }
  if (uscita == VOS_ELF_USCITA_FERMATO) { err = tr("Il driver e stato fermato"); return false; }
  return true;
}

}  // namespace

bool vdeComando(const char* nome, const char* sub, bool admin, int argc, const char* const* args, HeapBuf& out, ErrBuf& err) {
  if (argc < 0 || argc > ARG_PAROLE || !sub) { err = tr("Argomenti non validi"); return false; }
  VdeInfo* v = new VdeInfo();
  uint8_t* elf = nullptr;
  if (!vdeLeggi(nome, *v, &elf, err)) { delete v; return false; }
  char* ob = (char*)malloc(CMD_OUT_MAX + 1);
  if (!ob) { free(elf); delete v; err = tr("Memoria insufficiente"); return false; }
  vlog("CMD: %s %s (ELF, %s)", nome, sub, admin ? "admin" : "oper");   // mai gli argomenti: possono contenere percorsi o testi
  int ritorno = 0;
  bool tenuto = false;
  bool ok = esegui(*v, elf, sub, admin ? ROLE_ADMIN : ROLE_OPER, argc, args, CMD_MS, ob, &ritorno, &tenuto, err);
  if (ok) {
    out.add(ob);
    if (ritorno != 0) { err.setf(tr("Il driver ha finito con l'errore %d"), ritorno); ok = false; }
  }
  if (!tenuto) free(ob);                                      // finito o mai partito: il modulo non scrive piu
  delete v;
  return ok;
}

int vdeComandiLista(ModCmdEntry* out, int mx) {
  char (*nomi)[MOD_NAME_BUF] = new char[VDE_LIST_MAX][MOD_NAME_BUF];
  int tot = vdeElenco(nomi, VDE_LIST_MAX), n = tot < VDE_LIST_MAX ? tot : VDE_LIST_MAX, got = 0;
  VdeInfo* v = new VdeInfo();
  for (int i = 0; i < n && got < mx; i++) {
    ErrBuf e;
    if (!vdeLeggi(nomi[i], *v, nullptr, e)) continue;
    for (int k = 0; k < v->ncmd && got < mx; k++) {
      snprintf(out[got].driver, sizeof out[got].driver, "%s", v->name);
      out[got].c = v->cmd[k];
      got++;
    }
  }
  delete v;
  delete[] nomi;
  return got;
}

int vdeWidgetLista(ModWidgetEntry* out, int mx) {
  char (*nomi)[MOD_NAME_BUF] = new char[VDE_LIST_MAX][MOD_NAME_BUF];
  int tot = vdeElenco(nomi, VDE_LIST_MAX), n = tot < VDE_LIST_MAX ? tot : VDE_LIST_MAX, got = 0;
  VdeInfo* v = new VdeInfo();
  for (int i = 0; i < n && got < mx; i++) {
    ErrBuf e;
    if (!vdeLeggi(nomi[i], *v, nullptr, e) || !v->widget.on) continue;
    snprintf(out[got].driver, sizeof out[got].driver, "%s", v->name);
    out[got].w = v->widget;
    snprintf(out[got].icon, sizeof out[got].icon, "%s", v->icon);
    snprintf(out[got].source, sizeof out[got].source, "%s", v->source);
    got++;
  }
  delete v;
  delete[] nomi;
  return got;
}

// Valori del widget tenuti dal FIRMWARE (source=sd): la fotografia della microSD, nessuna lettura della scheda e nessun codice.
static void valoriSd(ModWidgetVals& vals) {
  memset(&vals, 0, sizeof vals);
  strcpy(vals.st, "off");
#if VOS_WITH_SD
  SdVolume s;
  const char* nomi[] = { tr("Non configurata"), tr("Montata"), tr("Non montata"), tr("Non rilevata"), tr("Formato non supportato: serve FAT32") };
  if (!sdSnapshot(s)) { snprintf(vals.v, sizeof vals.v, "%s", nomi[0]); return; }
  snprintf(vals.v, sizeof vals.v, "%s", nomi[s.state <= 4 ? s.state : 0]);
  if (s.state == SD_ST_MOUNTED) {
    strcpy(vals.st, "ok");
    snprintf(vals.s, sizeof vals.s, "%s, %u MB %s %u MB, %u kHz", s.fs, (unsigned)((s.totalKb - s.usedKb) / 1024), tr("liberi su"), (unsigned)(s.totalKb / 1024), (unsigned)s.khz);
  } else {
    snprintf(vals.s, sizeof vals.s, "%u kHz", (unsigned)s.khz);
  }
#endif
}

bool vdeWidgetValori(const char* nome, const char* source, ModWidgetVals& vals) {
  if (source && !strcmp(source, "sd")) { valoriSd(vals); return true; }
  VdeInfo* v = new VdeInfo();
  uint8_t* elf = nullptr;
  ErrBuf e;
  bool ok = vdeLeggi(nome, *v, &elf, e);
  char* ob = ok ? (char*)malloc(CMD_OUT_MAX + 1) : nullptr;
  if (ok && !ob) { free(elf); ok = false; }
  int ritorno = 0;
  bool tenuto = false;
  if (ok) ok = esegui(*v, elf, "widget", ROLE_OPER, 0, nullptr, WIDGET_MS, ob, &ritorno, &tenuto, e);
  if (ok) modWidgetParseOut(ob, vals);
  if (ob && !tenuto) free(ob);
  delete v;
  return ok;
}
#endif
