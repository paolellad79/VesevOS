// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_svc.cpp
// STRATO 6 (packages): il servizio "elf" nel registro dei servizi (vos_service), mostrato come SOTTOSISTEMA vde. E l'INTERRUTTORE A RUNTIME dei
// moduli ELF: ACCESO di fabbrica (dal 31/10/2026), lo cambia solo l'Admin (dai Task) e si salva in /vesevos.conf (sezione elf). Acceso non parte
// nessun modulo da solo (salvo quelli con avvio automatico, vos_prog.h): apre soltanto la possibilita di caricarne. Il codice nativo dei moduli gira con tutti i diritti e senza protezione di memoria (nota tecnica nella guida, nessun avviso a regime).
// Registra anche le primitive (gruppi base, log, sd, dev, gpio, i2c, spi, time) e avvia il livello A la prima volta che serve.
#include "vos_elf.h"

#if VOS_WITH_ELF
#include "vos_prim.h"
#include "vos_func.h"
#include "vos_elf_quar.h"
#include "vos_elf_run.h"
#include "../drivers/vos_drv_boot.h"
#include "../drivers/vos_drv_task.h"
#include "../core/vos_config.h"
#include "../core/vos_service.h"
#include "../core/vos_audit.h"
#include "../core/vos_i18n.h"

static bool g_pronto;                    // primitive registrate e livello A avviato (una volta sola)

static void* chiTask() { return drvTaskSelf(); }   // le primitive distinguono i moduli dal task che li esegue

static bool prepara(ErrBuf& err) {
  if (g_pronto) return true;
  vos_prim_task_set(chiTask);
  vos_prim_funz_set(funcOn);                // le funzionalita tolte dall'utente valgono anche per i moduli
  vos_prim_gruppi_registra();
  if (vos_elf_init() != VOS_ELF_OK) { err = tr("Sottosistema vde: avvio non riuscito"); return false; }
  g_pronto = true;
  return true;
}

// Una riga nel registro, senza avvisi ne livello giallo: l'Admin che accende o spegne sa cosa fa.
static void traccia(bool on) {
  auditClear("elf");
  auditEvent(AUD_INFO, "elf", on ? tr("Sottosistema vde acceso") : tr("Sottosistema vde spento"));
}

static void imposta(bool on) {
  cfg.elfOn = on;
  vos_elf_abilita(on);
  cfgSetOrigin("ELF");
  cfgSave();
  traccia(on);
}

static int svcState() { return cfg.elfOn ? SVC_ON : SVC_OFF; }

static bool svcBegin(ErrBuf& err) {
  if (!prepara(err)) return false;
  imposta(true);
  return true;
}

static void svcStop() {
  vos_elf_ferma_tutti();                  // spegnere il servizio ferma anche i moduli in corso
  imposta(false);
}

static bool svcJson(HeapBuf& out) {
  const vos_elf_caps_t* c = vos_elf_caps();
  return out.addf("{\"have\":true,\"on\":%s,\"psram\":%s,\"ramMax\":%u,\"fileMax\":%u,\"primitive\":%d,\"run\":%d,\"runMax\":%d}",
                  cfg.elfOn ? "true" : "false", c->esegue_da_psram ? "true" : "false",
                  (unsigned)c->ram_max_modulo, (unsigned)c->file_max, vos_prim_conta(), vos_elf_tab_attivi(), VOS_ELF_RUN_MAX);
}

static const ServiceOps ELF_OPS = { "elf", "Sottosistema vde", "Sistema", "task", ROLE_OPER, ROLE_ADMIN,
                                    svcState, svcBegin, svcStop, nullptr, svcJson, nullptr, nullptr };

// Dopo la lettura della configurazione: si iscrive nel registro e, se e acceso (di fabbrica lo e), avvia il livello A.
void elfServiceInit() {
  serviceRegister(&ELF_OPS);
  vos_elf_quar_nvs_usa();
  vos_elf_quar_al_boot(drvResetWasCrash());    // chi girava quando la scheda e caduta prende un punto di guasto (3 di fila = quarantena)
  if (!cfg.elfOn) return;
  ErrBuf e;
  if (prepara(e)) { vos_elf_abilita(true); traccia(true); }
  else cfg.elfOn = false;                // non si e potuto avviare: resta spento (il file di configurazione non cambia)
}
#endif
