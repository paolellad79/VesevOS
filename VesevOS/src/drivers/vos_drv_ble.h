// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ble.h
// STRATO 1 (Driver): l'unico file che conosce la libreria Bluetooth (NimBLE). Offre solo PRIMITIVE generiche:
//   accendi/spegni, nome, annuncio, UN servizio definito a runtime (fino a 4 caratteristiche), notifica, disconnessione.
// NON conosce Berry, la configurazione della scheda, la rete o i comandi: chi lo usa gli passa i dati e riceve gli eventi
// tramite puntatori a funzione (BleDrvEvents). Nessun servizio, nessun testo e nessun annuncio sono incorporati.
//
// SICUREZZA (sempre attiva, non si puo spegnere):
//   - ogni lettura e scrittura richiede un collegamento cifrato e abbinato con codice a 6 cifre (LE Secure Connections);
//     il codice lo sceglie chi accende (casuale a ogni accensione) e lo SA solo lui: il driver non lo scrive in nessun posto;
//   - una sola connessione per volta (le altre sono chiuse subito);
//   - al massimo 10 scritture e 10 notifiche al secondo, il resto e scartato;
//   - collegamento non abbinato entro 30 s, o fermo da 120 s: chiuso;
//   - dopo 3 collegamenti chiusi senza abbinamento il driver smette di annunciarsi e chiama onLocked (chi usa spegne tutto).
// CONCORRENZA: le funzioni vanno chiamate da un solo task alla volta (lo garantisce il blocco di packages/vos_ble).
// I callback arrivano dal task Bluetooth: solo mettere da parte i dati, niente lavoro lungo e niente blocchi.
// Futuro (NON presente): scansione e ruolo centrale (drvBleScan) per leggere altri dispositivi.
// Con VOS_WITH_BLE = 0 il driver non c'e.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../boards/vos_packages.h"

#if VOS_WITH_BLE
#define BLE_MAX_CHARS  4                 // caratteristiche del servizio
#define BLE_VALUE_MAX  128               // byte massimi di ogni valore
#define BLE_NAME_MAX   20                // caratteri massimi del nome
#define BLE_UUID_TEXT  37                // 36 caratteri + 0

enum { BLE_PROP_READ = 1, BLE_PROP_WRITE = 2, BLE_PROP_NOTIFY = 4 };

enum BleDrvErr : uint8_t {
  BLE_OK = 0,
  BLE_ERR_ARG,        // parametro fuori dai limiti
  BLE_ERR_UUID,       // UUID non valido o riservato
  BLE_ERR_PROPS,      // proprieta non valide
  BLE_ERR_TOOMANY,    // troppe caratteristiche
  BLE_ERR_DUP,        // UUID ripetuto
  BLE_ERR_RAM,        // memoria insufficiente o radio non parte
  BLE_ERR_STATE,      // radio spenta, nessun telefono collegato o iscritto
  BLE_ERR_SECURE,     // collegamento non ancora abbinato
  BLE_ERR_RATE,       // troppi messaggi al secondo
  BLE_ERR_SIZE        // valore troppo lungo
};

struct BleCharDef { char uuid[BLE_UUID_TEXT]; uint8_t props; };
struct BleServiceDef { char uuid[BLE_UUID_TEXT]; uint8_t n; BleCharDef ch[BLE_MAX_CHARS]; };   // n = 0: nessun servizio

struct BleDrvEvents {                    // chiamati dal task Bluetooth; i puntatori possono essere nullptr
  void (*onWrite)(uint8_t idx, const uint8_t* data, uint16_t len);   // il telefono ha scritto la caratteristica idx (len <= BLE_VALUE_MAX)
  void (*onRead)(uint8_t idx);                                       // il telefono ha letto la caratteristica idx
  void (*onLink)(bool up);                                           // collegato / scollegato
  void (*onLocked)();                                                // troppi tentativi senza abbinamento: spegnere
};

struct BleDrvConfig {
  char          name[BLE_NAME_MAX + 1];  // nome visibile (1-20 caratteri, gia controllato da chi chiama)
  uint32_t      passkey;                 // codice di abbinamento a 6 cifre (100000-999999): lo sceglie chi chiama
  bool          advertise;               // annunciarsi subito
  uint16_t      advIntervalMs;           // intervallo dell'annuncio (100-2000)
  BleServiceDef svc;
  BleDrvEvents  ev;
};

// Controlla e normalizza (minuscolo) gli UUID e le proprieta del servizio. UUID ammessi: 4 cifre esadecimali (16 bit) o 36 caratteri
// 8-4-4-4-12. Rifiuta i 16 bit riservati (0x1800, 0x1801, 0x2800-0x29FF), i doppioni e piu di BLE_MAX_CHARS caratteristiche.
BleDrvErr drvBleCheckService(BleServiceDef& s);

bool      drvBlePrime();                        // avvio a vuoto + spegnimento (per la RAM frammentata; oggi non e chiamata da nessuno)
BleDrvErr drvBleBegin(const BleDrvConfig& c);   // init + sicurezza + servizio (+ annuncio se richiesto)
void      drvBleEnd();                          // spegne e libera tutto
bool      drvBleIsUp();
bool      drvBleConnected();
bool      drvBleSecure();                       // il collegamento e cifrato e abbinato
bool      drvBleLocked();                       // bloccato per troppi tentativi
bool      drvBleAdvertise(bool on);             // riprende o ferma l'annuncio (acceso); fermo non e piu raggiungibile
BleDrvErr drvBleSetValue(uint8_t idx, const uint8_t* data, uint16_t len);   // valore che il telefono leggera
BleDrvErr drvBleNotify(uint8_t idx, const uint8_t* data, uint16_t len);     // imposta il valore e avvisa il telefono iscritto
void      drvBleDisconnect();                   // chiude il collegamento in corso
void      drvBleTick();                         // dal ciclo principale: scadenze di abbinamento e inattivita
#endif
