// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_roles.h
// Chi puo usare quale comando: UNA tabella (comando, sottocomando, ruolo minimo) in vos_shell_roles.cpp.
// La usano shellExecSess (esegue o rifiuta) e il completamento col Tab (nasconde cio che il ruolo non puo usare).
#pragma once
#include "vos_shell_line.h"

namespace shx {
// Ruolo minimo (ROLE_GUEST / ROLE_OPER / ROLE_ADMIN) per usare il comando cmd con il sottocomando sub (parola vuota = nessuno).
// Un comando che non e in tabella e solo per l'Admin: dimenticare una riga non apre mai niente.
int  roleMin(const Word& cmd, const Word& sub);

// Il ruolo role (0 Ospite, 1 Operatore, 2 Admin) puo usare il comando cmd con il sottocomando sub?
bool roleAllows(int role, const Word& cmd, const Word& sub);
}  // namespace shx
