// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_hooks.h
// GANCI verso gli strati alti: core, devices e net NON includono packages/ o interfaces/ (lo controlla checklayers).
// Quando serve una cosa di sopra (stato della pagina, MFA, MQTT...) la chiedono qui; i ganci li riempie vosWire()
// (src/vos_wire.cpp) all'inizio di setup(). Un gancio vuoto = il pezzo non c'e: si risponde "spento"/"no".
// Lo stato di MQTT, ESP-NOW e Bluetooth si legge dal registro dei servizi (serviceState), non da qui.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "vos_common.h"     // NetState
#include "../drivers/vos_drv_print.h"   // OutSink (uscita testo)

struct VosHooks {
  void (*webInit)();                                // avvio: registra i percorsi della pagina
  void (*webStart)();                               // rete pronta: avvia i server
  bool (*webHttpsUp)();
  void (*webTrimIdle)();                            // RAM a pezzi e pagina ferma: chiude i collegamenti HTTPS fermi
  void (*rulesInit)();
  void (*mqttInit)();
  void (*meshInit)();
  bool (*mqttPublish)(const char* sub, const char* msg);   // <prefisso>/<sub>; false se non collegato
  bool (*mfaOn)(int user);
  int  (*btnWpsClicks)();                           // click del tasto BOOT che avviano il WPS (0 = nessuno)
  void (*radiosOff)();                              // modo aereo: ferma ESP-NOW e Bluetooth
  // LED, rete, traffico e periferiche stanno sopra il nucleo (devices/, net/): il nucleo li chiede qui
  void (*ledInit)();
  void (*netInit)();
  void (*ledAlarm)(int level);                      // 0 nessuno, 1 giallo, 2 rosso (registro eventi)
  void (*ledFault)(bool on);
  void (*ledShutdown)();
  NetState (*netState)();
  void (*trafficTick)();
  void (*ioAllOff)();                               // tutte le uscite spente (stato sicuro)
  bool (*devIsOn)(const char* id);                  // la periferica esiste ed e accesa
  bool (*devOff)(const char* id);                   // spegne la periferica; false se non c'e o non si spegne
  void (*shellExec)(const char* line, OutSink& out, int role);   // esegue un comando della shell (le regole lo usano per le azioni)
  // velocita della CPU: la sua logica (core/vos_cpupm) sta nel ciclo dei moduli di base, quindi il monitor di vos_sys la chiama da qui
  int  (*cpuTick)(float tempC, int busyPct);        // un giro al secondo; ritorna lo stato termico (CpuThermal come numero)
  void (*cpuApplyMode)();                           // la scelta della CPU e cambiata: va applicata al prossimo giro
  int  (*cpuStatus)(char* out, size_t n);           // pezzo di JSON dello stato della CPU (campi cpuMhz...cpuChanges); ritorna i byte scritti, -1 se non entra
  uint32_t (*webIdleSec)();                         // secondi da quando qualcuno ha usato la pagina (senza pagina: nessuno la usa)
};

inline VosHooks g_hooks = {};

inline void hookCall(void (*f)()) { if (f) f(); }
inline bool hookWebHttpsUp() { return g_hooks.webHttpsUp && g_hooks.webHttpsUp(); }
inline bool hookMfaOn(int u) { return g_hooks.mfaOn && g_hooks.mfaOn(u); }
inline bool hookMqttPublish(const char* sub, const char* msg) { return g_hooks.mqttPublish && g_hooks.mqttPublish(sub, msg); }
inline NetState hookNetState() { return g_hooks.netState ? g_hooks.netState() : NET_BOOT; }
inline void hookLedAlarm(int lv) { if (g_hooks.ledAlarm) g_hooks.ledAlarm(lv); }
inline void hookLedFault(bool on) { if (g_hooks.ledFault) g_hooks.ledFault(on); }
inline bool hookDevIsOn(const char* id) { return g_hooks.devIsOn && g_hooks.devIsOn(id); }
inline bool hookDevOff(const char* id) { return g_hooks.devOff && g_hooks.devOff(id); }
inline int  hookBtnWpsClicks() { return g_hooks.btnWpsClicks ? g_hooks.btnWpsClicks() : 0; }
inline void hookShellExec(const char* line, OutSink& out, int role) { if (g_hooks.shellExec) g_hooks.shellExec(line, out, role); }
inline int  hookCpuTick(float t, int busy) { return g_hooks.cpuTick ? g_hooks.cpuTick(t, busy) : 0; }
inline void hookCpuApplyMode() { if (g_hooks.cpuApplyMode) g_hooks.cpuApplyMode(); }
inline int  hookCpuStatus(char* out, size_t n) { if (g_hooks.cpuStatus) return g_hooks.cpuStatus(out, n); if (n) out[0] = 0; return 0; }
inline uint32_t hookWebIdleSec() { return g_hooks.webIdleSec ? g_hooks.webIdleSec() : 0xFFFFFFFFUL; }
