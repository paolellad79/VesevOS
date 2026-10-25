// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cmdtab.h
// LA TABELLA UNICA DEI COMANDI della shell: per ogni comando il nome ufficiale (inglese), i vecchi nomi nascosti (alias), la sintassi,
// la descrizione breve, le parole che il Tab propone dopo il comando. La usano: help, help <comando>, man <comando>, il completamento col
// Tab e il cambio di nome degli alias (aereo -> airplane, locale -> region...). I testi lunghi di man stanno in vos_shell_man.cpp.
// I RUOLI non stanno qui: chi puo usare cosa lo decide solo vos_shell_roles.cpp (la tabella dei ruoli); qui lo si chiede, mai copiato.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "vos_shell_line.h"
#include "../drivers/vos_drv_print.h"

// Segna un testo italiano che si traduce DOPO, quando lo si stampa (una tabella statica non puo chiamare tr() all'avvio).
// Lo scanner delle traduzioni (tools/mklang.py) lo cerca come cerca tr.
#define TRK(s) s

namespace shx {

enum : uint8_t {
  CMD_HIDDEN = 1,        // nome valido ma nascosto: non compare in help e nel Tab (varianti e vecchi nomi che le regole usano ancora)
  CMD_ALIAS_ROW = 2,     // alias VISIBILE (compare in help e nel Tab): il campo alias contiene il nome ufficiale a cui rimanda (vesevos-led_control -> led)
};

struct CmdInfo {
  const char* name;      // nome ufficiale (minuscolo)
  const char* alias;     // altri nomi che fanno la stessa cosa, separati da spazi ("" = nessuno): canonicalName li cambia in name
                         // (con CMD_ALIAS_ROW: il nome ufficiale a cui questa riga rimanda)
  const char* usage;     // sintassi, per "help <comando>" e "man"
  const char* brief;     // descrizione breve (chiave tr, si traduce quando si stampa)
  const char* subs;      // parole che il Tab propone dopo il comando, separate da spazi; "@commands" = i nomi dei comandi; "" = niente
  uint8_t     flags;     // CMD_*
};

size_t         cmdCount();
const CmdInfo& cmdAt(size_t i);                 // tabella in ordine alfabetico del nome ufficiale
const CmdInfo* cmdFind(const char* name);       // per nome ufficiale o per alias; nullptr se non esiste

// Nome ufficiale di un comando: i vecchi nomi (aereo, licenza, users, watchdog, locale, restart, logout, ?) diventano quelli ufficiali.
// Dopo questa chiamata il resto della shell conosce solo i nomi ufficiali.
void canonicalName(Word& cmd);

// help: i comandi che il ruolo role puo usare, in ordine alfabetico, con la descrizione breve (vos_shell_man.cpp)
void cmdHelpList(OutSink& o, int role);
// help <comando> (full = false) e man <comando> (full = true). Ritorna false se il comando non esiste o il ruolo non lo puo usare
// (stessa risposta nei due casi: l'aiuto non rivela i comandi che il ruolo non vede).
bool cmdHelpOne(OutSink& o, const char* name, int role, bool full);
// "Uso: <sintassi>" di un comando della tabella
void cmdUsage(OutSink& o, const char* name);

}  // namespace shx
