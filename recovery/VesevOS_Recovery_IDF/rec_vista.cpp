// VesevOS Recovery - tema e messaggio della pagina (vedi rec_vista.h).
#include "rec_vista.h"
#include "rec_config.h"
#include "rec_vista_regole.h"
#include <stdio.h>

void recTemaLeggi(const char* percorso) {
  rc.tema.clear();
  FILE* f = fopen(percorso, "r");
  if (!f) return;                                    // nessun tema: colori di fabbrica
  std::string t;
  char buf[512];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
    t.append(buf, n);
    if (t.size() > REC_TEMA_MAX) break;              // piu grande del tetto: inutile leggere oltre
  }
  fclose(f);
  RecTemaEsito e = recTemaControlla(t);
  if (e == RT_OK) {
    rc.tema = t;
    printf("[recovery] tema della pagina letto (%u byte)\n", (unsigned)t.size());
  } else if (e != RT_VUOTO) {
    printf("[recovery] tema della pagina rifiutato (%s): colori di fabbrica\n", recTemaEsitoNome(e));
  }
}

std::string recVistaJson() {
  std::string nome = rc.read ? rc.host : "";
  return "{\"ok\":true,\"nm\":\"" + recJsonTesto(nome) + "\",\"ml\":\"" + recJsonTesto(rc.msgLogin) + "\"}";
}
