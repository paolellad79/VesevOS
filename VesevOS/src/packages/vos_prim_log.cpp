// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim_log.cpp
// STRATO 6 (packages): gruppi "base" (millis, sleep, print: sempre concessi) e "log" (log, log_int) del registro delle primitive.
// Una sola scrittura per tutti i linguaggi. ELF: simboli vos_millis, vos_sleep, vos_print, vos_log, vos_log_int.
// Un modulo gira con tutti i diritti: i testi che passa si leggono solo con un tetto di lunghezza.
#include "vos_prim.h"

#if VOS_WITH_ELF
#include "../core/vos_common.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_time.h"

#define SLEEP_MAX_MS 60000

static int primLog(const char* testo) {
  if (!testo) return -1;
  vlog("ELF: %.120s", testo);
  return 0;
}

static int primLogInt(const char* testo, int valore) {
  if (!testo) return -1;
  vlog("ELF: %.100s = %d", testo, valore);
  return 0;
}

// Uscita testuale del modulo: nel buffer del chiamante se c'e (comandi e widget dei driver ELF), altrimenti nel registro.
static int primPrint(const char* testo) {
  if (!testo) return -1;
  char t[160];
  if (!vos_prim_copia_testo(testo, t, sizeof t)) return -1;
  if (vos_prim_esec_scrivi(t) == 0 && t[0]) vlog("ELF: %.120s", t);
  else vos_prim_esec_scrivi("\n");                            // una riga per chiamata
  return 0;
}

static uint32_t primMillis() { return (uint32_t)drvMillis(); }
static void primSleep(int ms) { if (ms > 0) drvDelayMs(ms > SLEEP_MAX_MS ? SLEEP_MAX_MS : ms); }

static const vos_prim_t kBase[] = {
  { "millis", (const void*)primMillis, "uint32_t vos_millis(void)", ROLE_GUEST, 1 },
  { "sleep",  (const void*)primSleep,  "void vos_sleep(int ms)",     ROLE_GUEST, 1 },
  { "print",  (const void*)primPrint,  "int vos_print(const char *testo)", ROLE_GUEST, 3 },
};

static const vos_prim_t kLog[] = {
  { "log",     (const void*)primLog,    "int vos_log(const char *testo)",                ROLE_GUEST, 1 },
  { "log_int", (const void*)primLogInt, "int vos_log_int(const char *testo, int valore)", ROLE_GUEST, 1 },
};

void vos_prim_log_registra() {
  vos_prim_registra(VOS_PRIM_GRUPPO_BASE, kBase, sizeof(kBase) / sizeof(kBase[0]));
  vos_prim_registra("log", kLog, sizeof(kLog) / sizeof(kLog[0]));
}
#endif
