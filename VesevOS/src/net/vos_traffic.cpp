// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_traffic.cpp
// Conta i byte che passano dalle interfacce Wi-Fi (STA e hotspot) mettendo un piccolo contatore davanti alle
// funzioni di ingresso e uscita di lwIP. Si somma il traffico delle due interfacce. Nessun contenuto, nessun indirizzo.
#include "vos_traffic.h"
#include <esp_netif.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>

extern "C" void* esp_netif_get_netif_impl(esp_netif_t* esp_netif);   // dichiarata a mano: in alcune versioni di ESP-IDF sta in un file interno

#define TR_HIST 60
#define TR_IF   2

static volatile uint32_t g_in = 0, g_out = 0;                 // totali (si riavvolgono: si usa la differenza)
static struct netif*       g_nf[TR_IF] = {nullptr, nullptr};
static netif_input_fn      g_origIn[TR_IF];
static netif_linkoutput_fn g_origOut[TR_IF];
static uint32_t g_inH[TR_HIST], g_outH[TR_HIST];
static int      g_n = 0;
static uint32_t g_lastIn = 0, g_lastOut = 0;

template <int I> static err_t inWrap(struct pbuf* p, struct netif* n) { if (p) g_in += p->tot_len; return g_origIn[I](p, n); }
template <int I> static err_t outWrap(struct netif* n, struct pbuf* p) { if (p) g_out += p->tot_len; return g_origOut[I](n, p); }

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
  uint32_t i = g_in, o = g_out;
  uint32_t di = i - g_lastIn, dout = o - g_lastOut;
  g_lastIn = i; g_lastOut = o;
  if (g_n >= TR_HIST) {
    memmove(g_inH, g_inH + 1, (TR_HIST - 1) * sizeof(uint32_t));
    memmove(g_outH, g_outH + 1, (TR_HIST - 1) * sizeof(uint32_t));
    g_n = TR_HIST - 1;
  }
  g_inH[g_n] = di; g_outH[g_n] = dout; g_n++;
}

static void arr(String& j, const uint32_t* a, int n) {
  j += "[";
  for (int k = 0; k < n; k++) { if (k) j += ","; j += String((unsigned long)a[k]); }
  j += "]";
}

String trafficJson() {
  String j = "\"trafHook\":";
  j += (g_nf[0] || g_nf[1]) ? "true" : "false";
  j += ",\"inHist\":"; arr(j, g_inH, g_n);
  j += ",\"outHist\":"; arr(j, g_outH, g_n);
  return j;
}
