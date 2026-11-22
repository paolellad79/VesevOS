// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_taratura.h
// Taratura della temperatura: una correzione in gradi (offset) per ogni sensore, salvata in decimi di grado Celsius
// (cfg.tempTarSoc = sensore interno del chip, cfg.tempTarExt = sensore esterno).
// Il valore grezzo non si perde: la correzione si applica UNA sola volta, con queste funzioni, da chi legge il sensore
// (chip: sysCpuTempLetta in vos_sys.cpp; esterno: ioI2cRead in vos_io_i2c.cpp).
// L'unita C/F resta solo una conversione di visualizzazione (fmtTemp): l'offset si applica prima, sempre in gradi Celsius.
// Solo header e senza altri include: non entra nei cicli tra moduli.
#pragma once
#include <stdint.h>
#include <math.h>

#define TEMP_TAR_MAX 200            // intervallo ammesso: da -200 a +200 decimi di grado (da -20,0 a +20,0 gradi)

inline float tempCorretta(float gradiC, int decimi) {              // gradi C grezzi -> corretti (NAN resta NAN)
  if (isnan(gradiC)) return gradiC;
  return gradiC + (float)decimi / 10.0f;
}

inline int32_t tempCorrettaCent(int32_t cent, int decimi) {        // come sopra, in centesimi di grado (il sensore esterno misura cosi)
  return cent + (int32_t)decimi * 10;
}
