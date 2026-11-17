// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net_clienti.cpp
// Registro dei dispositivi che entrano o escono dall'hotspot. L'evento arriva dal task degli eventi (pila piccola):
// qui si copia in un anello e basta; la riga di registro la scrive il task di rete (netClientiLog, una volta al secondo).
// Del MAC si tengono solo gli ultimi 3 byte (niente dati personali oltre a quelli).
#include "vos_net.h"
#include "../drivers/vos_drv_wifi.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include <string.h>
#include <stdio.h>

namespace {
struct Voce { bool entrato; uint8_t mac3[3]; int aid; int motivo; };
const int ANELLO = 8;
Voce g_anello[ANELLO];
volatile int g_scrivi = 0;      // scrive solo il task degli eventi
int g_leggi = 0;                // legge solo il task di rete

void alEvento(bool entrato, const uint8_t mac[6], int aid, int motivo) {
  Voce& v = g_anello[g_scrivi % ANELLO];
  v.entrato = entrato;
  memcpy(v.mac3, mac + 3, 3);
  v.aid = aid;
  v.motivo = motivo;
  g_scrivi = g_scrivi + 1;
}
}  // namespace

void netClientiInit() { drvWifiOnApClient(alEvento); }

void netClientiLog() {
  int fine = g_scrivi;
  if (fine - g_leggi > ANELLO) g_leggi = fine - ANELLO;       // si e perso qualcosa: si parte dai piu recenti
  while (g_leggi < fine) {
    const Voce& v = g_anello[g_leggi % ANELLO];
    char mac[9];
    snprintf(mac, sizeof mac, "%02x:%02x:%02x", v.mac3[0], v.mac3[1], v.mac3[2]);
    if (v.entrato) vlog("%s", trf("NET: client entrato %s aid %d", mac, v.aid).c_str());
    else vlog("%s", trf("NET: client uscito %s aid %d, motivo %d", mac, v.aid, v.motivo).c_str());
    g_leggi++;
  }
}
