// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_prova.cpp (vedi vos_elf_prova.h)
// Il modulo gira nel SUO task (stack proprio, priorita bassa, core automatico); chi chiama aspetta a scatti di un tick, cosi il watchdog
// del suo task resta alimentato. Il task, finito il main, non si cancella da solo: si ferma e lo elimina sempre chi ha lanciato la prova
// (cosi non esiste mai un task che si cancella mentre qualcuno ne usa il riferimento).
#include "vos_elf_prova.h"

#if VOS_WITH_ELF
#include <string.h>
#include "vos_elf.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"

#define PROVA_STACK 8192
#define PROVA_PRIO  1

namespace {

volatile bool g_occupato;            // una prova alla volta

struct Lavoro {
  vos_elf_mod_t* mod;
  int            argc;
  char**         argv;
  int            ritorno;
  volatile bool  finito;
};

void taskModulo(void* arg) {
  Lavoro* w = (Lavoro*)arg;
  vos_elf_esegui(w->mod, w->argc, w->argv, &w->ritorno);
  w->finito = true;
  vTaskSuspend(nullptr);             // lo elimina chi ha lanciato la prova
}

// Lancia il main del modulo e aspetta la fine o il tetto di tempo. Ritorna false se il tempo e scaduto (task eliminato).
bool eseguiConTetto(vos_elf_mod_t* m, const vos_elf_prova_par_t& par, int& ritorno, bool& partito) {
  Lavoro w = { m, par.argc, par.argv, 0, false };
  TaskHandle_t h = nullptr;
  partito = drvTaskCreate(taskModulo, "elfprova", PROVA_STACK, &w, PROVA_PRIO, &h, DRV_CORE_ANY);
  if (!partito) return true;
  unsigned long t0 = drvMillis();
  while (!w.finito && (drvMillis() - t0) < par.tempo_max_ms) drvTaskPause();
  bool scaduto = !w.finito;
  vTaskDelete(h);
  if (scaduto) drvDelayMs(50);       // l'altro core puo essere ancora dentro il modulo per un attimo: non si scarica subito
  ritorno = w.ritorno;
  return !scaduto;
}

void misura(uint32_t& ram, uint32_t& psram) {
  ram = drvHeapFree();
  psram = drvPsramFree();
}

int fallita(vos_elf_prova_ris_t& ris, int fase, int err) {
  ris.fase = fase;
  ris.err = err;
  return err;
}

// Valida e carica. Ritorna 0 oppure l'errore (ris.fase dice dove).
int preparaModulo(const uint8_t* buf, size_t len, const vos_elf_prova_par_t& par, vos_elf_mod_t& mod, vos_elf_prova_ris_t& ris) {
  if (!vos_elf_abilitato()) return fallita(ris, VOS_ELF_FASE_VALIDA, VOS_ELF_E_SPENTO);
  vos_elf_param_t p;
  memset(&p, 0, sizeof(p));
  p.ruolo = par.ruolo;
  p.use = par.use;
  vos_elf_info_t info;
  int e = vos_elf_valida(buf, len, &p, &info);
  if (e) return fallita(ris, VOS_ELF_FASE_VALIDA, e);
  ris.ram_stima = info.ram_stima;
  unsigned long t0 = drvMillis();
  e = vos_elf_carica(buf, len, &p, &mod);
  ris.ms_carica = (uint32_t)(drvMillis() - t0);
  if (e) {
    const char* s = vos_elf_ultimo_simbolo();
    if (s) strncpy(ris.simbolo, s, sizeof(ris.simbolo) - 1);
    return fallita(ris, VOS_ELF_FASE_CARICA, e);
  }
  return 0;
}

}  // namespace

int vos_elf_prova(const uint8_t* buf, size_t len, const vos_elf_prova_par_t& par, vos_elf_prova_ris_t& ris) {
  memset(&ris, 0, sizeof(ris));
  if (!buf || !par.argv || par.argc < 1) return fallita(ris, VOS_ELF_FASE_VALIDA, VOS_ELF_E_ARG);
  if (g_occupato) return fallita(ris, VOS_ELF_FASE_VALIDA, VOS_ELF_E_OCCUPATO);
  g_occupato = true;
  vos_elf_mod_t mod;
  memset(&mod, 0, sizeof(mod));
  misura(ris.ram_prima, ris.psram_prima);
  int e = preparaModulo(buf, len, par, mod, ris);
  if (e) {                                     // una prova fallita non lascia nulla in memoria
    vos_elf_scarica_forza(&mod);
    misura(ris.ram_dopo, ris.psram_dopo);
    g_occupato = false;
    return e;
  }
  misura(ris.ram_caricato, ris.psram_caricato);
  ris.fase = VOS_ELF_FASE_ESEGUI;
  bool partito = false;
  unsigned long t0 = drvMillis();
  ris.scaduto = !eseguiConTetto(&mod, par, ris.ritorno, partito);
  ris.ms_esegui = (uint32_t)(drvMillis() - t0);
  vos_elf_scarica_forza(&mod);                 // dopo la fine o dopo l'eliminazione del task: il modulo non gira piu
  misura(ris.ram_dopo, ris.psram_dopo);
  g_occupato = false;
  if (!partito) return fallita(ris, VOS_ELF_FASE_ESEGUI, VOS_ELF_E_MEM);
  ris.fase = VOS_ELF_FASE_FINITA;
  return 0;
}
#endif
