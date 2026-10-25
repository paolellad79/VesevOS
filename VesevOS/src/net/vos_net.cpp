// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net.cpp
#include "vos_net.h"
#include "../drivers/vos_drv_task.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_i18n.h"
#include "../core/vos_config.h"
#include "../core/vos_util.h"
#include "../core/vos_log.h"
#include "../core/vos_cpuboost.h"
#include "../devices/vos_led.h"
#include "../core/vos_hooks.h"
#include "../core/vos_region.h"
#include "../core/vos_fw.h"
#include "../core/vos_wd.h"
#include "vos_wps.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_mdns.h"
#include "vos_dns.h"
#include "../core/vos_service.h"
#include "../core/vos_strbuf.h"
#include <esp_netif.h>
#include <time.h>
#include "../drivers/vos_drv_time.h"

static volatile NetState g_state = NET_BOOT;
static volatile bool g_reconf = true;
static volatile bool g_staFail = false;   // l'ultimo tentativo verso la Wi-Fi di casa e fallito
static volatile bool g_scanReq = false;
static volatile bool g_scanRun = false;
static HeapBuf g_scanJson(NET_SCAN_JSON_MAX);   // elenco gia in JSON; lo riscrive solo doScan (vuoto = "[]")
static bool g_scanMore = false;               // lista troncata (regola API 13)
#define SCAN_MAX 40                           // reti massime nell'elenco

NetState netState() { return g_state; }
void netReconfigure() { g_reconf = true; }
void netScanStart() { g_scanReq = true; }

static void setState(NetState s) {
  bool chg = (g_state != s);
  g_state = s; ledSetNetState(s);
  if (chg) EventBus::publish("net.state", "", (int)s);     // evento: la rete ha cambiato stato (valore = NetState)
}
static volatile int g_apCh = 0;            // 0 = canale di partenza della rete schede
static volatile bool g_apChReq = false;
void netSetApChannel(int ch) { if (regionChannelOk(ch)) { g_apCh = ch; g_apChReq = true; } }
bool netOnAp(uint32_t ip) { return (ip & 0xFFFFFF00UL) == 0xC0A80400UL; }

// ---- portale automatico (captive portal): in modo AP ogni nome DNS punta alla scheda ----
static void dnsStart() {
  if (dnsCaptiveRunning()) return;
  static const uint8_t apIp[4] = { 192, 168, 4, 1 };
  if (dnsCaptiveStart(apIp)) vlog("NET: portale automatico attivo");
}
static void dnsStop() { dnsCaptiveStop(); }
bool netCaptive() { return dnsCaptiveRunning(); }

// ---- servizi dell'hotspot e nome in rete: acceso/spento da Servizi ----
static volatile bool g_svcReq = false;
static bool g_mdnsRun = false;
void netApplyServices() { g_svcReq = true; }

static esp_netif_t* apNetif() { return esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"); }
static bool dhcpRunning() {                          // il server DHCP dell'hotspot sta rispondendo?
  esp_netif_t* ap = apNetif();
  esp_netif_dhcp_status_t st = ESP_NETIF_DHCP_INIT;
  return ap && esp_netif_dhcps_get_status(ap, &st) == ESP_OK && st == ESP_NETIF_DHCP_STARTED;
}
// Il DHCP serve solo all'hotspot. Se l'hotspot c'e per forza (nessuna Wi-Fi di casa) deve rispondere sempre, altrimenti il telefono non riceve l'indirizzo.
static bool dhcpWanted() { return cfg.dhcpOn || !cfg.apOn; }
static void dhcpApply() {                            // solo con l'hotspot acceso
  esp_netif_t* ap = apNetif();
  if (!ap) return;
  esp_netif_dhcps_stop(ap);
  if (!dhcpWanted()) { vlog("NET: server DHCP dell'hotspot spento"); return; }
  uint32_t lease = cfg.dhcpLease;                    // minuti
  esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease, sizeof(lease));
  esp_netif_dhcps_start(ap);
}
static void mdnsStop() { drvMdnsEnd(); g_mdnsRun = false; }   // PRIMA di spegnere o rifare la radio: mDNS tiene i puntatori alle interfacce di rete (DEF-18)
static void mdnsApply() {
  mdnsStop();
  if (!cfg.mdnsOn) { vlog("NET: mDNS spento"); return; }
  if (drvMdnsBegin(cfg.hostname.c_str())) {
    g_mdnsRun = true;
    if (cfg.httpOn) drvMdnsAddService("http", "tcp", cfg.httpPort);
    if (cfg.https) drvMdnsAddService("https", "tcp", cfg.httpsPort);
  }
}

