// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dev.h
// Registro delle periferiche: tutto (MQTT, Bluetooth, ESP-NOW, pin, LED, memoria...) risponde allo stesso modo:
// elenco, stato, acceso/spento, dettagli (JSON) e azioni. Le usano la pagina, la shell (comando "dev") e, in futuro, le app.
// Due tipi: DEV_VIRT = periferica virtuale (servizio con la sua pila), DEV_HW = hardware vero (Device Manager).
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

enum DevKind : uint8_t { DEV_VIRT = 0, DEV_HW = 1 };

struct DevOps {
  const char* id;        // nome corto per shell e API ("mqtt", "pin"...)
  const char* label;     // nome mostrato (tradotto)
  const char* cap;       // capacita: gruppo mostrato nel pannello
  uint8_t kind;          // DEV_VIRT o DEV_HW
  const char* tab;       // scheda della pagina che la configura
  uint8_t jsonRole;      // ruolo minimo per leggere i dettagli
  int  (*state)();                                   // 0 spenta, 1 accesa, 2 accesa ma con problema
  bool (*set)(bool on, ErrBuf& err);                 // nullptr = non si accende/spegne da qui
  bool (*json)(HeapBuf& out);                        // dettagli (JSON) in out; false = manca la memoria
  bool (*act)(const char* a, const char* arg, int role, HeapBuf& out, ErrBuf& err);   // azioni (nullptr = nessuna)
};

void           devSetVirtual(const DevOps* list, int n);   // periferiche virtuali dei package (le iscrive packages/vos_dev_pkg), in testa all'elenco
int            devCount();
const DevOps*  devAt(int i);
const DevOps*  devFind(const char* id);
// I JSON e i testi si scrivono in un HeapBuf del chiamante (false = manca la memoria o supera il tetto).
#define DEV_JSON_MAX 16384
bool           devListJson(HeapBuf& out);              // {"dev":[{id,label,cap,kind,tab,state,sw,act}]}
bool           devListText(HeapBuf& out);              // per la shell
bool           devStatusJson(const char* id, int role, HeapBuf& out);   // dettagli; false se non esiste, ruolo insufficiente (out vuoto) o memoria finita (out.oom()/over())
bool           devOffId(const char* id);                  // spegne la periferica (per il watchdog); false se non c'e o non si spegne
bool           devSet(const char* id, bool on, ErrBuf& err);
bool           devAct(const char* id, const char* a, const char* arg, int role, HeapBuf& out, ErrBuf& err);   // out: JSON della risposta (vuoto = solo ok)
