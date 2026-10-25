// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_traffic.cpp
// Conta i byte che passano dalle interfacce Wi-Fi (STA e hotspot) mettendo un piccolo contatore davanti alle
// funzioni di ingresso e uscita di lwIP. Si somma il traffico delle due interfacce. Nessun contenuto, nessun indirizzo.
#include "vos_traffic.h"
#include "../drivers/vos_drv_activity.h"
#include <esp_netif.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>

extern "C" void* esp_netif_get_netif_impl(esp_netif_t* esp_netif);   // dichiarata a mano: in alcune versioni di ESP-IDF sta in un file interno

#define TR_HIST 60
#define TR_IF   2

static struct netif*       g_nf[TR_IF] = {nullptr, nullptr};
static netif_input_fn      g_origIn[TR_IF];
static netif_linkoutput_fn g_origOut[TR_IF];
static uint32_t g_inH[TR_HIST], g_outH[TR_HIST];
static int      g_n = 0;
static uint32_t g_lastIn = 0, g_lastOut = 0;

// I contatori stanno nel driver (vos_drv_activity.h): li legge anche il LED per il trigger "wifi".
template <int I> static err_t inWrap(struct pbuf* p, struct netif* n) {
  if (p) drvActAdd(ACT_WIFI_RX, p->tot_len);
  return g_origIn[I](p, n);
}
template <int I> static err_t outWrap(struct netif* n, struct pbuf* p) {
  if (p) drvActAdd(ACT_WIFI_TX, p->tot_len);
  return g_origOut[I](n, p);
}

static void hook(int i, const char* key) {
  esp_netif_t* e = esp_netif_get_handle_from_ifkey(key);
  if (!e) return;
  struct netif* n = (struct netif*)esp_netif_get_netif_impl(e);
  if (!n || n == g_nf[i]) return;                             // non c'e, oppure gia agganciata
  if (!n->input || !n->linkoutput) return;
  g_origIn[i] = n->input; g_origOut[i] = n->linkoutput;
  if (i == 0) { n->input = inWrap<0>; n->linkoutput = outWrap<0>; }
  else        { n->input = inWrap<1>; n->linkoutput = outWrap<1>; }
  g_nf[i] = n;
}

void trafficTick() {
  hook(0, "WIFI_STA_DEF");
  hook(1, "WIFI_AP_DEF");
  uint32_t i = drvActGet(ACT_WIFI_RX), o = drvActGet(ACT_WIFI_TX);   // totali (si riavvolgono: si usa la differenza)
  uint32_t di = i - g_lastIn, dout = o - g_lastOut;
  g_lastIn = i; g_lastOut = o;
  if (g_n >= TR_HIST) {
    memmove(g_inH, g_inH + 1, (TR_HIST - 1) * sizeof(uint32_t));
    memmove(g_outH, g_outH + 1, (TR_HIST - 1) * sizeof(uint32_t));
    g_n = TR_HIST - 1;
  }
  g_inH[g_n] = di; g_outH[g_n] = dout; g_n++;
}

static void arr(StrBuf& j, const uint32_t* a, int n) {
  j.add('[');
  for (int k = 0; k < n; k++) { if (k) j.add(','); j.addu(a[k]); }
  j.add(']');
}

size_t trafficJson(char* out, size_t cap) {
  StrBuf j(out, cap);
  j.add("\"trafHook\":").add((g_nf[0] || g_nf[1]) ? "true" : "false");
  j.add(",\"inHist\":"); arr(j, g_inH, g_n);
  j.add(",\"outHist\":"); arr(j, g_outH, g_n);
  return j.length();
}
