// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_eventbus.h
// EVENT BUS: chi cambia stato lo annuncia (publish); chi vuole saperlo ascolta (subscribe) o chiede "cosa e successo dopo il numero N" (since).
// Anello fisso in RAM (nessun heap): CAP eventi, i piu vecchi vengono sovrascritti. Nessuna dipendenza dall'hardware (si prova sul computer).
// Regole: il gestore (Handler) deve essere BREVE, non blocca e non pubblica a sua volta. Nessun segreto negli argomenti.
#pragma once
#include <Arduino.h>

struct Event {
  uint32_t seq;       // numero progressivo (parte da 1, mai 0)
  uint32_t ms;        // millis() al momento della pubblicazione
  char     topic[16]; // "svc.state", "net.up"... (solo a-z 0-9 . _ -)
  char     arg[20];   // di solito un id ("ble")
  int32_t  val;       // un numero (es. nuovo stato)
};

class EventBus {
 public:
  static const int CAP = 24;       // eventi tenuti
  static const int MAXSUB = 8;     // ascoltatori
  typedef void (*Handler)(const Event&);

  static void     publish(const char* topic, const char* arg = "", int32_t val = 0);
  static bool     subscribe(const char* prefix, Handler h);    // prefix "" = tutti; false se pieno o gestore gia iscritto con lo stesso prefisso
  static bool     unsubscribe(Handler h);                      // toglie tutte le iscrizioni di h
  static uint32_t lastSeq();                                   // numero dell'ultimo evento (0 = nessuno)
  static int      since(uint32_t seq, Event* out, int max, uint32_t* lost = nullptr);   // eventi con numero > seq, dal piu vecchio; lost = quanti persi (sovrascritti)
  static String   json(uint32_t seq, int max = 20);            // {"last":N,"lost":k,"ev":[{"n":..,"ms":..,"t":"..","a":"..","v":..}]}
  static uint32_t ramBytes();                                  // RAM fissa usata
  static void     reset();                                     // solo per le prove
};
