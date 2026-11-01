// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog.cpp (vedi vos_prog.h): elenco, stop, avvio e JSON dei programmi in corso; sottosistemi vdb e vde.
#include "vos_prog.h"
#include "vos_prog_data.h"
#include <stdio.h>
#include <string.h>
#include "vos_berry.h"
#include "vos_app.h"
#include "vos_func.h"
#include "../core/vos_config.h"
#include "../core/vos_ext.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_service.h"
#include "../drivers/vos_drv_time.h"
#if VOS_WITH_ELF
#include "vos_elf.h"
#include "vos_elf_file.h"
#include "vos_elf_run.h"
#endif

const char* progExt(uint8_t kind) {
  return kind == PK_SCRIPT ? VOS_EXT_SCRIPT : kind == PK_APP_B ? VOS_EXT_APP_B : VOS_EXT_APP_E;
}

static char kindChar(uint8_t kind) { return kind == PK_SCRIPT ? 's' : kind == PK_APP_B ? 'b' : 'e'; }

int progRunning(ProgRun* out, int max, bool conFiniti) {
  int n = 0;
  BerryRunInfo b;
  if (n < max && berryRunInfo(b) && !b.cmd) {                       // i comandi dei driver e "berry <testo>" non sono programmi dell'utente
    ProgRun& r = out[n++];
    memset(&r, 0, sizeof r);
    r.kind = b.app ? PK_APP_B : PK_SCRIPT;
    snprintf(r.name, sizeof r.name, "%s", b.name);
    r.active = true;
    r.seconds = b.seconds;
    r.memory = b.memory;
    snprintf(r.use, sizeof r.use, "%s", b.use);
  }
#if VOS_WITH_ELF
  vos_elf_ps_t v[VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI];
  int ne = vos_elf_ps(v, VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI);
  for (int i = 0; i < ne && n < max; i++) {
    if (!v[i].attivo && !conFiniti) continue;
    ProgRun& r = out[n++];
    memset(&r, 0, sizeof r);
    r.kind = PK_APP_E;
    snprintf(r.name, sizeof r.name, "%s", v[i].nome);
    r.id = v[i].id;
    r.active = v[i].attivo;
    r.seconds = v[i].secondi;
    r.memory = v[i].ram_stima;
    r.result = v[i].ritorno;
    r.how = (uint8_t)v[i].uscita;
    snprintf(r.use, sizeof r.use, "%s", v[i].use);
  }
#else
  (void)conFiniti;
#endif
  return n;
}

bool progStop(const char* nomeOId, ErrBuf& err) {
  err = "";
  if (!nomeOId || !nomeOId[0]) { err = tr("Serve il nome del programma"); return false; }
  BerryRunInfo b;
  if (berryRunInfo(b) && !b.cmd && !strcmp(b.name, nomeOId)) {
    if (!berryStop(err)) return false;
    vlog("PROG: %s fermato", nomeOId);
    return true;
  }
#if VOS_WITH_ELF
  if (vos_elf_abilitato()) {
    int e = vos_elf_ferma(nomeOId);
    if (e == VOS_ELF_OK) { vlog("PROG: %s fermato", nomeOId); return true; }
    if (e != VOS_ELF_E_NOME) { err = vos_elf_err_testo(e); return false; }
  }
#endif
  err = tr("Nessun programma in esecuzione con questo nome");
  return false;
}

#if VOS_WITH_ELF
// Avvia un'app ELF installata con i gruppi di primitive che chiede (poi le Funzionalita e il ruolo possono solo toglierne).
static bool startElf(const char* name, int role, ErrBuf& err) {
  if (!vos_elf_abilitato()) { err = tr("Il sottosistema vde e spento: accendilo dai Task"); return false; }
  char path[64], nome[VOS_ELF_NOME_MAX + 1];
  int e = vos_elf_risolvi(name, path, sizeof path, nome);
  if (e == VOS_ELF_OK) {
    VosElfImg im;
    e = vos_elf_img_leggi(path, vos_elf_caps()->file_max, im);
    if (e == VOS_ELF_OK) {
      char use[VOS_ELF_USE_MAX];
      vos_elf_gruppi_richiesti(im.p, im.n, use, sizeof use);
      if (const char* off = funcMissing(use)) { err = trf("Il programma chiede la funzionalita %s, che e spenta (Sistema > Funzionalita)", off); return false; }
      if (!progDataReady(PK_APP_E, nome, err)) return false;       // dati sulla microSD scelta e la scheda non c'e: l'app non parte
      char arg0[VOS_ELF_NOME_MAX + 16];
      snprintf(arg0, sizeof arg0, "%s%s", nome, VOS_EXT_APP_E);
      char* argv[2] = { arg0, nullptr };
      vos_elf_run_par_t par = { role, use, 1, argv, 0, nullptr, 0 };
      int id = 0;
      e = vos_elf_run(nome, im.p, im.n, par, &id);
      if (e == VOS_ELF_OK) { im.cedi(); return true; }
    }
  }
  err = vos_elf_err_testo(e);
  return false;
}
#endif

