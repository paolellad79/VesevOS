// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_msglogin.h
// Messaggio personalizzabile della pagina di accesso: testo semplice scelto dall'Admin, visibile anche senza accesso.
// Regole: al massimo 200 caratteri, solo ASCII stampabile (32-126) e a capo, niente barra rovesciata.
// Nel file di configurazione l'a capo si scrive come due caratteri "\n" (la barra e vietata nel testo, quindi non c'e ambiguita).
#pragma once
#include <stddef.h>
#include <string_view>
#include "vos_errbuf.h"

#define MSGLOGIN_MAX 200   // caratteri del messaggio (a capo compresi)

// Pulisce (toglie i \r e gli spazi o gli a capo ai lati) e controlla il testo. Scrive in out (capacita almeno MSGLOGIN_MAX + 1).
// false = non valido: err dice perche e out resta vuoto.
bool msgLoginControlla(std::string_view in, char* out, size_t cap, ErrBuf& err);

// Per il file di configurazione: a capo <-> "\n" di due caratteri. out deve avere capacita almeno 2 * MSGLOGIN_MAX + 1.
void msgLoginCodifica(const char* in, char* out, size_t cap);
void msgLoginDecodifica(std::string_view in, char* out, size_t cap);
