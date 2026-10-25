// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ping.h
// STRATO 1 (driver): ping ICMP (echo) verso un indirizzo IPv4, con le sessioni "ping" di lwIP (esp_ping_new_session, componente lwip di
// ESP-IDF). E l'unico file che le usa. Una sola sessione alla volta. La funzione drvPingRun BLOCCA chi la chiama fino alla fine, ma ha
// sempre una scadenza (conteggio x intervallo + attesa + margine) e si puo annullare: non resta mai appesa.
// Qui stanno anche i LIMITI (conteggio, intervallo, attesa, dimensione, TTL): il driver li rifiuta anche se chi lo chiama sbaglia,
// cosi il comando non puo diventare uno strumento di flood. Il comando "ping" e la sua uscita: interfaces/vos_shell_ping.cpp.
// I tempi misurati da ESP-IDF sono in millisecondi interi: i microsecondi qui sotto sono quel valore per 1000.
#pragma once
#include <stdint.h>
#include <stddef.h>

// Limiti (gli indirizzi IP sono numeri con il primo numero in alto: 192.168.1.5 = 0xC0A80105, come ipParse()/ipTo())
#define DRV_PING_COUNT_MAX        20u       // pacchetti per sessione
#define DRV_PING_INTERVAL_MIN_MS  200u      // pausa minima tra due pacchetti
#define DRV_PING_INTERVAL_MAX_MS  30000u    // (serve a non far traboccare il calcolo della scadenza)
#define DRV_PING_TIMEOUT_MIN_MS   100u      // attesa minima della risposta
#define DRV_PING_TIMEOUT_MAX_MS   5000u     // attesa massima della risposta
#define DRV_PING_SIZE_MIN         16u       // byte di dati (senza le intestazioni)
#define DRV_PING_SIZE_MAX         1024u

struct DrvPingReq {
  uint32_t ip;          // destinazione IPv4
  uint32_t count;       // pacchetti da inviare (1..DRV_PING_COUNT_MAX)
  uint32_t intervalMs;  // pausa tra un invio e il successivo
  uint32_t timeoutMs;   // quanto aspettare ogni risposta
  uint32_t dataSize;    // byte di dati nel pacchetto (56 = pacchetto da 84 byte con intestazioni)
  uint32_t ttl;         // 1..255
};

// Un pacchetto inviato: la risposta (ok) oppure la mancata risposta.
struct DrvPingSample {
  uint16_t seq;         // numero d'ordine, da 1
  bool     ok;          // false = nessuna risposta entro l'attesa
  uint8_t  ttl;         // TTL della risposta (solo se ok)
  uint16_t size;        // byte della risposta con l'intestazione ICMP, come "64 bytes from" (solo se ok)
  uint32_t us;          // tempo di andata e ritorno in microsecondi (solo se ok)
};

// Risultato finale. Tempi in microsecondi (rtt: solo sui pacchetti con risposta; 0 se non ce ne sono).
struct DrvPingResult {
  uint32_t sent;        // pacchetti realmente inviati
  uint32_t received;
  uint32_t totalMs;     // dall'avvio all'ultimo pacchetto (come "time 3004ms" di ping)
  uint32_t minUs, avgUs, maxUs, mdevUs;
  bool     cancelled;   // fermata da chi chiama (Ctrl+C)
  bool     timedOut;    // fermata dalla scadenza di sicurezza
};

enum DrvPingErr : uint8_t {
  DP_OK = 0,
  DP_BUSY,              // un'altra sessione e in corso
  DP_BAD_ARG,           // un valore fuori dai limiti
  DP_NO_MEM,            // la sessione non si e potuta creare (memoria, socket)
  DP_START,             // la sessione c'e ma non parte
};

typedef void (*DrvPingSampleFn)(const DrvPingSample& s, void* user);   // chiamata, nel task di chi chiama, per ogni pacchetto concluso
typedef bool (*DrvPingCancelFn)(void* user);                          // chiamata ogni 100 ms: true = ferma il ping

// Avvia la sessione e aspetta la fine. onSample e cancel possono essere nullptr. Con DP_OK il risultato e completo (anche se annullato
// o scaduto: guarda cancelled e timedOut); con ogni altro codice niente e partito e out resta a zero.
DrvPingErr drvPingRun(const DrvPingReq& req, DrvPingSampleFn onSample, DrvPingCancelFn cancel, void* user, DrvPingResult& out);
