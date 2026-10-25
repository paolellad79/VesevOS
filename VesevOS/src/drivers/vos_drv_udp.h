// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_udp.h
// STRATO 1 (driver): piccolo server UDP sopra i socket di lwIP. Un task che aspetta i pacchetti, chiama il gestore e risponde
// a chi ha scritto. Il task esiste SOLO finche il server e acceso. Lo usano il server NTP (porta 123) e il DNS del portale (porta 53).
#pragma once
#include <stddef.h>
#include <stdint.h>

// Gestore: riceve il pacchetto (in, n) e scrive la risposta in out (al massimo cap byte). Ritorna i byte della risposta, 0 = nessuna risposta.
typedef size_t (*DrvUdpHandler)(const uint8_t* in, size_t n, uint8_t* out, size_t cap);

class DrvUdp {
public:
  bool begin(uint16_t port, DrvUdpHandler h, const char* taskName);   // false se la porta non e disponibile
  void end();                                                         // chiude il socket e ferma il task (aspetta al massimo 2 s)
  bool running() const { return task_ != nullptr; }
private:
  static void run(void* self);
  int           sock_ = -1;
  DrvUdpHandler handler_ = nullptr;
  void*         task_ = nullptr;      // TaskHandle_t
  volatile bool stop_ = false;
};
