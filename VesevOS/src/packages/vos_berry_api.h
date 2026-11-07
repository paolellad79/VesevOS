// Elenco delle funzioni Berry della scheda: nome, gruppo, ruolo minimo, descrizione.
// Serve alla rotta GET /api/script/api (il prompt per le AI della pagina Script).
// ATTENZIONE: la registrazione vera sta nei file vos_berry*.cpp (be_regfunc).
// Quando si aggiunge o si toglie una funzione, aggiornare anche questa tabella.
#pragma once

struct BerryFnInfo {
  const char* nome;        // nome della funzione in Berry
  const char* gruppo;      // gruppo di permessi (log, sd, gpio, ...)
  const char* ruolo;       // "operatore" oppure "admin"
  const char* descrizione; // descrizione breve, in italiano
};

extern const BerryFnInfo kFunzioniBerry[];
extern const int kNFunzioniBerry;

// Tetto del JSON della rotta /api/script/api (variante compatta: nome, gruppo, ruolo).
#define BERRY_API_MAX 4096

class HeapBuf;

// Scrive in o il JSON compatto dell'elenco. False se non entra o manca memoria: la rotta risponde ERRORE.
bool berryApiJson(HeapBuf& o);
