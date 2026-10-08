// VesevOS - prove sul computer: impostazioni della seriale (vos_serial.cpp)
#include <Arduino.h>
#include "vos_config.h"
#include "vos_serial.h"
#include "mini.h"
#include <unistd.h>

int main() {
  cfgDefaults();
  String e;
  CHECK(serialSet("eol", "lf", e) && cfg.serEol == 1);
  CHECK(serialSet("eol", "cr", e) && cfg.serEol == 2);
  CHECK(serialSet("eol", "crlf", e) && cfg.serEol == 0);
  CHECK(!serialSet("eol", "boh", e) && e.length() > 0 && cfg.serEol == 0);
  CHECK(serialSet("echo", "off", e) && !cfg.serEcho);
  CHECK(serialSet("echo", "on", e) && cfg.serEcho);
  CHECK(serialSet("input", "off", e) && !cfg.serIn);
  CHECK(serialSet("input", "1", e) && cfg.serIn);
  CHECK(serialSet("log", "off", e) && !cfg.serLogOut);
  CHECK(serialSet("banner", "off", e) && !cfg.serBanner);
  CHECK(serialSet("tx", "0", e) && cfg.serTx == 0);
  CHECK(serialSet("tx", "200", e) && cfg.serTx == 200);
  CHECK(!serialSet("tx", "201", e) && cfg.serTx == 200);
  CHECK(!serialSet("tx", "abc", e));
  CHECK(!serialSet("tx", "-1", e));
  CHECK(!serialSet("input", "", e) && cfg.serIn);                // valore mancante: non spegne niente
  CHECK(!serialSet("input", "no", e) && cfg.serIn);
  CHECK(!serialSet("echo", "false", e) && cfg.serEcho);
  CHECK(!serialSet("colore", "rosso", e));                       // impostazione che non esiste
  // velocita: solo quelle della lista; la nuova e "in prova" finche non si conferma
  CHECK(!serialSet("baud", "1234", e) && cfg.serBaud == 115200);
  CHECK(serialSet("baud", "9600", e));
  CHECK(serialJson().indexOf("\"trial\":true") >= 0);
  CHECK(cfg.serBaud == 115200);                                   // la prova non entra nella configurazione
  CHECK(serialSet("baud", "19200", e) && cfg.serBaud == 115200);  // seconda prova durante la prima
  CHECK(serialKeep() && cfg.serBaud == 19200);
  CHECK(serialJson().indexOf("\"trial\":false") >= 0);
  CHECK(!serialKeep());                                           // niente da confermare
  CHECK(serialText().indexOf("19200") >= 0);
  // PC che non legge: la prima scrittura in ritardo fa buttare via l'uscita per 2 secondi, senza altre attese
  cfgDefaults();
  unsigned b0 = Serial.calls;
  serOut().print("ciao\n");
  CHECK(Serial.calls == b0 + 1 && serStallCount() == 0);
  Serial.failWrite = true;
  serOut().print("ciao\n");
  CHECK(Serial.calls == b0 + 2 && serStallCount() == 1);
  unsigned c0 = Serial.calls;
  serOut().print("ancora\n"); serOut().print("e ancora\n");
  CHECK(Serial.calls == c0 && serStallCount() == 1);              // buttata via: nessuna scrittura tentata
  usleep(2100 * 1000);
  Serial.failWrite = false;
  serOut().print("di nuovo\n");
  CHECK(Serial.calls == c0 + 1 && serStallCount() == 1);          // passati i 2 secondi: si riprova e passa
  return done("serial");
}
