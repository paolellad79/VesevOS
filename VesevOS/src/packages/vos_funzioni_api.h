// Elenco delle funzioni VesevOS della scheda: nome, gruppo, ruolo minimo, descrizione.
// Serve alla rotta GET /api/script/api (il prompt per le AI della pagina Script).
// ATTENZIONE: la registrazione vera sta nei file vos_funzioni*.cpp (be_regfunc).
// Quando si aggiunge o si toglie una funzione, aggiornare anche questa tabella.
#pragma once

struct InfoFunzione {
  const char* nome;        // nome della funzione nelle funzioni VesevOS
  const char* gruppo;      // gruppo di permessi (log, sd, gpio, ...)
  const char* ruolo;       // "operatore" oppure "admin"
  const char* descrizione; // descrizione breve, in italiano
};

extern const InfoFunzione kElencoFunzioni[];
extern const int kNElencoFunzioni;

// Tetto del JSON della rotta /api/script/api (variante compatta: nome, gruppo, ruolo).
#define FUNZIONI_API_MAX 4096

class HeapBuf;

// Scrive in o il JSON compatto dell'elenco. False se non entra o manca memoria: la rotta risponde ERRORE.
bool funzioniApiJson(HeapBuf& o);
