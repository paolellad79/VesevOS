// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wps.cpp
#include "vos_wps.h"
#include "../core/vos_hooks.h"
#include "vos_net.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_audit.h"
#include "../core/vos_strbuf.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_service.h"
#include "../devices/vos_led.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_wps.h"
#include "../drivers/vos_drv_time.h"

#define WPS_WINDOW_MS   120000UL     // tempo concesso al router (standard WPS: 2 minuti)
#define WPS_GRACE_MS      5000UL     // margine prima di dichiarare il tempo scaduto da soli
#define WPS_LED_MS        3000UL     // quanto resta acceso il verde (riuscito) o il rosso (errore)

static volatile int  g_req = 0;                    // 0 niente, 1 avvia a pulsante, 2 avvia con PIN, 3 annulla
static volatile int  g_st = WPS_IDLE;
static bool          g_pinMode = false;
static bool          g_began = false;              // la sessione e davvero partita (il Wi-Fi e stato cambiato)
static bool          g_prevClient = false;         // prima della prova la scheda era collegata alla Wi-Fi di casa
static DrvWifiMode   g_prevMode = DW_OFF;
static uint32_t      g_until = 0, g_ledOff = 0;
static ErrBuf        g_err;                        // perche e fallita l'ultima prova
static char          g_pin[9], g_ssid[33], g_who[32];   // PIN (solo metodo PIN), rete ricevuta, chi ha chiesto la prova

bool wpsAllowed() { return cfg.wpsOn || !cfg.setupDone; }
bool wpsPinAllowed() { return cfg.wpsPin; }
int  wpsState() { return g_st; }
bool wpsActive() { return g_req != 0 || g_st == WPS_WAIT; }

bool wpsStart(bool pin, const char* who, ErrBuf& err) {
  if (!drvWpsAvailable()) { err = tr("WPS non presente in questa versione del firmware"); return false; }
  if (!wpsAllowed()) { err = tr("WPS spento: accendilo prima"); return false; }
  if (pin && !cfg.wpsPin) { err = tr("Il metodo PIN non e consentito: attivalo nelle impostazioni WPS"); return false; }
  NetState ns = netState();
  if (ns != NET_AP && ns != NET_CLIENT_OK) { err = tr("Il WPS parte solo con il Wi-Fi in funzione (non in modo aereo, non durante un collegamento)"); return false; }
  if (wpsActive()) { err = tr("WPS gia in corso"); return false; }
  snprintf(g_who, sizeof g_who, "%s", who ? who : "");
  g_err.clear(); g_pin[0] = 0; g_ssid[0] = 0;
  g_pinMode = pin;
  g_st = WPS_WAIT;                       // la pagina lo vede subito
  g_until = drvMillis() + WPS_WINDOW_MS + WPS_GRACE_MS;
  g_req = pin ? 2 : 1;
  return true;
}

void wpsStop() { if (g_st == WPS_WAIT || g_req) g_req = 3; }

// Fine della prova. restore = true: rimette il Wi-Fi come prima (altrimenti ci pensa chi ha fermato la prova).
static void finish(int st, const char* err, bool restore) {
  drvWpsStop();
  g_req = 0;
  g_pin[0] = 0;
  if (restore && g_began) {
    if (g_prevClient) netReconfigure();                       // torna alla rete di casa di prima
    else if (g_prevMode == DW_AP || g_prevMode == DW_AP_STA) { drvWifiDisconnect(false, false); drvWifiSetMode(DW_AP); }   // torna solo hotspot
  }
  g_began = false;
  g_err = err;
  g_st = st;
  if (st == WPS_OK) ledSetWps(2); else if (st == WPS_FAIL) ledSetWps(3); else ledSetWps(0);
  g_ledOff = drvMillis() + WPS_LED_MS;
}

static void beginSession() {
  g_began = true;
  g_prevMode = drvWifiMode();
  g_prevClient = (netState() == NET_CLIENT_OK);
  if (g_prevClient) { drvWifiDisconnect(false, false); drvWifiSetMode(DW_STA); }
  else drvWifiSetMode(DW_AP_STA);                    // l'hotspot resta acceso
  DrvWpsErr de;
  if (!drvWpsStart(g_pinMode, de)) { finish(WPS_FAIL, de == DWERR_ABSENT ? tr("WPS non presente in questa versione del firmware") : tr("Il WPS non parte (radio spenta o gia in uso)"), true); return; }
  ledSetWps(1);
  vlog("WPS: avviato (%s) da %s", g_pinMode ? "PIN" : "pulsante", g_who);   // il PIN non va mai nel registro
}

static void onSuccess() {
  char ssid[33] = "", pass[65] = "";
  size_t pl = 0;
  if (drvWpsCredentials(ssid, sizeof ssid, pass, sizeof pass)) pl = strlen(pass);
  if (!ssid[0] || (pl && (pl < 8 || pl > 64))) {
    finish(WPS_FAIL, tr("Il router non ha mandato dati validi"), true);
    return;
  }
  drvWpsStop();
  cfgSetOriginf("WPS %s", g_who);
  cfg.staSsid = ssid; cfg.staPass = pass; cfg.staEnabled = true;
  cfg.staDhcp = true;                                 // e una rete nuova: l'indirizzo fisso di prima non vale piu
  cfgSave();
  auditEvent(AUD_YELLOW, "wps", tr("La Wi-Fi di casa e stata cambiata con il WPS"));
  vlog("WPS: riuscito, rete '%s' salvata", ssid);
  snprintf(g_ssid, sizeof g_ssid, "%s", ssid);
  finish(WPS_OK, "", false);
  netReconfigure();                                   // collegamento alla rete nuova
}

