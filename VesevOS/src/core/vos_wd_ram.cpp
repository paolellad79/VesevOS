// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd_ram.cpp (realizza vos_ramwd.h; il nome e diverso apposta: un file con lo stesso nome del suo header entrerebbe nel ciclo tra moduli del nucleo, vedi tools/checklayers.py)
#include "vos_ramwd.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_audit.h"
#include "vos_hooks.h"
#include "vos_service.h"
#include "vos_strbuf.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_time.h"

static RamWdCtx g_ctx;
static volatile int32_t  g_busyN = 0;
static volatile uint32_t g_busyUntil = 0;     // millisecondi (0 = nessuna scadenza)
static int g_used = -1;
static char g_off[48];

void ramBusyInc() { __atomic_add_fetch(&g_busyN, 1, __ATOMIC_RELAXED); }
void ramBusyDec() { if (__atomic_load_n(&g_busyN, __ATOMIC_RELAXED) > 0) __atomic_sub_fetch(&g_busyN, 1, __ATOMIC_RELAXED); }
void ramBusyTouch(uint32_t holdSec) {
  uint32_t until = drvMillis() + holdSec * 1000UL;
  g_busyUntil = until ? until : 1;
}
bool ramBusy() {
  if (__atomic_load_n(&g_busyN, __ATOMIC_RELAXED) > 0) return true;
  uint32_t u = g_busyUntil;
  return u && (int32_t)(u - drvMillis()) > 0;
}

RamLvl ramWdLevel() { return g_ctx.lvl; }
int ramWdUsed() { return g_used; }
const char* ramWdOff() { return g_off; }
bool ramWdPoca() { return g_ctx.lvl >= RAML_RIDUCI; }

static void offAdd(const char* what) {
  size_t n = strlen(g_off);
  StrBuf b(g_off + n, sizeof(g_off) - n);
  if (n) b.add(", ");
  b.add(what);
}

// Un servizio opzionale acceso viene spento (stesso ordine della modalita ridotta del watchdog: mesh, mqtt, bluetooth)
static void spegni(const char* id) {
  if (!hookDevIsOn(id) || !hookDevOff(id)) return;
  offAdd(id);
  vlog("RAM: servizio '%s' spento per risparmiare memoria", id);
}

// Passi di riduzione, in ordine; chi non c'e o e gia spento non fa nulla. Mai: Wi-Fi, watchdog, log, pagina, recovery.
static void passo(uint8_t n) {
  switch (n) {
    case 0: hookCall(g_hooks.webTrimIdle); break;               // chiude i collegamenti HTTPS fermi
    case 1: cfgRilasciaCopia(); break;                          // la copia in RAM del registro delle modifiche
    case 2: spegni("mesh"); break;
    case 3: spegni("mqtt"); break;
    default:
      if (serviceState("ble") == SVC_ON) { ErrBuf be; serviceStop("ble", be); offAdd("ble"); vlog("RAM: Bluetooth spento per risparmiare memoria"); }
      break;
  }
}

static void segnala(RamLvl l, int used) {
  if (l == RAML_OK) { auditClear("ramalta"); vlog("RAM: tornata sotto il %u%% (usata %d%%)", (unsigned)RAMWD_SEGNALA, used); return; }
  if (l == RAML_SEGNALA) auditEvent(AUD_YELLOW, "ramalta", trf("Memoria in esaurimento: RAM interna usata al %d%%", used).c_str());
  else if (l == RAML_RIDUCI) auditEvent(AUD_YELLOW, "ramalta", trf("Memoria scarsa: RAM interna usata al %d%%, spengo il superfluo", used).c_str());
  else auditEvent(AUD_YELLOW, "ramalta", trf("Memoria quasi finita: RAM interna usata al %d%%", used).c_str());
  vlog("RAM: livello %d (usata %d%%)", (int)l, used);
}

bool ramWdTick() {
  RamWdIn in;
  in.used = ramUsedPerc(drvHeapTotal(), drvHeapFree());
  in.crit = ramWdClampCrit(cfg.wdRamPerc);
  in.nowSec = drvMillis() / 1000UL;
  in.abilitato = cfg.wdRam;
  in.occupato = ramBusy();
  in.guida = !cfg.setupDone;
  in.minima = VOS_RAM_MINIMA != 0;
  g_used = in.used;
  RamLvl prima = g_ctx.lvl;
  RamWdOut o = ramWdStep(g_ctx, in);
  if (prima != g_ctx.lvl || o.cambiato) { segnala(g_ctx.lvl, in.used); cfgSetRamScarsa(g_ctx.lvl >= RAML_RIDUCI); }
  if (o.passo) passo(o.nPasso);
  return o.riavvia;
}
