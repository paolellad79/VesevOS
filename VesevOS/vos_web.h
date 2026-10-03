// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.h
// Server web (porta 80), pagina e API. Tutto protetto da password.
#pragma once
#include <Arduino.h>

void webInit();     // registra i percorsi (non avvia il server)
void webStart();    // avvia il server: chiamare dopo l'avvio del Wi-Fi
