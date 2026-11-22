// VesevOS Recovery - aspetto della pagina letto dai file di VesevOS (solo lettura): tema /etc/tema.css e messaggio del login.
// Il tema si legge con loadConfig(), mentre la memoria dei file e montata. Se il file manca o non e valido restano i colori di fabbrica.
#pragma once
#include <string>

// Legge e controlla il tema (regole di rec_vista_regole.h). Se e valido lo mette in rc.tema, altrimenti lascia rc.tema vuoto e scrive il motivo sulla seriale.
void recTemaLeggi(const char* percorso);

// Corpo di GET /api/vista: {"ok":true,"nm":"<nome della scheda>","ml":"<messaggio>"} con tutti i testi protetti (vedi recJsonTesto).
std::string recVistaJson();
