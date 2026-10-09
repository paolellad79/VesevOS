// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net.cpp
#include "vos_net.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_i18n.h"
#include "../core/vos_config.h"
#include "../core/vos_util.h"
#include "../core/vos_log.h"
#include "../devices/vos_led.h"
#include "../interfaces/vos_web.h"
#include "../core/vos_region.h"
#include "../core/vos_fw.h"
#include "../core/vos_wd.h"
#include "../packages/vos_mesh.h"
#include "../packages/vos_ble.h"
#include "../drivers/vos_drv_wifi.h"
#include "../core/vos_service.h"
#include <ESPmDNS.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <DNSServer.h>
#include <time.h>

static volatile NetState g_state = NET_BOOT;
static volatile bool g_reconf = true;
static volatile bool g_staFail = false;   // l'ultimo tentativo verso la Wi-Fi di casa e fallito
static volatile bool g_scanReq = false;
static volatile bool g_scanRun = false;
static String g_scanJson = "[]";
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
static DNSServer* g_dns = nullptr;
static void dnsStart() {
  if (g_dns) return;
  g_dns = new DNSServer();
  g_dns->setErrorReplyCode(DNSReplyCode::NoError);
  if (g_dns->start(53, "*", IPAddress(192, 168, 4, 1))) vlog("NET: portale automatico attivo");
  else { delete g_dns; g_dns = nullptr; }
}
static void dnsStop() {
  if (!g_dns) return;
  g_dns->stop(); delete g_dns; g_dns = nullptr;
}
bool netCaptive() { return g_dns != nullptr; }

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
static void mdnsApply() {
  MDNS.end(); g_mdnsRun = false;
  if (!cfg.mdnsOn) { vlog("NET: mDNS spento"); return; }
  if (MDNS.begin(cfg.hostname.c_str())) {
    g_mdnsRun = true;
    if (cfg.httpOn) MDNS.addService("http", "tcp", cfg.httpPort);
    if (cfg.https) MDNS.addService("https", "tcp", cfg.httpsPort);
  }
}

// ---- modalita aereo ----
static volatile bool g_airReq = false, g_airOffReq = false;
bool netAirplane() { return cfg.airOn; }