// ---- modalita aereo ----
static volatile bool g_airReq = false, g_airOffReq = false;
bool netAirplane() { return cfg.airOn; }

bool netAirplaneOn(int ex, uint32_t param, ErrBuf& err) {
  if (ex < 0 || ex > 3) { err = tr("Modo di uscita non valido"); return false; }
  if (ex == 1 && (param < 10 || param > 7UL * 86400UL)) { err = tr("Tempo da 10 secondi a 7 giorni"); return false; }
  if (ex == 2 && param > 1439) { err = tr("Orario non valido"); return false; }
  cfg.airExit = ex; cfg.airAt = (ex == 2) ? param : 0;
  time_t now = time(NULL);
  cfg.airUntil = (ex == 1) ? (now > 1700000000 ? (uint32_t)now + param : (uint32_t)(drvMillis() / 1000) + param) : 0;
  cfg.airOn = 1;
  cfgSave();
  g_airReq = true;
  static const char* const EX[] = {"al prossimo avvio", "dopo un tempo", "a un orario", "solo a mano"};
  vlog("NET: modalita aereo ATTIVA (riattivazione: %s)", EX[ex]);
  return true;
}

void netAirplaneOff(const char* why) {
  if (!cfg.airOn) return;
  cfg.airOn = 0; cfgSave();
  g_airOffReq = true;
  vlog("NET: modalita aereo spenta (%s)", why);
}

std::string netAirplaneText() {
  if (!cfg.airOn) return tr("Modalita aereo: spenta");
  char w[64];
  if (cfg.airExit == 0) snprintf(w, sizeof(w), "%s", tr("al prossimo avvio"));
  else if (cfg.airExit == 1) {
    time_t now = time(NULL);
    long left = (cfg.airUntil > 1700000000UL) ? (long)cfg.airUntil - (long)now : (long)cfg.airUntil - (long)(drvMillis() / 1000);
    snprintf(w, sizeof(w), tr("tra %ld s"), left < 0 ? 0L : left);
  } else if (cfg.airExit == 2) {
    char b[8]; snprintf(b, sizeof(b), "%02u:%02u", cfg.airAt / 60, cfg.airAt % 60);
    snprintf(w, sizeof(w), tr("alle %s"), b);
  }
  else snprintf(w, sizeof(w), "%s", tr("solo a mano"));
  StrText t(256);
  t.buf().addf(tr("Modalita aereo: ATTIVA (si riattiva %s; sempre: 'airplane off' dalla seriale o tasto BOOT)"), w);
  return t.take();
}

// controllo ogni secondo: e ora di riaccendere?
static void airCheck() {
  if (!cfg.airOn) return;
  time_t now = time(NULL);
  if (cfg.airExit == 1) {
    bool due = (cfg.airUntil > 1700000000UL) ? ((uint32_t)now >= cfg.airUntil) : ((uint32_t)(drvMillis() / 1000) >= cfg.airUntil);
    if (due) netAirplaneOff("tempo scaduto");
  } else if (cfg.airExit == 2 && now > 1700000000) {
    struct tm t; localtime_r(&now, &t);
    if ((uint16_t)(t.tm_hour * 60 + t.tm_min) == cfg.airAt) netAirplaneOff("orario raggiunto");
  }
}

static void airEnter() {
  wpsAbort();
  hookCall(g_hooks.radiosOff);        // ESP-NOW e Bluetooth
  dnsStop();
  mdnsStop();
  drvWifiDisconnect(true, false);
  drvWifiSetMode(DW_OFF);
  setState(NET_AIR);
}

void netIpTo(char* out, size_t n) {
  ipTo(g_state == NET_AP ? drvWifiApIp() : drvWifiLocalIp(), out, n);   // AP anche durante la scansione (modo AP+STA)
}

