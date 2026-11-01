// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvcmd.h
// Registro dei COMANDI dei driver (sezioni [commands] e [cmd] dei file /devs/*.vdb, vedi devices/vos_mod_cmd.h) e loro esecuzione in Berry.
// Il firmware non conosce nessun comando di periferica (esempio: "sd" e nel driver microSD): la shell lo chiede qui.
// Il registro si rilegge da /devs alla prima richiesta dopo ogni caricamento o eliminazione di un driver (drvCmdRefresh).
// Si esclude con VOS_WITH_DRVCMD (boards/vos_packages.h): senza, nessun driver porta comandi.
#pragma once
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "vos_berry.h"

struct DrvCmd {
  char driver[25];              // il nome del driver (file /devs/<driver>.vdb)
  char cmd[13];                 // la parola del comando
  char sub[13];                 // il sottocomando
  bool admin;                   // serve l'Admin
  bool rest;                    // l'ultimo argomento e il resto della riga
};

#if VOS_WITH_DRVCMD && VOS_WITH_BERRY
#define DRVCMD_MAX 24           // sottocomandi in tutto, di tutti i driver
void drvCmdRefresh();                                                  // un driver e stato caricato o tolto: alla prossima richiesta si rilegge
bool drvCmdHas(const char* cmd);                                       // c'e un driver con questa parola di comando?
bool drvCmdFind(const char* cmd, const char* sub, DrvCmd& out);       // il sottocomando c'e? (out = la sua dichiarazione)
int  drvCmdSubs(const char* cmd, DrvCmd* out, int mx);                 // tutti i sottocomandi di un comando, nell'ordine del driver
int  drvCmdWords(char (*out)[13], int mx);                             // le parole dei comandi dei driver, senza doppioni (per help e Tab)
// Esegue il comando. callerAdmin = chi lo lancia e Admin: le funzioni di scrittura (admin in use=) ci sono solo se il sottocomando e "admin" E chi lancia e Admin.
bool drvCmdRun(const DrvCmd& c, const CmdCall& call, bool callerAdmin, HeapBuf& out, ErrBuf& err);
#else
inline void drvCmdRefresh() {}
inline bool drvCmdHas(const char*) { return false; }
inline bool drvCmdFind(const char*, const char*, DrvCmd&) { return false; }
inline int  drvCmdSubs(const char*, DrvCmd*, int) { return 0; }
inline int  drvCmdWords(char (*)[13], int) { return 0; }
inline bool drvCmdRun(const DrvCmd&, const CmdCall&, bool, HeapBuf&, ErrBuf& err) { err = "Comandi dei driver non presenti in questo firmware"; return false; }
#endif
