// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_csrf.h
// Difesa contro le richieste partite da un altro sito (CSRF) per tutte le richieste che modificano (POST e simili).
// Il cookie ha gia SameSite=Strict; questo controllo e la seconda difesa e non dipende dal server web, cosi si prova sul computer.
#pragma once

namespace webx {
// La richiesta viene da un altro sito? true = va rifiutata (403). Gli argomenti sono i valori delle intestazioni (nullptr o "" se mancano).
//   Sec-Fetch-Site (browser moderni): ammessi solo "same-origin" (la nostra pagina) e "none" (azione diretta dell'utente).
//   Altrimenti Origin (tutti i browser lo mandano nei POST): deve avere lo stesso indirizzo dell'intestazione Host; "null" no.
//   Nessuna delle due: non e un browser (aggiorna.py, curl, altri programmi): ammessa, perche il CSRF si fa solo con un browser.
bool csrfCrossSite(const char* secFetchSite, const char* origin, const char* host);
}  // namespace webx
