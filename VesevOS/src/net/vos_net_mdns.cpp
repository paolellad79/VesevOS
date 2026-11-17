// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net_mdns.cpp
#include "vos_net_mdns.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_primo_avvio.h"
#include "../drivers/vos_drv_mdns.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_time.h"

static bool g_run = false;
static uint8_t g_tentativi = 0;          // nuovi tentativi gia programmati dopo un avvio fallito
static uint32_t g_riprovaAlle = 0;       // ora (ms) del prossimo tentativo, 0 = nessuno
static const uint8_t TENTATIVI_MAX = 3;  // mai un ciclo infinito: dopo 3 nuovi tentativi (5, 10, 20 secondi) si smette

void netMdnsStop() {
  drvMdnsEnd();
  g_run = false;
  g_riprovaAlle = 0;
}

bool netMdnsRun() { return g_run; }

// Un avvio. true = parte; false = non e partito (il chiamante decide se riprovare).
static bool avvia() {
  drvMdnsEnd();
  if (!drvMdnsBegin(cfg.hostname.c_str())) return false;
  g_run = true;
  if (cfg.httpOn) drvMdnsAddService("http", "tcp", cfg.httpPort);
  if (cfg.https) drvMdnsAddService("https", "tcp", cfg.httpsPort);
  ramTrace("mdns");
  return true;
}

static void programmaRiprova() {
  if (g_tentativi >= TENTATIVI_MAX) {
    vlog("%s", tr("NET: mDNS non parte, rinuncio (si riprova da Servizi o al prossimo cambio di rete)"));
    return;
  }
  uint32_t attesaS = 5u << g_tentativi;
  g_tentativi++;
  g_riprovaAlle = (uint32_t)drvMillis() + attesaS * 1000u;
  if (!g_riprovaAlle) g_riprovaAlle = 1;
  vlog("%s", trf("NET: mDNS non parte (RAM libera %u KB), riprovo tra %u s", (unsigned)(drvHeapFree() / 1024), (unsigned)attesaS).c_str());
}

void netMdnsApply() {
  netMdnsStop();
  g_tentativi = 0;
  if (primoAvvioMinimo()) return;                    // prima accensione: niente mDNS (RAM)
  if (!cfg.mdnsOn) { vlog("NET: mDNS spento"); return; }
  if (!avvia()) programmaRiprova();
}

void netMdnsRiprova() {
  if (!g_riprovaAlle || (int32_t)((uint32_t)drvMillis() - g_riprovaAlle) < 0) return;
  g_riprovaAlle = 0;
  if (g_run || !cfg.mdnsOn || primoAvvioMinimo()) return;
  if (avvia()) vlog("NET: mDNS partito al nuovo tentativo");
  else programmaRiprova();
}
