// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_int.h
// Parti interne delle traduzioni condivise tra vos_i18n.cpp (ricerca) e vos_i18n_elenco.cpp (elenco delle lingue).
#pragma once
#include <stddef.h>
#include "vos_i18n_calc.h"
#include "../drivers/vos_drv_file.h"

#define LANG_DIR "/lang"

void langPath(const char* code, const char* ext, char* out, size_t cap);     // "/lang/<code>.<ext>"
bool langLeggiIntest(VosFile& f, LangVld& d);                                // legge e controlla l'intestazione del .vld aperto

// Stato "lingua in errore" (vos_i18n_errore.cpp). Il motivo e un LangGuasto di vos_i18n_salute_rule.h.
void langErroreImposta(const char* code, int guasto);   // mette in errore (una sola volta per episodio); mai per "it"; ignorato durante una sostituzione
void langErroreAzzera();                                 // langSet riuscito (anche "it") o file nuovo valido
void langSostituzione(bool inCorso);                     // un caricamento sta sostituendo il file in uso: niente guasti da segnalare
