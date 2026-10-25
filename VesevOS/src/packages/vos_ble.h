// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.h
// Bluetooth (BLE): INTERFACCIA DORMIENTE (decisione 22/10/2026). Lo stack resta compilato nel firmware, ma SPENTO:
// non alloca niente al boot e si inizializza (NimBLE + controller) solo quando qualcuno lo accende, senza riavvio.
// Il codice nativo non si puo aggiungere a runtime: per questo lo stack resta compilato; un driver (script Berry) lo usa da qui.
//
// CHI PUO' CHIAMARE COSA
//   - Utente (pagina, shell "ble on|off|status"): bleUserOn / bleUserOff. L'accensione senza limite di tempo si ricorda in
//     /vesevos.conf ([ble] on=1): al boot successivo il BLE riparte da solo. Con un limite di minuti NON si ricorda.
//   - Driver / app Berry con il permesso "ble" (use=ble nel manifest; negato di default): ble_on(), ble_off(), ble_status().
//     Non si ricordano nella configurazione e non toccano la scelta dell'utente. La richiesta e messa in coda ed eseguita dal
//     ciclo principale (bleTick): i driver NON inizializzano la radio dal proprio task.
//   - Spegnimento forzato (modo aereo, watchdog, scadenza, cambio nome): bleStop, mai ricordato.
// GARANZIE RICHIESTE A UN DRIVER FUTURO (che usi la radio dopo l'accensione)
//   1. Nessuna configurazione iniziale ne dato sensibile (password Wi-Fi, chiavi) via BLE senza accoppiamento con PIN
//      (RED/EN 18031, CRA). Il servizio attuale usa PIN a 6 cifre mostrato solo in pagina/shell, mai nel registro.
//   2. Nessun segreto (PIN, chiavi, password) nei log, negli eventi e nelle risposte di stato.
//   3. Niente scansioni o annunci non richiesti dall'utente; spegnere la radio quando non serve (ble_off()).
//   4. Chi accende deve gestire il rifiuto: modo aereo, memoria insufficiente o troppo frammentata (ble_status() resta "off").
// Con VOS_WITH_BLE = 0 (vos_common.h) il modulo non c'e e la pagina lo nasconde.
#pragma once
#include "../core/vos_errbuf.h"
#include <string>
#include "../core/vos_heapbuf.h"
#include "../drivers/vos_drv_string.h"
#include "../core/vos_common.h"

#define BLE_JSON_MAX 768
#if VOS_WITH_BLE
bool   bleUserOn(uint32_t minutes, ErrBuf& err);   // scelta dell'utente: acceso (0 minuti = senza limite e ricordato per il prossimo avvio)
void   bleUserOff();                         // scelta dell'utente: spento e non riparte al prossimo avvio
bool   bleEnabled();                         // impostazione "ble" della configurazione (parte al boot)
void   bleRequest(bool on);                  // per i driver Berry: chiede accensione/spegnimento, lo fa bleTick (non ricordato)
const char* bleStateName();                  // "off", "on" o "connected"
bool   bleStart(ErrBuf& err);     // acceso (senza limite, o per i minuti di bleSetLimit)
void   bleSetLimit(uint32_t minutes);        // minuti per la prossima accensione (0 = nessun limite, massimo 1440)
bool   bleLimited();                         // true se c'e un tempo che scade
uint32_t bleTotalSec();                      // durata totale del limite in corso (0 = nessun limite)
void   bleStop();
bool   bleRunning();
uint32_t bleLeftSec();                       // secondi rimasti (0 = spento)
void   bleServiceInit();          // iscrive il Bluetooth nel registro dei servizi (vos_service)
void   bleTick();                 // dal ciclo principale: esegue i comandi arrivati, spegne allo scadere
bool   bleJson(HeapBuf& out);             // false = manca la memoria o supera il tetto
std::string bleText();
#else
// Package spento (VOS_WITH_BLE = 0): funzioni vuote, il resto del codice non cambia.
inline bool   bleUserOn(uint32_t, ErrBuf& err) { err = "Bluetooth non presente in questo firmware"; return false; }
inline void   bleUserOff() {}
inline bool   bleEnabled() { return false; }
inline void   bleRequest(bool) {}
inline const char* bleStateName() { return "off"; }
inline bool   bleStart(ErrBuf& err) { err = "Bluetooth non presente in questo firmware"; return false; }
inline void   bleStop() {}
inline bool   bleRunning() { return false; }
inline void   bleSetLimit(uint32_t) {}
inline bool   bleLimited() { return false; }
inline uint32_t bleTotalSec() { return 0; }
inline uint32_t bleLeftSec() { return 0; }
inline void   bleServiceInit() {}
inline void   bleTick() {}
inline bool   bleJson(HeapBuf& out) { return out.add("{\"have\":false,\"on\":false}"); }
inline std::string bleText() { return "Bluetooth: non presente in questo firmware"; }
#endif
