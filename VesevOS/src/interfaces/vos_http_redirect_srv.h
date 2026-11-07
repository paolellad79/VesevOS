// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_http_redirect_srv.h
// Server HTTP MINIMO: un task con un socket che sulla porta HTTP risponde solo il redirect a HTTPS (regole in vos_redirect_plan.h).
// Niente rotte, niente pagina, niente cookie, niente dati. Parte solo quando HTTPS e in funzione.
#pragma once
#include <stdint.h>

bool httpRedirectSrvStart(uint16_t port);   // false se la porta o la memoria non bastano
