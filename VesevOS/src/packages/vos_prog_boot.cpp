// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_boot.cpp (vedi vos_prog.h): i programmi con AVVIO AUTOMATICO. L'elenco sta in cfg.boot (l'ordine e l'ordine di avvio);
// le app con autostart=required partono sempre, davanti agli altri; quelle con autostart=never non si possono scegliere.
// Partono dopo i servizi, da un task che sparisce quando ha finito. Berry ha un solo interprete: se un programma Berry e gia in corso, il
// successivo aspetta fino a 10 s e poi salta (con una riga nel registro): non e un errore, e il limite dichiarato.
#include "vos_prog.h"
#include "vos_progext.h"
#include "vos_prog_where.h"
#include <stdio.h>
#include <string.h>
#include "vos_app.h"
#include "vos_berry.h"
#include "vos_func.h"
#include "../core/vos_config.h"
#include "../core/vos_ext.h"
#include "../core/vos_i18n.h"
#include "../core/vos_layout.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"
#if VOS_WITH_ELF
#include "vos_elf.h"
#endif

namespace {

const uint32_t kAttesaAvvioMs = 8000;      // dopo l'avvio dei servizi: rete e pagina hanno il tempo di partire
const uint32_t kAttesaBerryMs = 10000;     // un solo interprete: quanto si aspetta che il programma Berry precedente finisca

bool esiste(uint8_t kind, const char* name) {
  char path[96];
  if (kind == PK_APP_B) return appAutostart(name) >= 0;
  progPercorso(path, sizeof path, layoutPath(kind == PK_SCRIPT ? LAY_SCRIPTS : LAY_APPS), name, kind == PK_SCRIPT ? PEXT_SCRIPT : PEXT_APP_E);   // nuovo o vecchio nome
  return drvFsExists(path);
}

// 0 permesso, 1 obbligatorio, 2 vietato (solo le app Berry ne hanno una scritta nel file).
int regola(uint8_t kind, const char* name) {
  if (kind != PK_APP_B) return 0;
  int r = appAutostart(name);
  return r < 0 ? 0 : r;
}

bool nomeOk(const char* n) {
  size_t l = strlen(n);
  if (l < 1 || l > VOS_BOOTPROG_NAME) return false;
  for (size_t i = 0; i < l; i++) {
    char c = n[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

// L'elenco da avviare: prima le app obbligatorie (anche se non sono nella lista), poi la lista dell'utente.
int elencoDaAvviare(ProgBoot* out, int max) {
  int n = 0;
  char req[VOS_BOOTPROG_MAX][25];
  int nr = appRequiredList(req, VOS_BOOTPROG_MAX);
  for (int i = 0; i < nr && n < max; i++) {
    out[n].kind = PK_APP_B;
    snprintf(out[n].name, sizeof out[n].name, "%.24s", req[i]);
    out[n].rule = 1;
    out[n].forced = true;
    n++;
  }
  for (int i = 0; i < cfg.bootN && n < max; i++) {
    bool gia = false;
    for (int k = 0; k < n; k++) if (out[k].kind == cfg.boot[i].kind && !strcmp(out[k].name, cfg.boot[i].name)) gia = true;
    if (gia) continue;
    out[n].kind = cfg.boot[i].kind;
    snprintf(out[n].name, sizeof out[n].name, "%s", cfg.boot[i].name);
    out[n].rule = regola(out[n].kind, out[n].name);
    out[n].forced = out[n].rule == 1;
    n++;
  }
  return n;
}

void taskAvvio(void*) {
  drvDelayMs(kAttesaAvvioMs);
  ProgBoot b[VOS_BOOTPROG_MAX];
  int n = elencoDaAvviare(b, VOS_BOOTPROG_MAX);
  for (int i = 0; i < n; i++) {
    if (b[i].rule == 2 || !esiste(b[i].kind, b[i].name)) {
      vlog("AVVIO: %s%s non partito: %s", b[i].name, progExt(b[i].kind), b[i].rule == 2 ? "vietato dal suo file" : "file non trovato");
      continue;
    }
    if (b[i].kind != PK_APP_E) {                                   // Berry: un solo programma alla volta
      for (uint32_t t = 0; berryRunning() && t < kAttesaBerryMs; t += 200) drvDelayMs(200);
      if (berryRunning()) { vlog("AVVIO: %s non partito: un altro programma Berry e gia in corso", b[i].name); continue; }
    }
    ErrBuf e;
    if (progStart(b[i].kind, b[i].name, ROLE_ADMIN, "avvio automatico", e)) vlog("AVVIO: %s%s partito", b[i].name, progExt(b[i].kind));
    else vlog("AVVIO: %s%s non partito: %s", b[i].name, progExt(b[i].kind), e.c_str());
  }
  vTaskDelete(nullptr);
}

}  // namespace

void progBootStart() {
  funcHooksSet(progUsaFunzionalita, progFermaFunzionalita);       // le Funzionalita sanno chi le usa e chi fermare
  progDataStart();                                                // se la microSD sparisce, le app con i dati li' si fermano
  char req[1][25];
  if (cfg.bootN == 0 && appRequiredList(req, 1) == 0) return;      // di fabbrica nessun programma parte da solo: il task non nasce
  if (!drvTaskCreate(taskAvvio, "progboot", 4096, nullptr, 1, nullptr, DRV_CORE_ANY)) vlog("AVVIO: task dei programmi non partito (memoria)");
}

bool progBootJson(HeapBuf& out) {
  ProgBoot b[VOS_BOOTPROG_MAX];
  int n = elencoDaAvviare(b, VOS_BOOTPROG_MAX);
  out.addf("{\"max\":%d,\"list\":[", VOS_BOOTPROG_MAX);
  for (int i = 0; i < n; i++) {
    out.addf("%s{\"k\":\"%c\",\"n\":\"", i ? "," : "", b[i].kind == PK_SCRIPT ? 's' : b[i].kind == PK_APP_B ? 'b' : 'e');
    out.addJson(b[i].name);
    out.addf("\",\"r\":%d,\"f\":%s,\"ok\":%s}", b[i].rule, b[i].forced ? "true" : "false", esiste(b[i].kind, b[i].name) ? "true" : "false");
  }
  out.add("]}");
  return !(out.oom() || out.over());
}

// csv: "s:nome,b:nome,e:nome" nell'ordine di avvio. Tutto o niente: se una voce non va, non si cambia nulla.
bool progBootSet(const char* csv, ErrBuf& err) {
  err = "";
  BootProg nuova[VOS_BOOTPROG_MAX];
  int n = 0;
  const char* p = csv ? csv : "";
  while (*p) {
    const char* e = strchr(p, ',');
    size_t l = e ? (size_t)(e - p) : strlen(p);
    if (l > 0) {
      if (l < 3 || p[1] != ':' || (p[0] != 's' && p[0] != 'b' && p[0] != 'e') || l - 2 > VOS_BOOTPROG_NAME) { err = tr("Elenco di avvio non valido"); return false; }
      if (n >= VOS_BOOTPROG_MAX) { err.setf(tr("Massimo %d programmi con avvio automatico"), VOS_BOOTPROG_MAX); return false; }
      char nome[VOS_BOOTPROG_NAME + 1];
      memcpy(nome, p + 2, l - 2); nome[l - 2] = 0;
      uint8_t kind = p[0] == 's' ? PK_SCRIPT : p[0] == 'b' ? PK_APP_B : PK_APP_E;
      if (!nomeOk(nome)) { err = tr("Nome non valido"); return false; }
      if (!esiste(kind, nome)) { err.setf(tr("%s non esiste"), nome); return false; }
      int rg = regola(kind, nome);
      if (rg == 2) { err.setf(tr("%s non puo partire da solo (lo dice il suo file)"), nome); return false; }
      for (int i = 0; i < n; i++) if (nuova[i].kind == kind && !strcmp(nuova[i].name, nome)) { err.setf(tr("%s e gia nell'elenco"), nome); return false; }
      if (rg != 1) {                                               // un'app obbligatoria parte comunque: non si salva nell'elenco
        nuova[n].kind = kind;
        snprintf(nuova[n].name, sizeof nuova[n].name, "%s", nome);
        n++;
      }
    }
    if (!e) break;
    p = e + 1;
  }
  for (int i = 0; i < n; i++) cfg.boot[i] = nuova[i];
  cfg.bootN = (uint8_t)n;
  cfgSetOrigin("avvio programmi");
  cfgSave();
  return true;
}
