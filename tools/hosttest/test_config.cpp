// VesevOS - prove sul computer: configurazione (vos_config.cpp) - salvataggio e lettura del testo
#include <Arduino.h>
#include "vos_config.h"
#include "mini.h"

int main() {
  cfgDefaults();
  CHECK(cfg.serBaud == 115200 && cfg.serEol == 0 && cfg.serEcho && cfg.serIn && cfg.serLogOut && cfg.serBanner && cfg.serTx == 10);
  CHECK(cfg.logLevel == 2);
  // cambio qualche valore, esporto, riparto da zero, importo: devono tornare uguali
  cfg.serBaud = 921600; cfg.serEol = 2; cfg.serEcho = false; cfg.serIn = false; cfg.serLogOut = false; cfg.serBanner = false; cfg.serTx = 77;
  cfg.logLevel = 3; cfg.hostname = "prova1"; cfg.cpuMhz = 80;
  String t = cfgExport(true);
  CHECK(t.indexOf("serbaud") >= 0);
  cfgDefaults();
  String err;
  CHECK(cfgImport(t, err));
  CHECK(cfg.serBaud == 921600 && cfg.serEol == 2 && !cfg.serEcho && !cfg.serIn && !cfg.serLogOut && !cfg.serBanner && cfg.serTx == 77);
  CHECK(cfg.logLevel == 3 && cfg.hostname == "prova1" && cfg.cpuMhz == 80);
  CHECK(cfgExport(true) == t);                                    // niente cambia a ogni giro
  // valori fuori misura: si tengono quelli di fabbrica o si limitano
  cfgDefaults();
  CHECK(cfgImport("config system 'system'\n\toption serbaud '5'\n\toption sereol '9'\n\toption sertx '999'\n\toption loglv '8'\n", err));
  CHECK(cfg.serBaud == 115200);                                   // 5 baud non ha senso
  CHECK(cfg.serEol == 2 && cfg.serTx == 200 && cfg.logLevel == 3);   // limitati al massimo
  // password mai nel testo pubblico
  cfgDefaults(); cfg.mqttPass = "segretissima";
  CHECK(cfgExport(false).indexOf("segretissima") < 0);
  // F2: NTP spento dalla guida si riaccende quando arriva il Wi-Fi; se lo ha spento l'utente resta spento
  cfgDefaults();
  CHECK(!cfgNtpAfterWifi() && cfg.ntpOn);                          // di fabbrica: niente da fare
  cfg.ntpOn = false; cfg.ntpAutoOff = true;                        // come dopo la guida senza Wi-Fi
  String t2 = cfgExport(true);
  cfgDefaults(); CHECK(cfgImport(t2, err));
  CHECK(!cfg.ntpOn && cfg.ntpAutoOff);                             // il segno si salva e si rilegge
  CHECK(cfgNtpAfterWifi() && cfg.ntpOn && !cfg.ntpAutoOff);        // arriva il Wi-Fi: NTP acceso
  CHECK(!cfgNtpAfterWifi());                                       // solo una volta
  cfgDefaults(); cfg.ntpOn = false; cfg.ntpAutoOff = false;        // spento dall'utente
  CHECK(!cfgNtpAfterWifi() && !cfg.ntpOn);                         // resta spento
  // secondo server NTP: di fabbrica nessuno (niente Google), va e torna nella configurazione
  cfgDefaults();
  CHECK(cfg.ntpServer == "pool.ntp.org" && cfg.ntpServer2 == "");
  CHECK(cfgExport(true).indexOf("server2") >= 0);
  cfg.ntpServer2 = "ntp.example.org";
  String ex = cfgExport(true); cfgDefaults(); String er;
  CHECK(cfgImport(ex, er) && cfg.ntpServer2 == "ntp.example.org");
  return done("config");
}
