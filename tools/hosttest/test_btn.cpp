// VesevOS - prove sul computer: click del tasto BOOT (packages/vos_btn.cpp) con orologio e azioni finti
#include <Arduino.h>
#include "mini.h"
#include "vos_config.h"
#include "vos_net.h"
#include "vos_wps.h"
#include "vos_led.h"
static unsigned long g_now = 100000;
#define millis() (g_now)

static bool g_air = false; static int g_airOff = 0;
bool netAirplane() { return g_air; }
void netAirplaneOff(const char*) { g_airOff++; g_air = false; }
static bool g_wAct = false, g_wAllowed = true; static int g_wStart = 0, g_wStop = 0;
bool wpsActive() { return g_wAct; }
bool wpsAllowed() { return g_wAllowed; }
bool wpsStart(bool, const char*, String&) { g_wStart++; g_wAct = true; return true; }
void wpsStop() { g_wStop++; g_wAct = false; }
static int g_ident = 0; static uint32_t g_identMs = 0;
void ledIdentify(uint32_t ms) { g_ident++; g_identMs = ms; }
#include "../../VesevOS/src/packages/vos_btn.cpp"

static void click() { btnClick(); g_now += 100; }                 // un click: premi e lascia
static void wait(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { g_now += 50; btnTick(false); } }

int main() {
  cfg.btnAct[0] = BA_AIR_OFF; cfg.btnAct[1] = BA_WPS; cfg.btnAct[2] = BA_NONE;
  // 2 click = WPS (aspetta mezzo secondo, potrebbe essere il terzo)
  click(); wait(100); click(); CHECK(g_wStart == 0); wait(600); CHECK(g_wStart == 1);
  // 1 click: aspetta mezzo secondo (potrebbero essere 2)
  g_air = true; g_wAct = false; g_wStart = 0;
  click(); CHECK(g_airOff == 0); wait(300); CHECK(g_airOff == 0); wait(200); CHECK(g_airOff == 1);
  // 1 click fuori dal modo aereo: niente
  click(); wait(700); CHECK(g_airOff == 1);
  // 2 click con WPS in corso = annulla
  g_wAct = true; click(); wait(100); click(); wait(600); CHECK(g_wStop == 1 && !g_wAct);
  // WPS spento: non parte
  g_wAllowed = false; g_wStart = 0; click(); wait(100); click(); wait(600); CHECK(g_wStart == 0); g_wAllowed = true;
  // 3 click non mappati: ignorati (non diventano 2 + 1)
  g_air = true; g_airOff = 0;
  click(); wait(100); click(); wait(100); click(); wait(700); CHECK(g_airOff == 0 && g_wStart == 0);
  // 3 click mappati su "trova": parte subito
  CHECK(btnSetAction(3, BA_FIND, *new String()));
  g_ident = 0; click(); wait(100); click(); wait(100); click(); CHECK(g_identMs == 10000 && g_ident >= 1);
  // pressione lunga: i click in sospeso si perdono
  g_air = true; g_airOff = 0; cfg.btnAct[2] = BA_NONE; cfg.btnAct[1] = BA_WPS;
  click(); btnLongRelease(); wait(700); CHECK(g_airOff == 0);
  // tasto ancora premuto: il gesto non scatta
  click(); btnTick(true); g_now += 1000; btnTick(true); CHECK(g_airOff == 0); btnTick(false); CHECK(g_airOff == 1);
  // valori non validi
  String e; CHECK(!btnSetAction(0, 1, e) && !btnSetAction(4, 1, e) && !btnSetAction(1, 9, e) && !btnSetAction(1, -1, e));
  CHECK(btnWpsClicks() == 2); cfg.btnAct[1] = BA_NONE; CHECK(btnWpsClicks() == 0); cfg.btnAct[1] = BA_WPS;
  // nessuna azione mappata: nessun errore
  uint8_t sv[3] = {cfg.btnAct[0], cfg.btnAct[1], cfg.btnAct[2]}; cfg.btnAct[0] = cfg.btnAct[1] = cfg.btnAct[2] = 0;
  click(); wait(700); memcpy(cfg.btnAct, sv, 3);
  // json
  String j = btnJson(); CHECK(j.indexOf("\"act\":[1,2,") >= 0 && j.indexOf("\"wpsClicks\":2") >= 0 && j.indexOf("\"test\":0") >= 0);
  // modo prova: nessuna azione, solo l'annotazione
  g_air = true; g_airOff = 0; g_wStart = 0; g_wAct = false;
  btnTestSet(true); CHECK(btnTestActive() && btnJson().indexOf("\"test\":60") >= 0);
  click(); wait(100); click(); wait(600); CHECK(g_wStart == 0 && g_airOff == 0);
  CHECK(btnJson().indexOf("2 click") >= 0 && btnJson().indexOf("non eseguito") >= 0);
  btnTestLong(8200); CHECK(btnJson().indexOf("8 s") >= 0);
  btnTick(true); g_now += 1500; btnTick(true); CHECK(btnJson().indexOf("\"down\":true") >= 0 && btnJson().indexOf("\"heldMs\":1500") >= 0);
  btnTick(false);
  g_now += 61000; btnTick(false); CHECK(!btnTestActive());          // dopo 60 s torna normale
  click(); wait(700); CHECK(g_airOff == 1);
  btnTestSet(true); btnTestSet(false); CHECK(!btnTestActive());
  // click liberi: vanno al programma solo se c'e e se il gesto e "Libero"
  cfg.btnAct[0] = BA_AIR_OFF; cfg.btnAct[1] = BA_WPS; cfg.btnAct[2] = BA_NONE; g_wAct = false;
  click(); click(); click(); CHECK(btnScriptPop() == 0);                  // nessun programma: i click si perdono
  btnScriptAttach(true);
  click(); click(); click(); CHECK(btnScriptPop() == 3 && btnScriptPop() == 0);
  cfg.btnAct[0] = BA_NONE; click(); wait(700); CHECK(btnScriptPop() == 1);   // anche 1 click, se libero
  CHECK(btnScriptFree(1) && !btnScriptFree(2) && btnScriptFree(3));
  CHECK(btnJson().indexOf("\"free\":[true,false,true]") >= 0 && btnJson().indexOf("\"script\":true") >= 0);
  for (int i = 0; i < 6; i++) { click(); click(); click(); } CHECK(btnScriptPop() == 3 && btnScriptPop() == 3 && btnScriptPop() == 3 && btnScriptPop() == 0);   // coda da 3 (4-1): il resto si perde
  click(); click(); click(); btnScriptAttach(false); CHECK(btnScriptPop() == 0);   // fine programma: coda vuota
  btnScriptAttach(true); btnTestSet(true); click(); click(); click(); CHECK(btnScriptPop() == 0 && btnJson().indexOf("Libero") >= 0);   // in prova non arrivano
  btnTestSet(false); btnScriptAttach(false);
  return done("btn");
}
