// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_line.h
// Una riga di [commands] e una riga di [widget] di un driver (.vdb), e i valori che il codice del driver dice al widget.
// Pezzo piccolo e senza dipendenze: lo usano il controllo del file (vos_mod.cpp) e la lettura dei comandi (vos_mod_cmd.cpp).
// Formato dei comandi: vedi vos_mod_cmd.h. Formato del widget: vedi vos_mod_cmd.h (sezione [widget]).
#pragma once
#include <string_view>

#define MOD_CMD_MAX 12          // righe di [commands] per driver

struct ModCmd {
  char cmd[13];                 // la parola del comando (1-12 caratteri: a-z, 0-9, -, _; comincia con una lettera)
  char sub[13];                 // il sottocomando (1-12 caratteri: a-z, 0-9, -, _)
  bool admin;                   // true = serve l'Admin, false = basta l'Operatore
  bool rest;                    // true = l'ultimo argomento e il resto della riga
};

bool modCmdLine(std::string_view line, ModCmd& out, const char** why);   // controlla una riga di [commands]; why = motivo gia tradotto

// ---- widget della Home definito dal driver ----
#define MODW_ALT_MAX 48         // lunghezza massima del testo alternativo
struct ModWidget {
  bool on;                      // il driver ha la sezione [widget]
  char title[33];               // titolo del riquadro (se manca: il titolo del driver)
  char alt[MODW_ALT_MAX + 1];   // testo alternativo dell'icona quando lo stato non e "ok" (icona sbarrata)
  char who[6];                  // ruolo minimo che lo vede: "oper" (predefinito) oppure "admin"
};
#define MODW_V_MAX 40           // lunghezza massima della riga principale (v=)
#define MODW_S_MAX 80           // lunghezza massima della riga secondaria (s=)
// I valori vivi, prodotti dalla funzione on_widget() del driver (stampa righe  st=ok|off  v=testo  s=testo)
struct ModWidgetVals { char st[4]; char v[MODW_V_MAX + 1]; char s[MODW_S_MAX + 1]; };

bool modWidgetKey(std::string_view key, std::string_view val, ModWidget& w, const char** why);   // una riga chiave=valore di [widget]; why = motivo gia tradotto
void modWidgetParseOut(const char* out, ModWidgetVals& vals);   // legge l'uscita di on_widget(): righe sconosciute ignorate, testi tagliati, solo ASCII stampabile
