// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dev.h
// Registro delle periferiche: tutto (MQTT, Bluetooth, ESP-NOW, pin, LED, memoria...) risponde allo stesso modo:
// elenco, stato, acceso/spento, dettagli (JSON) e azioni. Le usano la pagina, la shell (comando "dev") e, in futuro, le app.
// Due tipi: DEV_VIRT = periferica virtuale (servizio con la sua pila), DEV_HW = hardware vero (Device Manager).
#pragma once
#include <Arduino.h>

enum DevKind : uint8_t { DEV_VIRT = 0, DEV_HW = 1 };

struct DevOps {
  const char* id;        // nome corto per shell e API ("mqtt", "pin"...)
  const char* label;     // nome mostrato (tradotto)
  const char* cap;       // capacita: gruppo mostrato nel pannello
  uint8_t kind;          // DEV_VIRT o DEV_HW
  const char* tab;       // scheda della pagina che la configura
  uint8_t jsonRole;      // ruolo minimo per leggere i dettagli
  int  (*state)();                                   // 0 spenta, 1 accesa, 2 accesa ma con problema
  bool (*set)(bool on, String& err);                 // nullptr = non si accende/spegne da qui
  String (*json)();                                  // dettagli (JSON)
  bool (*act)(const String& a, const String& arg, int role, String& out, String& err);   // azioni (nullptr = nessuna)
};

int            devCount();
const DevOps*  devAt(int i);
const DevOps*  devFind(const String& id);
String         devListJson();                          // {"dev":[{id,label,cap,kind,tab,state,sw,act}]}
String         devListText();                          // per la shell
String         devStatusJson(const String& id, int role);   // dettagli; "" se non esiste o ruolo insufficiente
bool           devSet(const String& id, bool on, String& err);
bool           devAct(const String& id, const String& a, const String& arg, int role, String& out, String& err);
