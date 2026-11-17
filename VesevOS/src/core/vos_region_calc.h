// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region_calc.h
// ATTENZIONE - NON MODIFICARE senza un controllo legale: i canali e la potenza dipendono dal paese e dal guadagno
// dell'antenna (limite EIRP di legge). Un valore sbagliato puo violare le norme radio. Non e consulenza legale.
// Calcolo puro della potenza radio (nessuna dipendenza): serve al firmware (vos_region.cpp) e alla prova sul computer.
// Il limite di legge e un EIRP, cioe potenza al chip + guadagno dell'antenna: la potenza ammessa al chip e il limite meno
// il guadagno. Il guadagno e in decimi di dBi (43 = 4,3 dBi) e il risultato si arrotonda per DIFETTO a dBm interi, cosi
// l'EIRP non supera mai il limite. Non e consulenza legale.
#pragma once

#define REGION_POTENZA_MINIMA_DBM 2     // sotto questo valore la radio non e utile (come prima del 16/11/2026)

// Potenza massima al chip (dBm interi): min(limite - guadagno, tetto del chip), mai sotto il minimo.
static inline int regionCalcolaMaxDbm(int limiteEirpDbm, int guadagnoDeciDbi, int tettoChipDbm) {
  int decimi = limiteEirpDbm * 10 - guadagnoDeciDbi;
  int dbm = (decimi >= 0) ? decimi / 10 : -((-decimi + 9) / 10);    // per difetto anche se negativo
  if (dbm > tettoChipDbm) dbm = tettoChipDbm;
  if (dbm < REGION_POTENZA_MINIMA_DBM) dbm = REGION_POTENZA_MINIMA_DBM;
  return dbm;
}

// Con l'hotspot acceso la scheda puo avere un tetto proprio (tettoHotspotDbm = 0: nessun tetto): vale il minimo.
static inline int regionCalcolaConHotspot(int dbm, bool hotspotAcceso, int tettoHotspotDbm) {
  if (hotspotAcceso && tettoHotspotDbm > 0 && dbm > tettoHotspotDbm) return tettoHotspotDbm;
  return dbm;
}
