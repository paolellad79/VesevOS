// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dns.h
// Piccolo server DNS del portale automatico (captive portal): in modo hotspot ogni nome punta alla scheda.
#pragma once
#include <stdint.h>

bool dnsCaptiveStart(const uint8_t ip[4]);   // UDP 53; ogni richiesta di indirizzo (A) riceve questo IP. false se la porta non e libera
void dnsCaptiveStop();
bool dnsCaptiveRunning();
