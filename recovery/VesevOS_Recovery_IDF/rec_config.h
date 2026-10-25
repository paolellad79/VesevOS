// VesevOS Recovery - impostazioni lette (in sola lettura) dal file /vesevos.conf di VesevOS, nella partizione LittleFS "spiffs".
#pragma once
#include <string>

struct RecUser { std::string name, salt, hash; int role = 0; bool on = false; };

struct RecCfg {
  int ledPin = 48, ledBr = 80;                  // LED RGB integrato (WS2812), come in VesevOS
  std::string host = "vesevos", ssid, pass, ip, mask, gw, dns1, dns2, country;
  bool staOn = false, dhcp = true;
  RecUser users[8];
  int nUsers = 0;
  bool adminOk = false;                         // c'e almeno un amministratore valido
  bool read = false;                            // il file e stato letto davvero
};

extern RecCfg rc;

void loadConfig();                              // se qualcosa non va scrive il motivo sulla seriale e lascia i valori di fabbrica
int userFind(const std::string& name);          // -1 se non esiste

// Nome da usare in rete (DHCP e mDNS): esattamente il nome host di VesevOS (come la 0.4.4), solo lettere/cifre/trattino, al massimo 63 caratteri.
// Se la configurazione non e stata letta: "vesevos".
std::string recNetName();
