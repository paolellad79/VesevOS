// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_run.cpp (vedi vos_elf_run.h)
// Il task del modulo, finito il main, si sospende e basta: lo elimina il supervisore (cosi nessun task si cancella da solo mentre
// qualcuno ne tiene il riferimento). Il blocco g_mx protegge tabella e ultimi; non si tiene mai durante l'esecuzione del modulo.
#include "vos_elf_run.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_elf.h"
#include "../core/vos_lock.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"

#define RUN_STACK      8192
#define RUN_PRIO       1
#define SUP_STACK      3072
#define SUP_PRIO       2
#define SUP_GIRO_MS    200

namespace {

VosMutex& mx() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

bool          g_supAttivo = false;                   // il supervisore esiste
vos_elf_ps_t  g_ultimi[VOS_ELF_ULTIMI];
int           g_nUltimi = 0;                         // piu recente in cima (indice 0)

void taskModulo(void* arg) {
  vos_elf_proc_t* p = (vos_elf_proc_t*)arg;
  snprintf(p->mod.nome, sizeof p->mod.nome, "%s", p->nome);      // l'app si riconosce dal nome (cartella dei dati)
  vos_elf_esegui(&p->mod, p->argc, p->argv, &p->ritorno);
  p->finito = true;
  vTaskSuspend(nullptr);                             // lo elimina il supervisore
}

void ricordaFinito(const vos_elf_proc_t* p, int uscita) {
  for (int i = VOS_ELF_ULTIMI - 1; i > 0; i--) g_ultimi[i] = g_ultimi[i - 1];
  vos_elf_ps_t& u = g_ultimi[0];
  memset(&u, 0, sizeof u);
  u.id = p->id;
  snprintf(u.nome, sizeof u.nome, "%s", p->nome);
  u.secondi = (drvMillis() - p->inizio_ms) / 1000;
  u.ram_stima = p->ram_stima;
  u.ritorno = p->ritorno;
  u.uscita = uscita;
  snprintf(u.use, sizeof u.use, "%s", p->use);
  if (g_nUltimi < VOS_ELF_ULTIMI) g_nUltimi++;
}

// Ferma il task, scarica il modulo, libera il posto. Con il blocco preso.
void raccogli(vos_elf_proc_t* p, int uscita) {
  bool forzato = !p->finito;
  if (p->task) vTaskDelete((TaskHandle_t)p->task);
  if (forzato) drvDelayMs(50);                       // l'altro core puo essere ancora dentro il modulo per un attimo: non si scarica subito
  vos_elf_scarica_forza(&p->mod);
  free(p->img);
  vos_elf_quar_fine(p->nome);                        // finito o fermato bene: azzera i punti di guasto
  ricordaFinito(p, uscita);
  vos_elf_tab_rilascia(p);
}

void supervisore(void*) {
  for (;;) {
    drvDelayMs(SUP_GIRO_MS);
    VosLock L(mx());
    for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
      vos_elf_proc_t* p = vos_elf_tab_posto(i);
      if (p->stato != VOS_ELF_P_ATTIVO) continue;
      if (p->finito) raccogli(p, VOS_ELF_USCITA_NORMALE);
      else if (p->da_fermare) raccogli(p, VOS_ELF_USCITA_FERMATO);
      else if (p->tempo_max_ms && (drvMillis() - p->inizio_ms) > p->tempo_max_ms) raccogli(p, VOS_ELF_USCITA_SCADUTO);
    }
    if (vos_elf_tab_attivi() == 0) { g_supAttivo = false; break; }
  }
  vTaskDelete(nullptr);
}

// Con il blocco preso. false = non si crea (poca memoria).
bool assicuraSupervisore() {
  if (g_supAttivo) return true;
  g_supAttivo = drvTaskCreate(supervisore, "elfsup", SUP_STACK, nullptr, SUP_PRIO, nullptr, DRV_CORE_ANY);
  return g_supAttivo;
}

bool copiaArgomenti(vos_elf_proc_t* p, const vos_elf_run_par_t& par) {
  size_t usati = 0;
  if (par.argc < 1 || par.argc > VOS_ELF_ARGV_MAX + 1) return false;
  for (int i = 0; i < par.argc; i++) {
    size_t n = strlen(par.argv[i]) + 1;
    if (usati + n > sizeof p->argBuf) return false;
    memcpy(p->argBuf + usati, par.argv[i], n);
    p->argv[i] = p->argBuf + usati;
    usati += n;
  }
  p->argc = par.argc;
  p->argv[p->argc] = nullptr;
  return true;
}

}  // namespace

