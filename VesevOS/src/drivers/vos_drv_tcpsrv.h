// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_tcpsrv.h
// STRATO 1 (driver): server TCP minimo sopra i socket di lwIP, per risposte brevi "una domanda, una risposta" (oggi: il redirect
// da HTTP a HTTPS sulla porta 80). Un solo task con stack piccolo, UNA connessione alla volta, niente keep-alive.
// Limiti fissi contro chi rallenta o inonda: la richiesta si legge per al massimo TCPSRV_IN_MAX byte e TCPSRV_READ_MS millisecondi.
#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr size_t   TCPSRV_IN_MAX   = 1024;   // byte di richiesta letti al massimo (il resto si butta)
constexpr size_t   TCPSRV_OUT_MAX  = 768;    // byte di risposta al massimo
constexpr uint32_t TCPSRV_READ_MS  = 3000;   // tempo totale per ricevere la richiesta; poi si chiude senza rispondere

// Gestore: peerIp = indirizzo di chi ha chiamato (ordine umano, primo numero nel byte alto); in = richiesta letta, chiusa con 0 e
// modificabile (n byte); out = dove scrivere la risposta (al massimo cap byte). Ritorna i byte da inviare, 0 = chiudi senza rispondere.
typedef size_t (*DrvTcpHandler)(uint32_t peerIp, char* in, size_t n, char* out, size_t cap);

class DrvTcpMini {
public:
  bool begin(uint16_t port, DrvTcpHandler h, const char* taskName);   // false se la porta non e disponibile o manca la memoria
  bool running() const { return task_ != nullptr; }
private:
  static void run(void* self);
  void        serve(int c, uint32_t peerIp, char* in, char* out);
  int           sock_ = -1;
  DrvTcpHandler handler_ = nullptr;
  void*         task_ = nullptr;      // TaskHandle_t
};
