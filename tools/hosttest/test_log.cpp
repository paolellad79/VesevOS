// VesevOS - prove sul computer: registro (vos_log.cpp) - livelli [E][W][I][D]
#include <Arduino.h>
#include "vos_log.h"
#include "mini.h"

static bool has(const std::string& all, const char* s) { return all.find(s) != std::string::npos; }

int main() {
  logInit(); logSetLevel(LG_DBG); logClear();
  vlog("ERRORE: prova");
  vlog("ATTENZIONE: prova");
  vlog("AUDIT: ROSSO rosso");
  vlog("AUDIT: GIALLO giallo");
  vlog("NET: tutto bene");
  vlogl(LG_DBG, "TOP: dettaglio");
  std::string g = logGet(20);
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
  // limiti: registro vuoto, richiesta di 0 righe, piu righe del registro, registro pieno (150 righe lunghe)
  logClear();
  CHECK(logGet(20).find("registro svuotato") != std::string::npos);   // logClear scrive una riga
  CHECK(logGet(0).empty());
  for (int i = 0; i < 400; i++) vlog("NET: riga lunga %d %0100d", i, i);   // oltre la capacita del registro (150 righe)
  g = logGet(150);
  int righe = 0; for (char ch : g) if (ch == '\n') righe++;
  CHECK(righe == 150);
  CHECK(g.size() <= 150 * 120);                                       // mai piu di LOG_LINES * LOG_LEN
  CHECK(g.back() == '\n');
  CHECK(has(g, "riga lunga 399"));                                    // l'ultima riga c'e
  CHECK(!has(g, "riga lunga 249 "));                                  // le piu vecchie sono state scartate
  CHECK(logGet(1000).size() == g.size());                             // chiedere troppe righe non cambia
  CHECK(g.capacity() >= g.size());
  return done("log");
}