int vos_elf_run(const char* nome, uint8_t* img, size_t len, const vos_elf_run_par_t& par, int* id) {
  if (!vos_elf_abilitato()) return VOS_ELF_E_SPENTO;
  if (!vos_elf_nome_valido(nome) || !img || !par.argv) return VOS_ELF_E_ARG;
  if (vos_elf_quar_dentro(nome)) return VOS_ELF_E_QUARANTENA;
  VosLock L(mx());
  int err = 0;
  vos_elf_proc_t* p = vos_elf_tab_prendi(nome, &err);
  if (!p) return err;
  if (!copiaArgomenti(p, par)) { vos_elf_tab_rilascia(p); return VOS_ELF_E_ARG; }
  vos_elf_param_t vp;
  memset(&vp, 0, sizeof vp);
  vp.ruolo = par.ruolo;
  vp.use = par.use;
  uint32_t tetto = vos_elf_caps()->ram_max_modulo;
  vp.ram_max = tetto && tetto < VOS_ELF_RUN_RAM_MAX ? tetto : VOS_ELF_RUN_RAM_MAX;
  vos_elf_info_t info;
  int e = vos_elf_valida(img, len, &vp, &info);
  if (e == VOS_ELF_OK) e = vos_elf_carica(img, len, &vp, &p->mod);
  if (e != VOS_ELF_OK) { vos_elf_scarica_forza(&p->mod); vos_elf_tab_rilascia(p); return e; }
  p->ruolo = par.ruolo;
  snprintf(p->use, sizeof p->use, "%s", par.use ? par.use : "");
  p->mod.uscita = par.uscita;
  p->mod.uscita_cap = par.uscita_cap;
  if (par.uscita && par.uscita_cap) par.uscita[0] = 0;
  p->ram_stima = info.ram_stima;
  p->tempo_max_ms = par.tempo_max_ms;
  p->inizio_ms = drvMillis();
  p->img = img;
  vos_elf_quar_avvio(nome);                          // scritto PRIMA di eseguire: se la scheda cade, al riavvio si sa chi girava
  TaskHandle_t h = nullptr;
  char tn[16];
  snprintf(tn, sizeof tn, "elf%d", p->id);
  bool ok = drvTaskCreate(taskModulo, tn, RUN_STACK, p, RUN_PRIO, &h, DRV_CORE_ANY);
  if (ok) { p->task = h; ok = assicuraSupervisore(); }
  if (!ok) {
    if (h) vTaskDelete(h);
    vos_elf_scarica_forza(&p->mod);
    vos_elf_quar_fine(nome);
    vos_elf_tab_rilascia(p);
    return VOS_ELF_E_MEM;
  }
  if (id) *id = p->id;
  return VOS_ELF_OK;                                 // il file e ora del processo
}

int vos_elf_ps(vos_elf_ps_t* out, int max) {
  VosLock L(mx());
  int n = 0;
  for (int i = 0; i < VOS_ELF_RUN_MAX && n < max; i++) {
    const vos_elf_proc_t* p = vos_elf_tab_posto(i);
    if (p->stato != VOS_ELF_P_ATTIVO) continue;
    vos_elf_ps_t& o = out[n++];
    memset(&o, 0, sizeof o);
    o.id = p->id;
    snprintf(o.nome, sizeof o.nome, "%s", p->nome);
    o.attivo = true;
    o.secondi = (drvMillis() - p->inizio_ms) / 1000;
    o.ram_stima = p->ram_stima;
    snprintf(o.use, sizeof o.use, "%s", p->use);
  }
  for (int i = 0; i < g_nUltimi && n < max; i++) out[n++] = g_ultimi[i];
  return n;
}

int vos_elf_ferma(const char* nome_o_id) {
  {
    VosLock L(mx());
    vos_elf_proc_t* p = vos_elf_tab_trova(nome_o_id);
    if (!p) return VOS_ELF_E_NOME;
    p->da_fermare = true;
  }
  for (int i = 0; i < 20; i++) {                     // fino a 2 secondi: il supervisore passa ogni 200 ms
    drvDelayMs(100);
    VosLock L(mx());
    if (!vos_elf_tab_trova(nome_o_id)) return VOS_ELF_OK;
  }
  return VOS_ELF_E_OCCUPATO;
}

void vos_elf_ferma_tutti() {
  {
    VosLock L(mx());
    for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
      vos_elf_proc_t* p = vos_elf_tab_posto(i);
      if (p->stato == VOS_ELF_P_ATTIVO) p->da_fermare = true;
    }
  }
  for (int i = 0; i < 20; i++) {
    drvDelayMs(100);
    VosLock L(mx());
    if (vos_elf_tab_attivi() == 0) return;
  }
}

// Il gruppo e una delle parole di use= (separate da spazi)?
static bool usaGruppo(const char* use, const char* gruppo) {
  size_t gl = strlen(gruppo);
  if (!gl) return false;
  const char* q = use;
  while (*q) {
    while (*q == ' ' || *q == ',') q++;
    const char* s = q;
    while (*q && *q != ' ' && *q != ',') q++;
    if ((size_t)(q - s) == gl && !strncmp(s, gruppo, gl)) return true;
  }
  return false;
}

int vos_elf_ferma_uso(const char* gruppo) {
  VosLock L(mx());
  int n = 0;
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
    vos_elf_proc_t* p = vos_elf_tab_posto(i);
    if (p->stato == VOS_ELF_P_ATTIVO && usaGruppo(p->use, gruppo)) { p->da_fermare = true; n++; }
  }
  return n;
}

int vos_elf_conta_uso(const char* gruppo) {
  VosLock L(mx());
  int n = 0;
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
    const vos_elf_proc_t* p = vos_elf_tab_posto(i);
    if (p->stato == VOS_ELF_P_ATTIVO && !p->finito && usaGruppo(p->use, gruppo)) n++;
  }
  return n;
}

void vos_elf_ferma_aspetta() {
  for (int i = 0; i < 20; i++) {
    drvDelayMs(100);
    VosLock L(mx());
    bool daFermare = false;
    for (int k = 0; k < VOS_ELF_RUN_MAX; k++) {
      const vos_elf_proc_t* p = vos_elf_tab_posto(k);
      if (p->stato == VOS_ELF_P_ATTIVO && p->da_fermare) daFermare = true;
    }
    if (!daFermare) return;
  }
}
#endif