// Aggiunge ,"chiave":numero  /  ,"chiave":true|false  /  ,"chiave":"testo" (testo senza caratteri da proteggere o gia protetto).
static void jNum(HeapBuf& o, const char* key, long v) {
  char t[40]; snprintf(t, sizeof(t), ",\"%s\":%ld", key, v); o.add(t);
}
static void jBool(HeapBuf& o, const char* key, bool v) {
  o.add(",\""); o.add(key); o.add(v ? "\":true" : "\":false");
}
static void jStr(HeapBuf& o, const char* key, const char* v) {
  o.add(",\""); o.add(key); o.add("\":\""); o.addJson(v, strlen(v)); o.add('"');
}

bool netStatusJson(HeapBuf& j) {
  const char* mode = (g_state == NET_AIR) ? tr("Modo aereo") : (g_state == NET_AP) ? "AP" : (g_state == NET_CLIENT_OK) ? "Client"
                   : (g_state == NET_CLIENT_TRY) ? tr("Connessione...") : tr("Avvio");
  char ipb[16], mac1[20], mac2[20];
  netIpTo(ipb, sizeof ipb); netMacTo(false, mac1, sizeof mac1); netMacTo(true, mac2, sizeof mac2);
  j.add("{\"mode\":\""); j.addJson(mode, strlen(mode)); j.add('"');
  jStr(j, "ip", ipb);
  jStr(j, "host", cfg.hostname.c_str());
  j.add(",\"fqdn\":\""); j.addJson(cfg.hostname.c_str(), cfg.hostname.length());
  if (cfg.domain.length()) { j.add('.'); j.addJson(cfg.domain.c_str(), cfg.domain.length()); }
  j.add('"');
  jNum(j, "st", (int)g_state);                 // 0 avvio, 1 AP, 2 provo client, 3 client ok
  jStr(j, "apSsid", cfg.apSsid.c_str());
  // Valori IP reali (testo vuoto se non disponibili)
  char gw[16] = "", mk[16] = "", d1[16] = "", d2[16] = "";
  if (g_state == NET_CLIENT_OK) {
    jStr(j, "ssid", drvWifiSsid().s);
    jNum(j, "rssi", drvWifiRssi());
    if (drvWifiGateway()) ipTo(drvWifiGateway(), gw, sizeof gw);
    if (drvWifiMask())    ipTo(drvWifiMask(), mk, sizeof mk);
    if (drvWifiDns(0))    ipTo(drvWifiDns(0), d1, sizeof d1);
    if (drvWifiDns(1))    ipTo(drvWifiDns(1), d2, sizeof d2);
  } else {
    jStr(j, "ssid", cfg.apSsid.c_str());
    if (g_state == NET_AP) { ipTo(drvWifiApIp(), gw, sizeof gw); snprintf(mk, sizeof mk, "255.255.255.0"); }
  }
  jStr(j, "gw", gw); jStr(j, "mask", mk); jStr(j, "dns", d1); jStr(j, "dns2", d2);
  jNum(j, "ch", (int)drvWifiChannel());
  if (g_state == NET_AP) jNum(j, "clients", (int)drvWifiApClients());
  jBool(j, "staFail", g_staFail && cfg.staEnabled);
  jBool(j, "apOn", cfg.apOn);
  jBool(j, "dhcpRun", g_state == NET_AP && dhcpRunning());
  jBool(j, "mdnsRun", g_mdnsRun);
  jBool(j, "captive", dnsCaptiveRunning());
  jBool(j, "air", cfg.airOn);
  jNum(j, "airExit", cfg.airExit);
  jNum(j, "airAt", cfg.airAt);
  jStr(j, "mac", mac1); jStr(j, "apMac", mac2);
  return j.add('}');
}

// MAC letto dalla eFuse: funziona anche con il Wi-Fi spento o in un altro modo.
void netMacTo(bool ap, char* out, size_t n) { drvWifiMac(ap, out, n); }