bool netAirplaneOn(int ex, uint32_t param, String& err) {
  if (ex < 0 || ex > 3) { err = tr("Modo di uscita non valido"); return false; }
  if (ex == 1 && (param < 10 || param > 7UL * 86400UL)) { err = tr("Tempo da 10 secondi a 7 giorni"); return false; }
  if (ex == 2 && param > 1439) { err = tr("Orario non valido"); return false; }
  cfg.airExit = ex; cfg.airAt = (ex == 2) ? param : 0;
  time_t now = time(NULL);
  cfg.airUntil = (ex == 1) ? (now > 1700000000 ? (uint32_t)now + param : (uint32_t)(millis() / 1000) + param) : 0;
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

String netAirplaneText() {
  if (!cfg.airOn) return tr("Modalita aereo: spenta");
  String w;
  if (cfg.airExit == 0) w = tr("al prossimo avvio");
  else if (cfg.airExit == 1) {
    time_t now = time(NULL);
    long left = (cfg.airUntil > 1700000000UL) ? (long)cfg.airUntil - (long)now : (long)cfg.airUntil - (long)(millis() / 1000);
    w = trf("tra %ld s", left < 0 ? 0L : left);
  } else if (cfg.airExit == 2) { char b[8]; snprintf(b, sizeof(b), "%02u:%02u", cfg.airAt / 60, cfg.airAt % 60); w = trf("alle %s", b); }
  else w = tr("solo a mano");
  return trf("Modalita aereo: ATTIVA (si riattiva %s; sempre: 'airplane off' dalla seriale o tasto BOOT)", w.c_str());
}

// controllo ogni secondo: e ora di riaccendere?
static void airCheck() {
  if (!cfg.airOn) return;
  time_t now = time(NULL);
  if (cfg.airExit == 1) {
    bool due = (cfg.airUntil > 1700000000UL) ? ((uint32_t)now >= cfg.airUntil) : ((uint32_t)(millis() / 1000) >= cfg.airUntil);
    if (due) netAirplaneOff("tempo scaduto");
  } else if (cfg.airExit == 2 && now > 1700000000) {
    struct tm t; localtime_r(&now, &t);
    if ((uint16_t)(t.tm_hour * 60 + t.tm_min) == cfg.airAt) netAirplaneOff("orario raggiunto");
  }
}

static void airEnter() {
  meshStop();
  bleStop();
  dnsStop();
  MDNS.end(); g_mdnsRun = false;
  drvWifiDisconnect(true, false);
  drvWifiSetMode(DW_OFF);
  setState(NET_AIR);
}

String netIpString() {
  if (g_state == NET_AP) return ipToStr(drvWifiApIp());   // anche durante la scansione (modo AP+STA)
  return ipToStr(drvWifiLocalIp());
}

String netStatusJson() {
  String mode = (g_state == NET_AIR) ? String(tr("Modo aereo")) : (g_state == NET_AP) ? String("AP") : (g_state == NET_CLIENT_OK ? String("Client") : (g_state == NET_CLIENT_TRY ? String(tr("Connessione...")) : String(tr("Avvio"))));
  String j = "{\"mode\":\"" + mode + "\"";
  j += ",\"ip\":\"" + netIpString() + "\"";
  j += ",\"host\":\"" + jsonEscape(cfg.hostname) + "\"";
  j += ",\"fqdn\":\"" + jsonEscape(cfg.domain.length() ? cfg.hostname + "." + cfg.domain : cfg.hostname) + "\"";
  j += ",\"st\":" + String((int)g_state);                 // 0 avvio, 1 AP, 2 provo client, 3 client ok
  j += ",\"apSsid\":\"" + jsonEscape(cfg.apSsid) + "\"";
  // Valori IP reali (stringa vuota se non disponibili)
  auto ipS = [](uint32_t a) { return a == 0 ? String("") : ipToStr(a); };
  String gw = "", mk = "", d1 = "", d2 = "";
  if (g_state == NET_CLIENT_OK) {
    j += ",\"ssid\":\"" + jsonEscape(drvWifiSsid()) + "\"";
    j += ",\"rssi\":" + String(drvWifiRssi());
    gw = ipS(drvWifiGateway()); mk = ipS(drvWifiMask()); d1 = ipS(drvWifiDns(0)); d2 = ipS(drvWifiDns(1));
  } else {
    j += ",\"ssid\":\"" + jsonEscape(cfg.apSsid) + "\"";
    if (g_state == NET_AP) { gw = ipToStr(drvWifiApIp()); mk = "255.255.255.0"; }
  }
  j += ",\"gw\":\"" + gw + "\",\"mask\":\"" + mk + "\",\"dns\":\"" + d1 + "\",\"dns2\":\"" + d2 + "\"";
  j += ",\"ch\":" + String((int)drvWifiChannel());
  if (g_state == NET_AP) j += ",\"clients\":" + String((int)drvWifiApClients());
  j += ",\"staFail\":" + String(g_staFail && cfg.staEnabled ? "true" : "false");
  j += ",\"apOn\":" + String(cfg.apOn ? "true" : "false");
  j += ",\"dhcpRun\":" + String(g_state == NET_AP && dhcpRunning() ? "true" : "false") + ",\"mdnsRun\":" + String(g_mdnsRun ? "true" : "false");
  j += ",\"captive\":" + String(g_dns ? "true" : "false") + ",\"air\":" + String(cfg.airOn ? "true" : "false") +
       ",\"airExit\":" + String(cfg.airExit) + ",\"airAt\":" + String(cfg.airAt);
  j += ",\"mac\":\"" + netMac(false) + "\",\"apMac\":\"" + netMac(true) + "\"";
  j += "}";
  return j;
}

// MAC letto dalla eFuse: funziona anche con il Wi-Fi spento o in un altro modo.
String netMac(bool ap) {
  uint8_t m[6] = {0};
  if (esp_read_mac(m, ap ? ESP_MAC_WIFI_SOFTAP : ESP_MAC_WIFI_STA) != ESP_OK) return "";
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

String netInfoText() {
  String t;
  const char* modo = g_state == NET_AP ? "Access Point" : g_state == NET_CLIENT_OK ? "Client" : "-";
  t += trf("Modo:        %s", modo) + "\n";
  t += trf("Nome host:   %s", cfg.hostname.c_str()) + "\n";
  t += trf("IP:          %s", netIpString().c_str()) + "\n";
  if (g_state == NET_CLIENT_OK) {
    t += trf("Rete:        %s (%d dBm, canale %d)", drvWifiSsid().c_str(), drvWifiRssi(), drvWifiChannel()) + "\n";
    t += trf("Gateway:     %s", ipToStr(drvWifiGateway()).c_str()) + "\n";
  }
  t += trf("MAC client:  %s", netMac(false).c_str()) + "\n";
  t += trf("MAC AP:      %s", netMac(true).c_str()) + "\n";
  return t;
}

String netScanJson() {
  return "{\"running\":" + String(g_scanRun ? "true" : "false") + ",\"more\":" + String(g_scanMore ? "true" : "false") + ",\"list\":" + g_scanJson + "}";
}

// L'hotspot parte se il servizio e acceso, oppure se non c'e nessuna Wi-Fi di casa (altrimenti la scheda sarebbe irraggiungibile).
static bool apAllowed() { return cfg.apOn || !cfg.staEnabled || !cfg.staSsid.length(); }

static void startAp() {
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
  if (!ipParse(cfg.ip, ip) || !ipParse(cfg.mask, mk) || !maskValid(mk) || !ipParse(cfg.gw, gw)) {
    vlog("NET: IP statico non valido, uso DHCP");
    return false;
  }
  ipParse(cfg.dns1, d1); ipParse(cfg.dns2, d2);
  drvWifiStaConfigStatic(ip, gw, mk, d1, d2);
  return true;
}

static bool tryClient() {
  dnsStop();
  setState(NET_CLIENT_TRY);
  drvWifiDisconnect(true, false);
  delay(200);
  drvWifiSetMode(DW_STA);
  regionApplyRadio();                                 // paese e potenza PRIMA di trasmettere
  drvWifiSetHostname(cfg.hostname.c_str());
  if (!applyStaticIp()) drvWifiStaConfigDhcp();       // torna a DHCP
  drvWifiStaBegin(nullptr, cfg.staSsid.c_str(), cfg.staPass.c_str());
  for (int i = 0; i < 40; i++) {            // max 20 s
    if (drvWifiStaStatus() == DWS_CONNECTED) {
      setState(NET_CLIENT_OK); g_staFail = false;
      vlog("NET: client connesso, IP %s", ipToStr(drvWifiLocalIp()).c_str());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  g_staFail = true;
  return false;
}

static void doScan() {
  g_scanRun = true;
  DrvWifiMode m = drvWifiMode();
  if (m == DW_AP) drvWifiSetMode(DW_AP_STA);
  int n = drvWifiScanStart();                   // il paese impostato limita i canali cercati
  String j = "[";
  g_scanMore = n > SCAN_MAX;
  if (n > SCAN_MAX) n = SCAN_MAX;
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(drvWifiScanSsid(i)) + "\",\"rssi\":" + String(drvWifiScanRssi(i)) +
         ",\"ch\":" + String(drvWifiScanChannel(i)) +
         ",\"open\":" + String(drvWifiScanOpen(i) ? "true" : "false") + "}";
  }
  j += "]";
  g_scanJson = j;
  drvWifiScanClear();
  if (m == DW_AP) drvWifiSetMode(DW_AP);
  g_scanRun = false;
}

// ---- prova del Wi-Fi di casa dalla guida: l'hotspot resta acceso, niente viene salvato ----
static volatile bool g_tReq = false;
static volatile int g_tState = 0;          // 0 niente, 1 in corso, 2 collegata, 3 non riuscita
static String g_tSsid, g_tPass, g_tIp, g_tErr;
void netTestStart(const String& ssid, const String& pass) {
  if (g_tState == 1) return;
  g_tSsid = ssid; g_tPass = pass; g_tIp = ""; g_tErr = ""; g_tState = 1; g_tReq = true;
}
String netTestJson() {
  String j = "{\"state\":" + String((int)g_tState);
  if (g_tState == 2) j += ",\"ip\":\"" + jsonEscape(g_tIp) + "\"";
  if (g_tState == 3) j += ",\"err\":\"" + jsonEscape(g_tErr) + "\"";
  return j + "}";
}
static void doTest() {
  if (g_state != NET_AP) { g_tErr = tr("Prova possibile solo con l'hotspot acceso"); g_tState = 3; return; }
  drvWifiSetMode(DW_AP_STA);
  drvWifiStaBegin(cfg.hostname.c_str(), g_tSsid.c_str(), g_tPass.c_str());
  DrvWifiSta st = DWS_IDLE;
  for (int i = 0; i < 40; i++) {                       // max 20 s
    st = drvWifiStaStatus();
    if (st == DWS_CONNECTED) break;
    vTaskDelay(pdMS_TO_TICKS(500));
    wdBeat("net");
  }
  if (st == DWS_CONNECTED) { g_tIp = ipToStr(drvWifiLocalIp()); g_tState = 2; vlog("NET: prova Wi-Fi di casa riuscita"); }
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
  wdWatch("net", 90);                                // tryClient puo durare 20 s, la scansione qualche secondo
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
      if (cfg.airOn) { airEnter(); webStart(); }
    }
    if (cfg.airOn) { airCheck(); vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    if (g_reconf) {
      g_reconf = false; fails = 0;
      if (cfg.staEnabled && cfg.staSsid.length()) {
        if (!tryClient() && apAllowed()) startAp();     // se fallisce torna AP (se il servizio AP e acceso)
      } else startAp();
      mdnsApply();
      webStart();                       // il server parte solo ora che la rete e pronta
    }
    if (g_svcReq) {                                  // DHCP, portale automatico e mDNS: si applicano subito, senza riavvio
      g_svcReq = false;
      if (g_state == NET_AP) {
        dhcpApply();
        if (cfg.apCaptive && cfg.httpOn && cfg.httpPort == 80) dnsStart(); else dnsStop();
      }
      mdnsApply();
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
    if (g_dns) g_dns->processNextRequest();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Wi-Fi come servizio (sola lettura nel registro): non si ferma da qui (si perderebbe la pagina); per spegnerlo c'e il modo aereo.
static int  svcState() { NetState n = netState(); return n == NET_CLIENT_OK || n == NET_AP ? SVC_ON : n == NET_AIR ? SVC_OFF : SVC_STARTING; }
static bool svcBegin(String& err) { (void)err; netReconfigure(); return true; }
static void svcStop() {}
static const ServiceOps NET_OPS = { "wifi", "Wi-Fi", "Rete", "net", ROLE_OPER, ROLE_ADMIN,
                                    svcState, svcBegin, svcStop, nullptr, netStatusJson, nullptr, nullptr };

void netInit() {
  serviceRegister(&NET_OPS);
  g_reconf = true;
  if (!cfg.apOn && cfg.dhcpOn && cfg.staEnabled && cfg.staSsid.length()) {     // schede configurate prima della 1.7.18: hotspot spento ma DHCP rimasto acceso
    cfg.dhcpOn = false; cfgSetOrigin("DHCP segue hotspot"); cfgSave(); vlog("NET: hotspot spento, spengo anche il DHCP");
  }
  xTaskCreatePinnedToCore(netTask, "net", 6144, NULL, 2, NULL, 0);
}
