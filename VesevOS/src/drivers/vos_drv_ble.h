// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ble.h
// STRATO 1 (Driver): l'unico file che conosce la libreria NimBLE. Sa solo accendere/spegnere la radio Bluetooth,
// annunciarsi, mostrare un servizio con due caratteristiche (ricevi testo / invia testo) e dire se un telefono e collegato.
// NON conosce la configurazione della scheda, la rete, i comandi: chi lo usa gli passa i nomi e riceve i testi arrivati.
// Chi usa il driver: vos_ble (ciclo di vita) -> vos_ble_cfg (comandi). Pensato per altri servizi sullo stesso driver.
// Con VOS_WITH_BLE = 0 il driver non c'e.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_BLE
typedef void (*BleRxCb)(const String& text);      // e arrivato un testo (chiamata dal task Bluetooth: solo mettere da parte, lavoro lungo no)
typedef void (*BleLinkCb)(bool connected);        // un dispositivo si e collegato / scollegato

struct BleDrvConfig {                             // tutte le stringhe devono restare valide finche il driver e acceso (usare stringhe fisse)
  const char* devName;                            // nome visibile
  const char* svcUuid;                            // servizio
  const char* rxUuid;                             // caratteristica in scrittura (dal telefono alla scheda)
  const char* txUuid;                             // caratteristica in lettura/notifica (dalla scheda al telefono)
  uint32_t    passkey;                            // codice di accoppiamento a 6 cifre (la scheda lo MOSTRA)
  const char* hello;                              // primo valore della caratteristica txUuid
  BleRxCb     onRx;
  BleLinkCb   onLink;
};

bool drvBlePrime();                               // avvio a vuoto + spegnimento (nessun annuncio): le poche allocazioni che NimBLE lascia dopo deinit vanno al boot, quando la RAM e ancora in un pezzo
bool drvBleBegin(const BleDrvConfig& c);          // init + sicurezza + servizio + annuncio
void drvBleEnd();                                 // spegne e libera tutto
bool drvBleIsUp();
bool drvBleConnected();
bool drvBleSend(const String& text);              // scrive il valore e avvisa il telefono
void drvBleAdvertise(bool on);                    // riprende o ferma l'annuncio (acceso)
// futuro: drvBleScan(...) per leggere altri dispositivi BLE (sensori)
#endif