std::string netInfoText() {
  StrText t(640);
  StrBuf& b = t.buf();
  const char* modo = g_state == NET_AP ? "Access Point" : g_state == NET_CLIENT_OK ? "Client" : "-";
  b.addf(tr("Modo:        %s"), modo).add('\n');
  b.addf(tr("Nome host:   %s"), cfg.hostname.c_str()).add('\n');
  char ipb[16], mac1[20], mac2[20];
  netIpTo(ipb, sizeof ipb); netMacTo(false, mac1, sizeof mac1); netMacTo(true, mac2, sizeof mac2);
  b.addf(tr("IP:          %s"), ipb).add('\n');
  if (g_state == NET_CLIENT_OK) {
    b.addf(tr("Rete:        %s (%d dBm, canale %d)"), drvWifiSsid().s, drvWifiRssi(), drvWifiChannel()).add('\n');
    b.addf(tr("Gateway:     %s"), IpText(drvWifiGateway()).s).add('\n');
  }
  b.addf(tr("MAC client:  %s"), mac1).add('\n');
  b.addf(tr("MAC AP:      %s"), mac2).add('\n');
  return t.take();
}

bool netScanRunning() { return g_scanRun; }

bool netScanJson(HeapBuf& out) {
  out.add("{\"running\":"); out.add(g_scanRun ? "true" : "false");
  out.add(",\"more\":"); out.add(g_scanMore ? "true" : "false");
  out.add(",\"list\":"); out.add(g_scanRun || !g_scanJson.length() ? "[]" : g_scanJson.c_str());
  return out.add('}');
}

// L'hotspot parte se il servizio e acceso, oppure se non c'e nessuna Wi-Fi di casa (altrimenti la scheda sarebbe irraggiungibile).
static bool apAllowed() { return cfg.apOn || !cfg.staEnabled || !cfg.staSsid.length(); }

static void startAp() {
  mdnsStop();                                         // la radio puo spegnersi qui sotto: mDNS va fermato prima
  drvWifiDisconnect(true, false);
  drvWifiSetMode(DW_AP);
  regionApplyRadio();                                 // paese e potenza PRIMA di trasmettere
  int ch = g_apCh ? g_apCh : cfg.meshCh;
  if (!regionChannelOk(ch)) ch = 1;
  drvWifiApStart(cfg.apSsid.c_str(), cfg.apPass.c_str(), ch, 0xC0A80401, 0xC0A80401, 0xFFFFFF00);   // 192.168.4.1 / 255.255.255.0   // sempre con password (WPA2), unica per ogni scheda
  setState(NET_AP);
  vlog("NET: AP '%s' su 192.168.4.1 (canale %d)", cfg.apSsid.c_str(), ch);
  if (!dhcpWanted()) dhcpApply(); else if (cfg.dhcpLease != 120) dhcpApply();   // di fabbrica: DHCP gia acceso, 120 minuti
  if (cfg.apCaptive && cfg.httpOn && cfg.httpPort == 80) dnsStart();   // il portale automatico serve solo con HTTP sulla porta 80
}

static bool applyStaticIp() {
  if (cfg.staDhcp) return true;
  uint32_t ip, mk, gw, d1 = 0, d2 = 0;
  if (!ipParse(cfg.ip.view(), ip) || !ipParse(cfg.mask.view(), mk) || !maskValid(mk) || !ipParse(cfg.gw.view(), gw)) {
    vlog("NET: IP statico non valido, uso DHCP");
    return false;
  }
  ipParse(cfg.dns1.view(), d1); ipParse(cfg.dns2.view(), d2);
  drvWifiStaConfigStatic(ip, gw, mk, d1, d2);
  return true;
}

