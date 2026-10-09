// VesevOS - prove sul computer: comandi di prima configurazione dal telefono (vos_ble_cfg.cpp), senza radio
#include <Arduino.h>
#include "vos_config.h"
#include "vos_ble_cfg.h"
#include "vos_region.h"
#include "vos_net.h"
#include "vos_time.h"
#include "mini.h"

// finti pezzi del nucleo: contano le chiamate
static int g_reconf = 0, g_apply = 0;
void netReconfigure() { g_reconf++; }
String netIpString() { return "192.168.1.50"; }
bool regionValid(const String& c) { return c == "IT" || c == "DE"; }
void timeApply() { g_apply++; }

int main() {
  bool stop = false; String r;
  cfgDefaults(); cfg.hostname = "vesevos2"; cfg.country = "IT";
  r = bleCfgRun("status", stop);
  CHECK(r == "vesevos2 192.168.1.50 IT" && !stop);
  r = bleCfgRun("  status  ", stop);                                  // spazi attorno
  CHECK(r == "vesevos2 192.168.1.50 IT");
  cfg.country = "";
  CHECK(bleCfgRun("status", stop) == "vesevos2 192.168.1.50 --");
  // done: risposta ok e richiesta di spegnere
  r = bleCfgRun("done", stop); CHECK(r == "ok" && stop);
  // wifi
  g_reconf = 0;
  r = bleCfgRun("wifi MiaRete|password1", stop);
  CHECK(r == "ok" && cfg.staSsid == "MiaRete" && cfg.staPass == "password1" && cfg.staEnabled && cfg.staDhcp && g_reconf == 1 && !stop);
  r = bleCfgRun("wifi Aperta", stop);                                 // rete senza password
  CHECK(r == "ok" && cfg.staSsid == "Aperta" && cfg.staPass == "");
  g_reconf = 0; cfg.staSsid = "prima";
  r = bleCfgRun("wifi X|corta", stop);                                // password sotto 8 caratteri
  CHECK(r != "ok" && cfg.staSsid == "prima" && g_reconf == 0);
  r = bleCfgRun("wifi |password1", stop);                             // nome vuoto
  CHECK(r != "ok" && cfg.staSsid == "prima");
  r = bleCfgRun("wifi 123456789012345678901234567890123|password1", stop);   // nome di 33 caratteri
  CHECK(r != "ok" && cfg.staSsid == "prima");
  // country
  r = bleCfgRun("country it", stop); CHECK(r == "ok" && cfg.country == "IT");
  r = bleCfgRun("country ZZ", stop); CHECK(r != "ok" && cfg.country == "IT");
  // name
  r = bleCfgRun("name Casa1", stop); CHECK(r == "ok" && cfg.hostname == "casa1");
  r = bleCfgRun("name nome non valido", stop); CHECK(r != "ok" && cfg.hostname == "casa1");
  // mesh
  String k = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  r = bleCfgRun("mesh 1 " + k, stop); CHECK(r == "ok" && cfg.meshRole == 1 && cfg.meshKey == k);
  r = bleCfgRun("mesh 3 " + k, stop); CHECK(r != "ok" && cfg.meshRole == 1);
  r = bleCfgRun("mesh 2 abc", stop); CHECK(r != "ok" && cfg.meshRole == 1);
  // sconosciuto
  r = bleCfgRun("reboot", stop); CHECK(r.indexOf("sconosciuto") >= 0 && !stop);
  // il comando che non riesce non spegne
  r = bleCfgRun("", stop); CHECK(r.indexOf("sconosciuto") >= 0 && !stop);
  return done("blecfg");
}
