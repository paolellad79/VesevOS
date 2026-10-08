// VesevOS - prove sul computer: registro (vos_log.cpp) - livelli [E][W][I][D]
#include <Arduino.h>
#include "vos_log.h"
#include "mini.h"

static bool has(const String& all, const char* s) { return all.indexOf(s) >= 0; }

int main() {
  logInit(); logSetLevel(LG_DBG); logClear();
  vlog("ERRORE: prova");
  vlog("ATTENZIONE: prova");
  vlog("AUDIT: ROSSO rosso");
  vlog("AUDIT: GIALLO giallo");
  vlog("NET: tutto bene");
  vlogl(LG_DBG, "TOP: dettaglio");
  String g = logGet(20);
  CHECK(has(g, "[E] ERRORE: prova"));
  CHECK(has(g, "[W] ATTENZIONE: prova"));
  CHECK(has(g, "[E] AUDIT: ROSSO"));
  CHECK(has(g, "[W] AUDIT: GIALLO"));
  CHECK(has(g, "[I] NET: tutto bene"));
  CHECK(has(g, "[D] TOP: dettaglio"));
  // un testo scelto dall'utente a meta riga NON alza il livello (nome scheda, rete Wi-Fi...)
  logClear();
  vlog("NET: collegato alla rete ERRORE ATTENZIONE AUDIT: ROSSO");
  vlog("CFG: nome scheda ATTENZIONE");
  g = logGet(20);
  CHECK(has(g, "[I] NET: collegato"));
  CHECK(has(g, "[I] CFG: nome scheda"));
  CHECK(!has(g, "[E]") && !has(g, "[W]"));
  // livello di registrazione
  logClear(); logSetLevel(LG_WARN);
  vlog("NET: info"); vlog("ATTENZIONE: x"); vlog("ERRORE: y");
  g = logGet(20);
  CHECK(!has(g, "NET: info") && has(g, "ATTENZIONE: x") && has(g, "ERRORE: y"));
  logSetLevel(LG_ERR); logClear();
  vlog("ATTENZIONE: x"); vlog("ERRORE: y");
  g = logGet(20);
  CHECK(!has(g, "ATTENZIONE") && has(g, "ERRORE: y"));
  // caratteri strani diventano '?'
  logSetLevel(LG_INFO); logClear();
  vlog("NET: a\x01" "b");
  CHECK(has(logGet(5), "a?b"));
  return done("log");
}