void wpsAbort() {
  if (g_st == WPS_WAIT || g_req) { vlog("WPS: interrotto"); finish(WPS_IDLE, "", false); }
}

void wpsTick() {
  uint32_t now = drvMillis();
  int rq = g_req; g_req = 0;
  if (rq == 3) { if (g_st == WPS_WAIT) { vlog("WPS: annullato"); finish(WPS_IDLE, "", true); } g_began = false; return; }
  if (rq == 1 || rq == 2) beginSession();
  if (g_st == WPS_WAIT) {
    DrvWpsEvent ev = drvWpsPoll();
    if (g_pinMode && !g_pin[0]) drvWpsPin(g_pin, sizeof g_pin);
    if (ev == DWE_OK) onSuccess();
    else if (ev == DWE_FAILED) finish(WPS_FAIL, tr("Il router non ha accettato (due router in WPS insieme? riprova)"), true);
    else if (ev == DWE_TIMEOUT || (int32_t)(now - g_until) >= 0) finish(WPS_FAIL, tr("Tempo scaduto: il router non ha risposto (il WPS puo essere spento sul router)"), true);
  } else if ((g_st == WPS_OK || g_st == WPS_FAIL) && g_ledOff && (int32_t)(now - g_ledOff) >= 0) { ledSetWps(0); g_ledOff = 0; }
}

bool wpsSetOn(bool on, ErrBuf& err) {
  if (on && !drvWpsAvailable()) { err = tr("WPS non presente in questa versione del firmware"); return false; }
  if (!on) wpsStop();
  cfg.wpsOn = on; cfgSetOrigin("WPS"); cfgSave();
  return true;
}

bool wpsJson(HeapBuf& j) {
  int left = 0;
  if (g_st == WPS_WAIT) { int32_t l = (int32_t)(g_until - drvMillis()) / 1000 - (int)(WPS_GRACE_MS / 1000); left = l < 0 ? 0 : (l > 120 ? 120 : (int)l); }
  j.addf("{\"avail\":%s,\"on\":%s,\"allowed\":%s,\"pinOk\":%s,\"btn\":%d,\"setup\":%s,\"net\":%d,\"state\":%d,\"method\":\"%s\",\"left\":%d",
         drvWpsAvailable() ? "true" : "false", cfg.wpsOn ? "true" : "false", wpsAllowed() ? "true" : "false", cfg.wpsPin ? "true" : "false",
         hookBtnWpsClicks(), cfg.setupDone ? "false" : "true", (int)netState(), (int)g_st, g_pinMode ? "pin" : "pbc", left);
  if (g_st == WPS_WAIT && g_pinMode && g_pin[0]) j.addf(",\"pin\":\"%s\"", g_pin);
  if (g_st == WPS_FAIL) { j.add(",\"err\":\""); j.addJson(g_err.c_str()); j.add('"'); }
  if (g_st == WPS_OK) { j.add(",\"ssid\":\""); j.addJson(g_ssid); j.add('"'); }
  return j.add('}');
}

// Al massimo ~700 caratteri (la riga dell'errore arriva a 160): 1 KB bastano
std::string wpsText() {
  StrText t(1024);
  StrBuf& b = t.buf();
  b.addf(tr("WPS: %s"), cfg.wpsOn ? tr("acceso") : (cfg.setupDone ? tr("spento") : tr("acceso (prima configurazione)"))).add('\n');
  b.addf(tr("Metodo PIN: %s"), cfg.wpsPin ? tr("consentito") : tr("non consentito")).add('\n');
  int bc = hookBtnWpsClicks();
  if (bc) b.addf(tr("Tasto BOOT: %d click avviano il WPS"), bc);
  else b.add(tr("Tasto BOOT: nessun gesto avvia il WPS"));
  b.add('\n');
  if (!drvWpsAvailable()) b.add(tr("WPS non presente in questa versione del firmware")).add('\n');
  if (g_st == WPS_WAIT) {
    b.addf(tr("In corso (%s): premi il tasto WPS del router."), g_pinMode ? tr("PIN") : tr("pulsante")).add('\n');
    if (g_pinMode) {
      if (g_pin[0]) b.addf(tr("PIN da scrivere nel router: %s"), g_pin);
      else b.add(tr("PIN in arrivo..."));
      b.add('\n');
    }
  } else if (g_st == WPS_OK) b.addf(tr("Ultima prova: riuscita, rete '%s'"), g_ssid).add('\n');
  else if (g_st == WPS_FAIL) b.addf(tr("Ultima prova: non riuscita - %s"), g_err.c_str()).add('\n');
  return t.take();
}

// ---- servizio nel registro: l'interruttore dice se il WPS e consentito (la prova a mano parte dai pulsanti) ----
static int  svcState() { return cfg.wpsOn ? SVC_ON : SVC_OFF; }
static bool svcBegin(ErrBuf& err) { return wpsSetOn(true, err); }
static void svcStop() { ErrBuf e; wpsSetOn(false, e); }
static const ServiceOps WPS_OPS = { "wps", "WPS", "Rete", "wps", ROLE_ADMIN, ROLE_ADMIN,
                                    svcState, svcBegin, svcStop, nullptr, wpsJson, nullptr, nullptr };
void wpsInit() { serviceRegister(&WPS_OPS); }
