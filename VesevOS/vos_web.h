// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.h
// Server web: HTTPS (443) e HTTP (80, portale e hotspot). Pagina e API protette da utenti e ruoli.
#pragma once
#include <Arduino.h>

void webInit();     // registra i percorsi (non avvia il server)
void webStart();    // avvia i server (80 e 443): chiamare dopo l'avvio del Wi-Fi
void webAllowFirstPass();   // dopo il reset con BOOT: 10 minuti per scegliere la password anche dalla rete di casa
bool webHttpsUp();
uint32_t webIdleSec();                 // secondi dall'ultima richiesta della pagina
