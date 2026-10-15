// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crash.h
// Rapporto dei crash SENZA partizione di core dump (la tabella con il recovery non ha spazio).
// Al crash (errore grave, watchdog dei task, watchdog degli interrupt) il gestore del core Arduino copia nella memoria RTC
// motivo, core, task, indirizzo e backtrace (16 indirizzi). All'avvio dopo si scrive nel log e in /crash.txt (ultimi 5, file di sistema).
// Gli indirizzi si traducono in file e riga con il .elf della stessa versione:  xtensa-esp32s3-elf-addr2line -pfiaC -e VesevOS.ino.elf <indirizzi>
#pragma once
#include <Arduino.h>

void   crashEarly();      // primissima cosa nel setup: legge (e azzera) il rapporto lasciato nella memoria RTC
void   crashInstall();    // installa il gestore dei crash
void   crashBoot();       // dopo file system e log: scrive il rapporto del crash precedente (se c'e)
String crashText();       // rapporti salvati (dal piu recente)
bool   crashClear();
bool   crashPending();    // c'e un crash non ancora visto dall'ultimo avvio (per la pagina)
