// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mesh.h
// Rete tra schede con ESP-NOW (servizio "mesh", SPENTO di fabbrica: la prima volta si accende a mano).
// - Ogni messaggio e firmato con la chiave comune (HMAC-SHA256): chi non ha la chiave non puo comandare.
// - Ripetizione automatica fino a 3 salti; i doppioni si scartano; numeri di sequenza contro la ripetizione.
// - Ruoli: 0 nodo, 1 gateway (annuncia il canale, pubblica tutti su MQTT), 2 sensore (manda solo lo stato).
// - Solo sui canali ammessi dal paese scelto. Il contenuto non e cifrato: niente dati personali o segreti.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include <string>
#include "../drivers/vos_drv_string.h"
#include "../core/vos_common.h"

constexpr size_t MESH_KEY_CHARS = 64;

#if VOS_WITH_MESH
void   meshInit();                                  // registra il servizio; parte solo se gia attivato a mano (auto)
bool   meshStart(ErrBuf& err);                      // accende (serve il Wi-Fi acceso e la chiave)
void   meshStop();
bool   meshRunning();
int    meshNodeCount();                             // schede vicine conosciute
bool   meshSendCmd(const char* to, const char* cmd, ErrBuf& err);    // to = nome o MAC; esegue un'azione sull'altra scheda
bool   meshSendText(const char* to, const char* text, ErrBuf& err);  // to = nome, MAC o "*" (tutti)
void meshNewKey(char (&k)[MESH_KEY_CHARS + 1]);            // 64 cifre esadecimali casuali (piu lo zero finale)
bool        meshJson(HeapBuf& out);                // stato, nodi, ultimi messaggi, in out (per i servizi e i dispositivi)
std::string meshJsonStd();                          // lo stesso, senza copie: per la risposta web
std::string meshText();
int    meshChannel();                               // canale in uso
#else
// Package spento (VOS_WITH_MESH = 0): funzioni vuote, il resto del codice non cambia.
inline void   meshInit() {}
inline bool   meshStart(ErrBuf& err) { err = "ESP-NOW non presente in questo firmware"; return false; }
inline void   meshStop() {}
inline bool   meshRunning() { return false; }
inline int    meshNodeCount() { return 0; }
inline bool   meshSendCmd(const char*, const char*, ErrBuf& err) { err = "ESP-NOW non presente in questo firmware"; return false; }
inline bool   meshSendText(const char*, const char*, ErrBuf& err) { err = "ESP-NOW non presente in questo firmware"; return false; }
inline void meshNewKey(char (&k)[MESH_KEY_CHARS + 1]) { k[0] = 0; }
inline bool meshJson(HeapBuf& out) { return out.add("{\"present\":false}"); }
inline std::string meshJsonStd() { return "{\"present\":false}"; }
inline std::string meshText() { return "ESP-NOW: non presente in questo firmware"; }
inline int    meshChannel() { return 0; }
#endif
