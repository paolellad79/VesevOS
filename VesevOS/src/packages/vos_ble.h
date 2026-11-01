// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.h
// Bluetooth (BLE): nel firmware resta SPENTO e INUTILIZZABILE da solo (decisione dell'utente, 26/10/2026).
// Non ha servizi, canali di testo, annunci, comandi o interruttori propri: ne' nella shell, ne' nella pagina, ne' nella configurazione.
// Lo accende SOLO un'app Berry con il permesso "ble" nel manifest (use=ble), con le funzioni ble_on, ble_name, ble_adv, ble_service...
// (vos_berry_ble.cpp). Qui sta la POLITICA e il ciclo di vita; la radio sta in drivers/vos_drv_ble.
//
// REGOLE
//   - Un solo programma alla volta possiede il Bluetooth (il suo nome e il "proprietario"); un secondo riceve un errore chiaro.
//   - Si spegne e si pulisce da solo (servizio, annunci, collegamenti, coda degli eventi) quando il programma finisce, si ferma,
//     fallisce, scade il tempo chiesto (ble_on(minuti), massimo BLE_MINUTES_MAX) o i collegamenti falliscono troppe volte.
//   - Il collegamento richiede sempre abbinamento con codice a 6 cifre casuale (diverso a ogni accensione). Il codice si vede solo
//     con ble_passkey() in un programma lanciato dall'Admin: mai nel registro, nelle verifiche, negli eventi, nello stato.
//   - Il registro delle verifiche (audit) annota accensione e spegnimento col nome del programma, senza segreti.
// Con VOS_WITH_BLE = 0 (vos_common.h) il modulo non c'e.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_common.h"
#include "vos_ble_types.h"

#if VOS_WITH_BLE
// ---- per i programmi Berry con il permesso ble (owner = nome del programma in corso) ----
bool bleAppOn(const char* owner, uint32_t minutes, ErrBuf& err);           // minuti 0 = finche il programma gira
bool bleAppOff(const char* owner, ErrBuf& err);                            // rilascia e spegne (solo il proprietario)
bool bleAppName(const char* owner, const char* name, ErrBuf& err);         // 1-20 caratteri (lettere, cifre, spazio . _ -); prima di ble_on
bool bleAppAdv(const char* owner, bool on, ErrBuf& err);                   // l'annuncio parte solo se richiesto
bool bleAppService(const char* owner, BleServiceDef& def, ErrBuf& err);    // prima di ble_on; normalizza gli UUID
bool bleAppSet(const char* owner, const char* uuid, const uint8_t* d, size_t n, ErrBuf& err);     // valore letto dal telefono
bool bleAppNotify(const char* owner, const char* uuid, const uint8_t* d, size_t n, ErrBuf& err);
bool bleAppDisconnect(const char* owner, ErrBuf& err);
bool bleAppPopEvent(const char* owner, BleAppEvent& ev);                   // false = nessun evento
bool bleAppUuid(const char* owner, uint8_t idx, char* out, size_t n);      // UUID della caratteristica idx
void bleAppEnded(const char* owner);                                       // il programma e finito: si spegne tutto (lo fa bleTick)
void bleSetOwnerCheck(bool (*alive)(const char* owner));                   // chi sa dire se il programma gira ancora (rete di sicurezza)

// ---- stato e controllo generale ----
void   bleStatusLine(char* out, size_t n);        // "off [err=..]" oppure "on|connected owner=.. left=.. adv=.. paired=.. name=.."
bool   blePasskey(char* out, size_t n);           // il codice (6 cifre) se acceso; SOLO per chi e Admin: il chiamante decide
const char* bleStateName();                       // "off", "on" o "connected"
bool   bleRunning();
void   bleStop();                                 // spegnimento forzato (modo aereo, watchdog): mai ricordato
void   bleServiceInit();                          // iscrive il Bluetooth nel registro dei servizi (solo stato e stop)
void   bleTick();                                 // dal ciclo principale: avvio richiesto, scadenze, pulizia
#else
// Package spento (VOS_WITH_BLE = 0): funzioni vuote, il resto del codice non cambia.
inline const char* bleStateName() { return "off"; }
inline bool   bleRunning() { return false; }
inline void   bleStop() {}
inline void   bleServiceInit() {}
inline void   bleTick() {}
inline void   bleAppEnded(const char*) {}
inline void   bleStatusLine(char* out, size_t n) { if (n) snprintf(out, n, "off"); }
inline bool   blePasskey(char*, size_t) { return false; }
#endif
