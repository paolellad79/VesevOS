// VesevOS - prove sul computer: WPS (net/vos_wps.cpp) con driver, rete, LED e audit finti e orologio finto
#include <Arduino.h>
#include "mini.h"
#include "vos_config.h"
#include "vos_net.h"
#include "vos_led.h"
#include "vos_audit.h"
#include "vos_drv_wifi.h"
#include "vos_drv_wps.h"
static unsigned long g_now = 100000;
#define millis() (g_now)

// ---- finti pezzi ----
static NetState g_ns = NET_AP;
NetState netState() { return g_ns; }
static int g_reconf = 0; void netReconfigure() { g_reconf++; }
static DrvWifiMode g_mode = DW_AP; static int g_disc = 0;
DrvWifiMode drvWifiMode() { return g_mode; }
void drvWifiSetMode(DrvWifiMode m) { g_mode = m; }
void drvWifiDisconnect(bool, bool) { g_disc++; }
static bool g_avail = true, g_startOk = true, g_wpsOn = false, g_pinReq = false; static DrvWpsEvent g_ev = DWE_NONE; static String g_pinVal = "", g_cs = "", g_cp = "";
bool drvWpsAvailable() { return g_avail; }
bool drvWpsStart(bool pin, DrvWpsErr& err) { err = DWERR_NONE; if (!g_startOk) { err = DWERR_START; return false; } g_wpsOn = true; g_pinReq = pin; g_ev = DWE_NONE; return true; }
void drvWpsStop() { g_wpsOn = false; }
DrvWpsEvent drvWpsPoll() { DrvWpsEvent e = g_ev; g_ev = DWE_NONE; return e; }
String drvWpsPin() { return g_pinVal; }
bool drvWpsCredentials(String& s, String& p) { s = g_cs; p = g_cp; return s.length() > 0; }
static int g_led = 0; void ledSetWps(int s) { g_led = s; }
static int g_aud = 0; void auditEvent(AuditLevel, const String&, const String&) { g_aud++; }
#include "../../VesevOS/src/net/vos_wps.cpp"
static int clicks2() { return 2; }    // il tasto BOOT arriva dal gancio (core/vos_hooks.h), come sulla scheda

static void tick() { wpsTick(); }

