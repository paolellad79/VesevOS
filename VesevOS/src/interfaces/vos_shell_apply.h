// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_apply.h
// Cio che si fa quando la password c'e (scritta in riga, arrivata dalla pagina o risposta alla domanda della seriale): un solo punto per ognuno.
// Sta qui da solo, cosi vos_shell_ask.cpp non deve includere tutta vos_shell_int.h (che riporterebbe un ciclo tra i moduli).
#pragma once
#include <stdint.h>
#include "../drivers/vos_drv_print.h"
#include "vos_shell_secret.h"

namespace shx {
void   shPasswdApply(const char* user, const char* pass, OutSink& o);                  // vos_shell_admin.cpp: passwd
void   shUserAddApply(const char* name, int role, const char* pass, OutSink& o);        // vos_shell_admin.cpp: user add
void   shWifiSetApply(const char* ssid, const char* pass, OutSink& o);                  // vos_shell_net.cpp  : wifi set (pass vuota = rete aperta)
void   shWarnPassInLine(OutSink& o);                                                    // vos_shell_admin.cpp: avviso, la password e scritta nella riga
void   shBackupSecret(SecretFor what, const char* name, const char* pass, OutSink& o);  // vos_shell_backup.cpp: la frase del backup e arrivata (create / restore)
}  // namespace shx
