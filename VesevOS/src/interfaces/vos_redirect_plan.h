// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_redirect_plan.h
// Regole del redirect da HTTP a HTTPS, senza dipendere dal server web: le usano il server completo (Psychic, vos_web_redirect.cpp)
// e il server minimo (vos_http_redirect_srv.cpp), cosi le risposte sono sempre le stesse.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace webx {
// Cosa rispondere a una richiesta sulla porta HTTP quando il redirect e attivo. Ritorna il codice HTTP:
//   307 = vai a url (costruito dalle impostazioni della scheda e dalla porta HTTPS, MAI dall'intestazione Host)
//   405 = metodo non ammesso (solo GET e HEAD), 400 = percorso non valido, 414 = indirizzo troppo lungo: in *text il motivo (tradotto)
// methodOk = il metodo e GET o HEAD; path e query senza il '?' (il percorso vuoto vale "/").
int  redirectPlan(bool methodOk, const char* path, const char* query, char* url, size_t cap, const char** text);

// Quale server tiene la porta HTTP? true = quello MINIMO (solo redirect), false = quello completo (pagina, API, portale).
// Il minimo serve solo se il redirect e acceso, HTTP e HTTPS sono accesi e HTTPS e partito, e l'hotspot non puo accendersi da solo.
bool httpMinimalWanted();
}  // namespace webx