bool progStart(uint8_t kind, const char* name, int role, const char* who, ErrBuf& err) {
  err = "";
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questa operazione"); return false; }
  switch (kind) {
    case PK_SCRIPT:
      if (role < ROLE_ADMIN) { err = tr("Serve il livello Admin"); return false; }
      if (!berryStart(name, err)) return false;
      vlog("PROG: script %s avviato da %s", name, who ? who : "?");
      return true;
    case PK_APP_B:
      return appStart(name, err, (uint8_t)role, who);
    case PK_APP_E:
#if VOS_WITH_ELF
      if (role < ROLE_ADMIN) { err = tr("Serve il livello Admin"); return false; }
      if (!startElf(name, role, err)) return false;
      vlog("PROG: app %s avviata da %s", name, who ? who : "?");
      return true;
#else
      err = tr("Moduli ELF non presenti in questo firmware");
      return false;
#endif
    default:
      err = tr("Tipo di programma sconosciuto");
      return false;
  }
}

bool progRestart(uint8_t kind, const char* name, int role, ErrBuf& err) {
  ErrBuf e;
  progStop(name, e);                                        // se non girava, riavviare vuol dire avviare
  for (int i = 0; i < 10; i++) {                           // Berry: aspetta che l'interprete sia libero
    if (kind == PK_APP_E || !berryRunning()) break;
    drvDelayMs(100);
  }
  return progStart(kind, name, role, "riavvio", err);
}

// ---------- Funzionalita: chi le usa e chi le ferma (collegato a vos_func.cpp da progBootStart) ----------
static bool inUso(const char* use, const char* gruppo) {
  size_t gl = strlen(gruppo);
  for (const char* q = use ? use : ""; *q;) {
    while (*q == ' ' || *q == ',') q++;
    const char* s = q;
    while (*q && *q != ' ' && *q != ',') q++;
    if ((size_t)(q - s) == gl && !strncmp(s, gruppo, gl)) return true;
  }
  return false;
}

int progUsaFunzionalita(const char* id) {
  int n = 0;
  BerryRunInfo b;
  if (berryRunInfo(b) && b.app && inUso(b.use, id)) n++;
#if VOS_WITH_ELF
  n += vos_elf_conta_uso(id);
#endif
  return n;
}

void progFermaFunzionalita(const char* id) {
  BerryRunInfo b;
  ErrBuf e;
  if (berryRunInfo(b) && b.app && inUso(b.use, id)) berryStop(e);
#if VOS_WITH_ELF
  if (vos_elf_ferma_uso(id)) vos_elf_ferma_aspetta();
#endif
}

// ---------- sottosistemi ----------
int progSubs(ProgSub* out, int max) {
  int n = 0;
  ProgRun r[PROG_RUN_MAX];
  int nr = progRunning(r, PROG_RUN_MAX, false);
#if VOS_WITH_BERRY
  if (n < max) {
    out[n] = { "vdb", cfg.berryOn, 0, 0 };
    for (int i = 0; i < nr; i++) if (r[i].kind != PK_APP_E) { out[n].running++; out[n].memory += r[i].memory; }
    n++;
  }
#endif
#if VOS_WITH_ELF
  if (n < max) {
    out[n] = { "vde", cfg.elfOn, 0, 0 };
    for (int i = 0; i < nr; i++) if (r[i].kind == PK_APP_E && r[i].active) { out[n].running++; out[n].memory += r[i].memory; }
    n++;
  }
#endif
  return n;
}

const char* progSubService(const char* nome) {
  if (!nome) return nullptr;
  if (!strcmp(nome, "vdb")) return serviceFind("vdb") ? "vdb" : nullptr;
  if (!strcmp(nome, "vde") || !strcmp(nome, "elf")) return serviceFind("elf") ? "elf" : nullptr;
  return nullptr;
}

// ---------- elenco per i Task, ps e la pagina ----------
void progPsText(HeapBuf& out) {
  ProgRun r[PROG_RUN_MAX];
  int n = progRunning(r, PROG_RUN_MAX, true);
  if (n == 0) return;
  out.add(tr("Programmi in corso")); out.add('\n');
  out.add(tr("Nome                      Tipo  Stato      Secondi  Memoria")); out.add('\n');
  for (int i = 0; i < n; i++) {
    const char* st = r[i].active ? tr("in corso") : r[i].how == 1 ? tr("fermato") : r[i].how == 2 ? tr("scaduto") : tr("finito");
    out.addf("%-25s %-5s %-10s %-8u %u\n", r[i].name, progExt(r[i].kind) + 1, st, (unsigned)r[i].seconds, (unsigned)r[i].memory);
  }
}

bool progJson(HeapBuf& out) {
  ProgSub s[2];
  int ns = progSubs(s, 2);
  out.add("\"subs\":[");
  for (int i = 0; i < ns; i++) {
    out.addf("%s{\"id\":\"%s\",\"on\":%s,\"n\":%d,\"m\":%u}", i ? "," : "", s[i].id, s[i].on ? "true" : "false", s[i].running, (unsigned)s[i].memory);
  }
  out.add("],\"progs\":[");
  ProgRun r[PROG_RUN_MAX];
  int n = progRunning(r, PROG_RUN_MAX, true);
  for (int i = 0; i < n; i++) {
    out.addf("%s{\"k\":\"%c\",\"n\":\"", i ? "," : "", kindChar(r[i].kind));
    out.addJson(r[i].name);
    out.addf("\",\"id\":%d,\"a\":%s,\"s\":%u,\"m\":%u,\"rc\":%d,\"h\":%d,\"u\":\"", r[i].id, r[i].active ? "true" : "false", (unsigned)r[i].seconds,
             (unsigned)r[i].memory, r[i].result, (int)r[i].how);
    out.addJson(r[i].use);
    out.add("\"}");
  }
  out.add(']');
  return !(out.oom() || out.over());
}
