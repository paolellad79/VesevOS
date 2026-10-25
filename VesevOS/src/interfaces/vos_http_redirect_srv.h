// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_http_redirect_srv.h
// Server HTTP MINIMO: un task con un socket che sulla porta HTTP risponde solo il redirect a HTTPS (stesse regole del server completo,
// vedi vos_redirect_plan.h). Niente rotte, niente pagina, niente cookie, niente dati. Si usa al posto del server completo solo quando
// il redirect e acceso e l'hotspot non puo accendersi (httpMinimalWanted): risparmia la RAM di un secondo server web.
#pragma once
#include <stdint.h>

bool httpRedirectSrvStart(uint16_t port);   // false se la porta o la memoria non bastano: il chiamante torna al server completo
