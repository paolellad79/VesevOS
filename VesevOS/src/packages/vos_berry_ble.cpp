// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_ble.cpp: funzioni Berry del Bluetooth (ble_on, ble_off, ble_status). Vedi l'interfaccia in vos_ble.h.
// Accendere e spegnere NON avviene qui: si mette una richiesta in coda e la esegue il ciclo principale (bleTick),
// cosi la radio non viene inizializzata dal task dello script. Niente scansione ne annunci: quelle le fara' un driver futuro.
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include "vos_ble.h"

static int bfBleOn(bvm* vm) { bleRequest(true); be_return_nil(vm); }          // chiede l'accensione (senza limite, non ricordata)
static int bfBleOff(bvm* vm) { bleRequest(false); be_return_nil(vm); }
static int bfBleStatus(bvm* vm) { be_pushstring(vm, bleStateName()); be_return(vm); }   // "off", "on" o "connected"

// Chiamata da vos_berry.cpp quando lo script/app ha il permesso (script di /scripts: sempre; app: use=ble)
void berryBleRegister(bvm* vm) {
  be_regfunc(vm, "ble_on", bfBleOn);
  be_regfunc(vm, "ble_off", bfBleOff);
  be_regfunc(vm, "ble_status", bfBleStatus);
}
#endif
