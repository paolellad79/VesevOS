// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_service.h
// STRATO SERVIZI: un solo modo di avviare, fermare e leggere lo stato di un servizio (Bluetooth oggi; ESP-NOW, MQTT... dopo).
// Ogni servizio descrive se stesso con una ServiceOps e si iscrive con serviceRegister(). Pagina, shell e app usano solo queste funzioni.
// Il servizio NON tocca l'hardware: lo fa un driver (vos_drv_*). Vedi claude/architettura/VesevOS-schema-strati.md.
#pragma once
#include <Arduino.h>

enum ServiceState : int { SVC_OFF = 0, SVC_ON = 1, SVC_ON_PROBLEM = 2, SVC_STARTING = 3, SVC_ERROR = 4 };

struct ServiceOps {
  const char* id;          // nome corto ("ble")
  const char* label;       // nome mostrato (tradotto)
  const char* cap;         // gruppo nel pannello
  const char* tab;         // scheda della pagina che lo configura
  uint8_t roleRead;        // ruolo minimo per leggere i dettagli
  uint8_t roleWrite;       // ruolo minimo per avviare/fermare
  int      (*state)();                      // ServiceState
  bool     (*begin)(String& err);           // avvia (se e gia acceso: rinnova)
  void     (*stop)();                       // ferma
  bool     (*restart)(String& err);         // nullptr = stop poi begin
  String   (*json)();                       // dettagli (JSON)
  bool     (*act)(const String& a, const String& arg, int role, String& out, String& err);   // nullptr = nessuna azione
  uint32_t (*ramBytes)();                   // RAM usata adesso (stima), nullptr = non nota
};

#define VOS_MAX_SERVICES 16

bool               serviceRegister(const ServiceOps* s);        // false se pieno o id doppio
int                serviceCount();
const ServiceOps*  serviceAt(int i);
const ServiceOps*  serviceFind(const String& id);
int                serviceState(const String& id);               // SVC_OFF se sconosciuto
bool               serviceStart(const String& id, String& err);
bool               serviceStop(const String& id, String& err);   // false solo se sconosciuto
bool               serviceRestart(const String& id, String& err);
uint32_t           serviceRam(const String& id);
