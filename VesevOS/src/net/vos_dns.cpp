// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dns.cpp
// Risponde a ogni domanda di tipo A (indirizzo IPv4) o ANY con l'IP della scheda; per gli altri tipi (per esempio AAAA)
// risponde "nessun errore, nessuna risposta", cosi i telefoni passano subito a IPv4. Una sola domanda per pacchetto.
#include "vos_dns.h"
#include <string.h>
#include "../drivers/vos_drv_udp.h"

static DrvUdp  g_udp;
static uint8_t g_ip[4] = { 192, 168, 4, 1 };

static const uint16_t TYPE_A = 1, TYPE_ANY = 255, CLASS_IN = 1;
static const uint32_t TTL_SEC = 60;

static uint16_t rd16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static void wr16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v & 255; }

// Lunghezza del nome nella domanda (dal byte 12), 0 se non e valido
static size_t nameLen(const uint8_t* in, size_t n) {
  size_t i = 12;
  while (i < n) {
    uint8_t l = in[i];
    if (l == 0) return i + 1 - 12;
    if (l > 63) return 0;                       // niente compressione nella domanda
    i += 1 + l;
  }
  return 0;
}

static size_t dnsReply(const uint8_t* in, size_t n, uint8_t* out, size_t cap) {
  if (n < 17) return 0;
  uint16_t flags = rd16(in + 2);
  if ((flags & 0x8000) || ((flags >> 11) & 0xF) != 0) return 0;     // solo domande standard
  if (rd16(in + 4) != 1) return 0;                                   // una sola domanda
  size_t nl = nameLen(in, n);
  if (!nl || 12 + nl + 4 > n) return 0;
  size_t qEnd = 12 + nl + 4;                                         // fine della domanda
  uint16_t qtype = rd16(in + 12 + nl), qclass = rd16(in + 14 + nl);
  bool answer = (qtype == TYPE_A || qtype == TYPE_ANY) && qclass == CLASS_IN;
  if (qEnd + (answer ? 16 : 0) > cap) return 0;

  memcpy(out, in, qEnd);                                             // intestazione e domanda come arrivate
  wr16(out + 2, 0x8400 | (flags & 0x0100));                          // risposta, autorevole, RD copiato, nessun errore
  wr16(out + 6, answer ? 1 : 0);                                     // risposte
  wr16(out + 8, 0); wr16(out + 10, 0);                               // niente autorita ne dati aggiuntivi
  if (!answer) return qEnd;

  uint8_t* a = out + qEnd;
  wr16(a, 0xC00C);                                                   // il nome e quello della domanda
  wr16(a + 2, TYPE_A); wr16(a + 4, CLASS_IN);
  a[6] = TTL_SEC >> 24; a[7] = TTL_SEC >> 16; a[8] = TTL_SEC >> 8; a[9] = TTL_SEC;
  wr16(a + 10, 4);
  memcpy(a + 12, g_ip, 4);
  return qEnd + 16;
}

bool dnsCaptiveStart(const uint8_t ip[4]) {
  if (g_udp.running()) return true;
  memcpy(g_ip, ip, 4);
  return g_udp.begin(53, dnsReply, "dns");
}

void dnsCaptiveStop() { g_udp.end(); }
bool dnsCaptiveRunning() { return g_udp.running(); }
