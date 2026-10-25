// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_benchsha.h
// Misura diagnostica dell'SHA-256 (hardware o software) e della formula della password. Comando "benchsha" (solo Admin).
#pragma once
#include "../drivers/vos_drv_print.h"

// Esegue le misure e scrive il rapporto su o. Dura qualche secondo: cede il processore tra un blocco e l'altro (watchdog).
// Non cambia nessuna impostazione e non usa password vere (sale e password sono valori di prova fissi).
void benchShaRun(OutSink& o);
