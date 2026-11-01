// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_cmd.h
// COMANDI di un driver (.vdb): due sezioni, entrambe facoltative insieme.
//   [commands]  una riga per sottocomando:  <comando> <sotto> <chi> [testo]
//               comando = la parola che si scrive nella shell (uguale in tutte le righe del driver); sotto = il sottocomando;
//               chi = admin | oper (ruolo MINIMO di chi lo lancia); "testo" = dopo la prima parola, tutto il resto della riga e UN solo argomento (puo contenere spazi).
//               Esempio:  sd write admin testo
//   [cmd]       codice Berry con  def on_cmd(sub, args) ... end  (massimo 4 KB). Gira solo quando si lancia un comando.
//   [widget]    (facoltativa, solo con i comandi) il riquadro del driver nella Home: title= alt= who=
//               title = titolo; alt = testo alternativo dell'icona quando lo stato non e ok (icona sbarrata); who = oper (predefinito) oppure admin.
//               L'icona e quella di [icon]. I valori vivi li stampa la funzione  def on_widget()  del codice [cmd], una riga per valore:
//                 st=ok|off   (ok = icona normale, off = icona sbarrata e dati grigi)    v=riga principale (40)    s=riga secondaria (80)
//               on_widget() gira SENZA diritti di scrittura, sempre; il firmware la chiama al massimo ogni 10 secondi e mai se Berry e occupato.
// Il firmware non conosce nessun comando ne nessun widget di periferica: li legge da qui (packages/vos_drvcmd.cpp e vos_drvwidget.cpp).
#pragma once
#include "vos_mod.h"
#include "vos_mod_line.h"
struct ModCmdEntry { char driver[25]; ModCmd c; };

int  modCmdList(ModCmdEntry* out, int mx);                                // tutti i comandi dei driver in /devs (righe non valide saltate); ritorna quanti ne ha messi
bool modCmdCode(const char* driver, HeapBuf& code, char* use, size_t useCap, ErrBuf& err);   // il codice di [cmd] e i permessi use= del driver (file controllato per intero)

struct ModWidgetEntry { char driver[25]; ModWidget w; char icon[MOD_ICON_MAX + 16]; char source[13]; };   // source: sorgente del firmware (driver .vde con source=), vuota = si chiama il driver   // grande (~2,2 KB): sempre sull'heap
int  modWidgetList(ModWidgetEntry* out, int mx);                           // i driver in /devs che hanno [widget] (file controllati per intero); ritorna quanti ne ha messi