int main() {
  g_hooks.btnWpsClicks = clicks2;
  String err;
  // permessi
  cfg.setupDone = false; cfg.wpsOn = false; cfg.wpsPin = false;
  CHECK(wpsAllowed());                       // prima configurazione: tutto consentito
  cfg.setupDone = true;
  CHECK(!wpsAllowed());                     // scheda configurata: WPS spento di fabbrica
  CHECK(!wpsStart(false, "prova", err) && err.length());
  CHECK(wpsSetOn(true, err) && cfg.wpsOn && wpsAllowed());
  CHECK(!wpsStart(true, "prova", err));                            // PIN non consentito
  g_ns = NET_AIR; CHECK(!wpsStart(false, "prova", err)); g_ns = NET_CLIENT_TRY; CHECK(!wpsStart(false, "prova", err));
  g_avail = false; g_ns = NET_AP; CHECK(!wpsStart(false, "prova", err)); g_avail = true;

  // pulsante: parte, aspetta, riesce
  CHECK(wpsStart(false, "web 1.2.3.4", err) && wpsActive() && wpsState() == WPS_WAIT);
  CHECK(!wpsStart(false, "altro", err));                           // gia in corso
  tick();
  CHECK(g_wpsOn && !g_pinReq && g_mode == DW_AP_STA && g_led == 1);
  CHECK(wpsJson().indexOf("\"state\":1") > 0 && wpsJson().indexOf("\"pin\":\"") < 0);
  g_now += 30000; tick(); CHECK(wpsState() == WPS_WAIT);
  g_cs = "CasaMia"; g_cp = "segretissima"; g_ev = DWE_OK; cfg.staDhcp = false; cfg.staEnabled = false; g_reconf = 0; g_aud = 0;
  tick();
  CHECK(wpsState() == WPS_OK && !g_wpsOn && cfg.staSsid == "CasaMia" && cfg.staPass == "segretissima" && cfg.staEnabled && cfg.staDhcp);
  CHECK(g_reconf == 1 && g_aud == 1 && g_led == 2 && !wpsActive());
  CHECK(wpsJson().indexOf("CasaMia") > 0 && wpsJson().indexOf("segretissima") < 0);   // la password non esce mai
  g_now += 3500; tick(); CHECK(g_led == 0);

  // tempo scaduto (evento del chip) e tempo scaduto da soli: torna solo hotspot
  CHECK(wpsStart(false, "tasto BOOT", err)); tick(); g_mode = DW_AP_STA;
  g_ev = DWE_TIMEOUT; tick();
  CHECK(wpsState() == WPS_FAIL && g_mode == DW_AP && g_led == 3 && !g_wpsOn && wpsJson().indexOf("err") > 0);
  g_now += 4000; tick(); CHECK(g_led == 0);
  CHECK(wpsStart(false, "web", err)); tick(); g_now += 126000; tick();
  CHECK(wpsState() == WPS_FAIL && !g_wpsOn);
  // rifiuto del router
  CHECK(wpsStart(false, "web", err)); tick(); g_ev = DWE_FAILED; tick(); CHECK(wpsState() == WPS_FAIL);
  // il driver non parte
  g_startOk = false; CHECK(wpsStart(false, "web", err)); tick(); CHECK(wpsState() == WPS_FAIL && !wpsActive()); g_startOk = true;
  // dati non validi dal router
  CHECK(wpsStart(false, "web", err)); tick(); g_cs = "Casa"; g_cp = "corta"; g_ev = DWE_OK; cfg.staSsid = "vecchia"; tick();
  CHECK(wpsState() == WPS_FAIL && cfg.staSsid == "vecchia");
  CHECK(wpsStart(false, "web", err)); tick(); g_cs = ""; g_ev = DWE_OK; tick(); CHECK(wpsState() == WPS_FAIL && cfg.staSsid == "vecchia");

  // annullamento (anche di una richiesta non ancora iniziata)
  CHECK(wpsStart(false, "web", err)); tick(); wpsStop(); tick();
  CHECK(wpsState() == WPS_IDLE && !g_wpsOn && !wpsActive() && g_led == 0);
  CHECK(wpsStart(false, "web", err)); wpsStop(); tick(); CHECK(wpsState() == WPS_IDLE && !g_wpsOn && g_mode == DW_AP);
  wpsStop(); tick(); CHECK(wpsState() == WPS_IDLE);                // stop senza prova: niente

  // PIN: solo se consentito; il PIN esce solo mentre aspetta e non finisce mai nel registro
  cfg.wpsPin = true; g_pinVal = "";
  CHECK(wpsStart(true, "web", err)); tick(); CHECK(g_wpsOn && g_pinReq && wpsJson().indexOf("\"pin\":\"") < 0);
  g_pinVal = "12345670"; tick(); CHECK(wpsJson().indexOf("\"pin\":\"12345670\"") > 0 && wpsText().indexOf("12345670") >= 0);
  g_ev = DWE_TIMEOUT; tick(); CHECK(wpsJson().indexOf("12345670") < 0 && wpsText().indexOf("12345670") < 0);

  // dalla rete di casa: scollega, prova, e se fallisce rimette la rete di prima
  g_ns = NET_CLIENT_OK; g_mode = DW_STA; g_disc = 0; g_reconf = 0;
  CHECK(wpsStart(false, "web", err)); tick(); CHECK(g_disc == 1 && g_mode == DW_STA && g_wpsOn);
  g_ev = DWE_TIMEOUT; tick(); CHECK(wpsState() == WPS_FAIL && g_reconf == 1);
  // il task di rete deve fare altro: ferma senza ripristinare
  g_reconf = 0; CHECK(wpsStart(false, "web", err)); tick(); wpsAbort();
  CHECK(!wpsActive() && !g_wpsOn && g_reconf == 0 && wpsState() == WPS_IDLE);
  // interruttore spento durante la prova: la ferma
  g_ns = NET_AP; g_mode = DW_AP; CHECK(wpsStart(false, "web", err)); tick(); CHECK(wpsSetOn(false, err)); tick();
  CHECK(!cfg.wpsOn && !g_wpsOn && wpsState() == WPS_IDLE);
  // servizio nel registro
  CHECK(wpsText().length() > 0);
  return done("WPS");
}