static bool tryClient() {
  dnsStop();
  mdnsStop();                                         // il cambio di rete spegne la radio e distrugge le interfacce: mDNS va fermato prima
  setState(NET_CLIENT_TRY);
  drvWifiDisconnect(true, false);
  drvDelayMs(200);
  drvWifiSetMode(DW_STA);
  regionApplyRadio();                                 // paese e potenza PRIMA di trasmettere
  drvWifiSetHostname(cfg.hostname.c_str());
  if (!applyStaticIp()) drvWifiStaConfigDhcp();       // torna a DHCP
  drvWifiStaBegin(nullptr, cfg.staSsid.c_str(), cfg.staPass.c_str());
  for (int i = 0; i < 40; i++) {            // max 20 s
    if (drvWifiStaStatus() == DWS_CONNECTED) {
      setState(NET_CLIENT_OK); g_staFail = false;
      vlog("NET: client connesso, IP %s", IpText(drvWifiLocalIp()).s);
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  g_staFail = true;
  return false;
}

static void doScan() {
  g_scanRun = true;
  cpuBoost("scan", 8);                           // la scansione Wi-Fi e piu veloce con la CPU al massimo
  DrvWifiMode m = drvWifiMode();
  if (m == DW_AP) drvWifiSetMode(DW_AP_STA);
  int n = drvWifiScanStart();                   // il paese impostato limita i canali cercati
  g_scanJson.clear();
  g_scanJson.add('[');
  g_scanMore = n > SCAN_MAX;
  if (n > SCAN_MAX) n = SCAN_MAX;
  for (int i = 0; i < n; i++) {
    if (i) g_scanJson.add(',');
    DrvSsid ssid = drvWifiScanSsid(i);
    char t[48];
    g_scanJson.add("{\"ssid\":\""); g_scanJson.addJson(ssid.s, strlen(ssid.s));
    snprintf(t, sizeof(t), "\",\"rssi\":%d,\"ch\":%d,\"open\":%s}", drvWifiScanRssi(i), (int)drvWifiScanChannel(i), drvWifiScanOpen(i) ? "true" : "false");
    g_scanJson.add(t);
  }
  g_scanJson.add(']');
  drvWifiScanClear();
  if (m == DW_AP) drvWifiSetMode(DW_AP);
  g_scanRun = false;
}

// ---- prova del Wi-Fi di casa dalla guida: l'hotspot resta acceso, niente viene salvato ----
static volatile bool g_tReq = false;
static volatile int g_tState = 0;          // 0 niente, 1 in corso, 2 collegata, 3 non riuscita
static char g_tSsid[33], g_tPass[65], g_tIp[16];
static ErrBuf g_tErr;
void netTestStart(const char* ssid, const char* pass) {
  if (g_tState == 1) return;
  snprintf(g_tSsid, sizeof(g_tSsid), "%s", ssid);
  snprintf(g_tPass, sizeof(g_tPass), "%s", pass);
  g_tIp[0] = 0; g_tErr.clear(); g_tState = 1; g_tReq = true;
}
bool netTestJson(HeapBuf& j) {
  char t[24]; snprintf(t, sizeof(t), "{\"state\":%d", (int)g_tState);
  j.add(t);
  if (g_tState == 2) { j.add(",\"ip\":\""); j.addJson(g_tIp, strlen(g_tIp)); j.add('"'); }
  if (g_tState == 3) { j.add(",\"err\":\""); j.addJson(g_tErr.c_str(), g_tErr.length()); j.add('"'); }
  return j.add('}');
}
static void doTest() {
  if (g_state != NET_AP) { g_tErr = tr("Prova possibile solo con l'hotspot acceso"); g_tState = 3; return; }
  drvWifiSetMode(DW_AP_STA);
  drvWifiStaBegin(cfg.hostname.c_str(), g_tSsid, g_tPass);
  DrvWifiSta st = DWS_IDLE;
  for (int i = 0; i < 40; i++) {                       // max 20 s
    st = drvWifiStaStatus();
    if (st == DWS_CONNECTED) break;
    vTaskDelay(pdMS_TO_TICKS(500));
    wdBeat("net");
  }
  if (st == DWS_CONNECTED) { ipTo(drvWifiLocalIp(), g_tIp, sizeof g_tIp); g_tState = 2; vlog("NET: prova Wi-Fi di casa riuscita"); }
  else {
    g_tErr = (st == DWS_NO_SSID) ? tr("Rete non trovata (la scheda usa solo 2,4 GHz)") : tr("Non collegata: controlla la password della rete");
    g_tState = 3; vlog("NET: prova Wi-Fi di casa fallita");
  }
  drvWifiDisconnect(false, false);
  drvWifiSetMode(DW_AP);
}

static void netTask(void*) {
  int fails = 0;
  if (cfg.airOn && cfg.airExit == 0) netAirplaneOff("nuovo avvio");   // "al prossimo avvio"
  wdWatch("net", wdLimit("net"));                    // base 180 s: tryClient puo durare 20 s, la scansione qualche secondo
  for (;;) {
    wdBeat("net");
    fwTick();
    if (g_apChReq) {                                 // cambio canale dell'hotspot senza spegnere il Wi-Fi (la rete schede resta accesa)
      g_apChReq = false;
      if (g_state == NET_AP) { drvWifiApRetune(cfg.apSsid.c_str(), cfg.apPass.c_str(), g_apCh); vlog("NET: hotspot sul canale %d", (int)g_apCh); }
    }
    if (g_airOffReq) { g_airOffReq = false; g_reconf = true; }
    if (g_airReq || (cfg.airOn && g_state != NET_AIR)) {
      g_airReq = false;
      if (cfg.airOn) { airEnter(); hookCall(g_hooks.webStart); }
    }
    if (cfg.airOn) { airCheck(); vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    if (g_reconf) {
      wpsAbort();                                    // una prova WPS in corso non regge un cambio di rete
      g_reconf = false; fails = 0;
      if (cfg.staEnabled && cfg.staSsid.length()) {
        if (!tryClient() && apAllowed()) startAp();     // se fallisce torna AP (se il servizio AP e acceso)
      } else startAp();
      mdnsApply();
      hookCall(g_hooks.webStart);                       // il server parte solo ora che la rete e pronta
    }
    if (g_svcReq) {                                  // DHCP, portale automatico e mDNS: si applicano subito, senza riavvio
      g_svcReq = false;
      if (g_state == NET_AP) {
        dhcpApply();
        if (cfg.apCaptive && cfg.httpOn && cfg.httpPort == 80) dnsStart(); else dnsStop();
      }
      mdnsApply();
    }
    wpsTick();                                       // WPS: avvio, attesa, esito (solo qui si tocca il Wi-Fi)
    if (wpsActive()) {                               // durante il WPS niente scansioni, prove o recuperi di rete
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (g_scanReq) { g_scanReq = false; doScan(); }
    if (g_tReq) { g_tReq = false; doTest(); }
    if (cfg.staEnabled && g_state == NET_CLIENT_OK && drvWifiStaStatus() != DWS_CONNECTED) {
      vlog("NET: connessione persa");
      setState(NET_CLIENT_TRY);
    }
    if (g_state == NET_CLIENT_TRY) {
      if (drvWifiStaStatus() == DWS_CONNECTED) setState(NET_CLIENT_OK);
      else if (++fails >= 60) {
        fails = 0;
        if (apAllowed()) { vlog("NET: troppi tentativi, torno in AP"); startAp(); }
        else { vlog("NET: Wi-Fi assente, il servizio AP e spento: riprovo"); tryClient(); }   // scelta dell'utente: recupero con BOOT 8 s
      }
    } else fails = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Wi-Fi come servizio (sola lettura nel registro): non si ferma da qui (si perderebbe la pagina); per spegnerlo c'e il modo aereo.
static int  svcState() { NetState n = netState(); return n == NET_CLIENT_OK || n == NET_AP ? SVC_ON : n == NET_AIR ? SVC_OFF : SVC_STARTING; }
static bool svcBegin(ErrBuf& err) { (void)err; netReconfigure(); return true; }
static void svcStop() {}
static const ServiceOps NET_OPS = { "wifi", "Wi-Fi", "Rete", "net", ROLE_OPER, ROLE_ADMIN,
                                    svcState, svcBegin, svcStop, nullptr, netStatusJson, nullptr, nullptr };

void netInit() {
  serviceRegister(&NET_OPS);
  wpsInit();
  g_reconf = true;
  if (!cfg.apOn && cfg.dhcpOn && cfg.staEnabled && cfg.staSsid.length()) {     // schede configurate prima della 1.7.18: hotspot spento ma DHCP rimasto acceso
    cfg.dhcpOn = false; cfgSetOrigin("DHCP segue hotspot"); cfgSave(); vlog("NET: hotspot spento, spengo anche il DHCP");
  }
  drvTaskCreate(netTask, "net", 6144, NULL, 2, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 0 fisso)
}
