// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wifi.cpp
// Wi-Fi con esp_wifi + esp_netif + esp_event di ESP-IDF (senza la libreria WiFi di Arduino).
// Le impostazioni sono quelle che usava Arduino-ESP32 3.3: stessi buffer, stesse regole di riconnessione,
// stessa configurazione di client e hotspot. Qui la riconnessione ha in piu una pausa che cresce a ogni errore.
#include "vos_drv_wifi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <dhcpserver/dhcpserver.h>

// ---- stato del driver (gli eventi arrivano dal task degli eventi, le funzioni dal task di rete) ----
static bool                      s_up = false;          // esp_wifi_init fatto
static bool                      s_started = false;     // esp_wifi_start fatto
static esp_netif_t*              s_staIf = nullptr;
static esp_netif_t*              s_apIf = nullptr;
static esp_event_handler_instance_t s_wifiEv = nullptr, s_ipEv = nullptr;
static esp_timer_handle_t        s_retryTimer = nullptr;
static char                      s_hostname[64] = "";
static bool                      s_staticIp = false;    // l'IP del client e fisso (non si usa il DHCP)
static volatile DrvWifiSta       s_status = DWS_OTHER;
static volatile bool             s_connected = false;   // associato al router (anche senza IP)
static volatile bool             s_apUp = false;        // hotspot partito
static volatile bool             s_want = false;        // e stata chiesta una connessione e non e stata annullata
static volatile bool             s_first = true;        // primo errore dopo la richiesta: si riprova sempre
static volatile uint8_t          s_tries = 0;           // tentativi falliti di fila

// numeri IP: ESP-IDF li tiene con il primo numero nel byte piu basso, noi con il primo numero in alto
static uint32_t toNum(uint32_t espAddr) { return __builtin_bswap32(espAddr); }
static uint32_t toEsp(uint32_t num) { return __builtin_bswap32(num); }

// ---- riconnessione con pausa crescente ----
static bool reasonRetry(uint8_t reason) {      // stessi motivi che Arduino considera passeggeri
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED: case WIFI_REASON_AUTH_EXPIRE: case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: case WIFI_REASON_802_1X_AUTH_FAILED: case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_LEAVE: case WIFI_REASON_ASSOC_EXPIRE: case WIFI_REASON_ASSOC_TOOMANY: case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_NOT_ASSOCED: case WIFI_REASON_ASSOC_NOT_AUTHED: case WIFI_REASON_MIC_FAILURE: case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    case WIFI_REASON_INVALID_PMKID: case WIFI_REASON_BEACON_TIMEOUT: case WIFI_REASON_NO_AP_FOUND: case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_CONNECTION_FAIL: case WIFI_REASON_AP_TSF_RESET: case WIFI_REASON_ROAMING:
      return true;
    default:
      return false;
  }
}
static const uint32_t kBackoffMs[] = {200, 1000, 2000, 4000, 8000, 15000, 30000};   // pausa prima del tentativo successivo

static void retryNow(void*) {
  if (s_want && s_started && !s_connected) esp_wifi_connect();
}

static void onStaDisconnected(uint8_t reason) {
  s_connected = false;
  if (!reason) reason = WIFI_REASON_UNSPECIFIED;
  if (reason == WIFI_REASON_NO_AP_FOUND) s_status = DWS_NO_SSID;
  else if (reason != WIFI_REASON_AUTH_EXPIRE) s_status = DWS_OTHER;
  bool retry = s_want && reason != WIFI_REASON_ASSOC_LEAVE && (s_first || reasonRetry(reason));   // ASSOC_LEAVE = scollegata da noi
  s_first = false;
  if (!retry || !s_retryTimer) return;
  uint8_t n = s_tries < 6 ? s_tries : 6;
  if (s_tries < 255) s_tries = (uint8_t)(s_tries + 1);
  esp_timer_stop(s_retryTimer);
  esp_timer_start_once(s_retryTimer, (uint64_t)kBackoffMs[n] * 1000ULL);
}

static void onEvent(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base == WIFI_EVENT) {
    switch (id) {
      case WIFI_EVENT_STA_START:        s_status = DWS_OTHER; esp_wifi_set_ps(WIFI_PS_MIN_MODEM); break;
      case WIFI_EVENT_STA_STOP:         s_status = DWS_OTHER; s_connected = false; break;
      case WIFI_EVENT_STA_CONNECTED:    s_connected = true; s_status = DWS_IDLE; break;
      case WIFI_EVENT_STA_DISCONNECTED: onStaDisconnected(data ? ((wifi_event_sta_disconnected_t*)data)->reason : 0); break;
      case WIFI_EVENT_AP_START:         s_apUp = true; break;
      case WIFI_EVENT_AP_STOP:          s_apUp = false; break;
      default: break;
    }
  } else if (base == IP_EVENT) {
    if (id == IP_EVENT_STA_GOT_IP) { s_status = DWS_CONNECTED; s_tries = 0; }
    else if (id == IP_EVENT_STA_LOST_IP) s_status = DWS_IDLE;
  }
}

// ---- accensione e spegnimento della radio ----
static bool radioUp() {
  if (s_up) return true;
  if (esp_netif_init() != ESP_OK) return false;
  esp_err_t e = esp_event_loop_create_default();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return false;       // INVALID_STATE = esiste gia
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.static_tx_buf_num = 0;          // buffer dinamici, come Arduino-ESP32 (di fabbrica)
  cfg.dynamic_tx_buf_num = 32;
  cfg.tx_buf_type = 1;
  cfg.cache_tx_buf_num = 4;
  cfg.static_rx_buf_num = 4;
  cfg.dynamic_rx_buf_num = 32;
  if (esp_wifi_init(&cfg) != ESP_OK) return false;
  esp_wifi_set_storage(WIFI_STORAGE_RAM);                            // la password della rete resta nella nostra configurazione, non in una seconda copia nella flash
  s_apIf = esp_netif_create_default_wifi_ap();
  s_staIf = esp_netif_create_default_wifi_sta();
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr, &s_wifiEv);
  esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr, &s_ipEv);
  if (!s_retryTimer) {
    esp_timer_create_args_t a = {}; a.callback = retryNow; a.name = "wifiretry";
    esp_timer_create(&a, &s_retryTimer);
  }
  s_up = true;
  return true;
}

static void radioDown() {
  if (!s_up) return;
  s_want = false;
  if (s_retryTimer) esp_timer_stop(s_retryTimer);
  if (s_started) { esp_wifi_stop(); s_started = false; }
  if (s_wifiEv) { esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifiEv); s_wifiEv = nullptr; }
  if (s_ipEv) { esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, s_ipEv); s_ipEv = nullptr; }
  if (s_apIf) { esp_netif_destroy_default_wifi(s_apIf); s_apIf = nullptr; }
  if (s_staIf) { esp_netif_destroy_default_wifi(s_staIf); s_staIf = nullptr; }
  esp_wifi_deinit();
  s_up = false; s_staticIp = false; s_connected = false; s_apUp = false; s_status = DWS_OTHER;
}

DrvWifiMode drvWifiMode() {
  wifi_mode_t m;
  if (!s_started || esp_wifi_get_mode(&m) != ESP_OK) return DW_OFF;
  switch (m) {
    case WIFI_MODE_STA: return DW_STA;
    case WIFI_MODE_AP: return DW_AP;
    case WIFI_MODE_APSTA: return DW_AP_STA;
    default: return DW_OFF;
  }
}

void drvWifiSetMode(DrvWifiMode m) {
  if (m == DW_OFF) { radioDown(); return; }
  if (!radioUp()) return;
  DrvWifiMode cur = drvWifiMode();
  bool wantSta = (m == DW_STA || m == DW_AP_STA), hadSta = (cur == DW_STA || cur == DW_AP_STA);
  if (wantSta && !hadSta && s_hostname[0] && s_staIf) esp_netif_set_hostname(s_staIf, s_hostname);   // il nome va dato prima di collegarsi
  if (!wantSta) { s_want = false; s_connected = false; }
  esp_wifi_set_mode(m == DW_STA ? WIFI_MODE_STA : m == DW_AP ? WIFI_MODE_AP : WIFI_MODE_APSTA);
  if (!s_started && esp_wifi_start() == ESP_OK) s_started = true;
}

void drvWifiDisconnect(bool wifiOff, bool eraseAp) {
  DrvWifiMode m = drvWifiMode();
  if (m == DW_OFF || m == DW_AP) return;        // senza client non c'e niente da scollegare
  s_want = false;
  if (s_retryTimer) esp_timer_stop(s_retryTimer);
  esp_wifi_disconnect();
  for (int i = 0; i < 20 && s_connected; i++) vTaskDelay(pdMS_TO_TICKS(5));   // al massimo 100 ms
  if (eraseAp) { wifi_config_t empty; memset(&empty, 0, sizeof empty); esp_wifi_set_config(WIFI_IF_STA, &empty); }
  if (wifiOff) drvWifiSetMode(m == DW_STA ? DW_OFF : DW_AP);          // spegne la parte client (e la radio se era l'unica)
}

// ---- client (rete di casa) ----
void drvWifiSetHostname(const char* hostname) {
  snprintf(s_hostname, sizeof s_hostname, "%s", hostname ? hostname : "");
  if (s_staIf && s_hostname[0]) esp_netif_set_hostname(s_staIf, s_hostname);
}

static void setDns(uint32_t d1, uint32_t d2) {
  esp_netif_dns_info_t d; memset(&d, 0, sizeof d);
  d.ip.type = ESP_IPADDR_TYPE_V4;
  d.ip.u_addr.ip4.addr = d1; esp_netif_set_dns_info(s_staIf, ESP_NETIF_DNS_MAIN, &d);
  d.ip.u_addr.ip4.addr = d2; esp_netif_set_dns_info(s_staIf, ESP_NETIF_DNS_BACKUP, &d);
  d.ip.u_addr.ip4.addr = 0;  esp_netif_set_dns_info(s_staIf, ESP_NETIF_DNS_FALLBACK, &d);
}

void drvWifiStaConfigStatic(uint32_t ip, uint32_t gw, uint32_t mask, uint32_t dns1, uint32_t dns2) {
  if (!s_staIf) return;
  esp_netif_dhcpc_stop(s_staIf);               // "gia fermo" non e un errore
  esp_netif_ip_info_t info; memset(&info, 0, sizeof info);
  info.ip.addr = toEsp(ip); info.gw.addr = toEsp(gw); info.netmask.addr = toEsp(mask);
  esp_netif_set_ip_info(s_staIf, &info);
  setDns(toEsp(dns1), toEsp(dns2));
  s_staticIp = true;
}

void drvWifiStaConfigDhcp() {
  if (!s_staIf) return;
  esp_netif_dhcpc_stop(s_staIf);
  esp_netif_ip_info_t info; memset(&info, 0, sizeof info);
  esp_netif_set_ip_info(s_staIf, &info);
  setDns(0, 0);
  esp_netif_dhcpc_start(s_staIf);              // "gia acceso" non e un errore
  s_staticIp = false;
}

void drvWifiStaBegin(const char* hostname, const char* ssid, const char* pass) {
  if (hostname) drvWifiSetHostname(hostname);
  DrvWifiMode m = drvWifiMode();
  if (m == DW_OFF) drvWifiSetMode(DW_STA);
  else if (m == DW_AP) drvWifiSetMode(DW_AP_STA);
  if (!s_started || !s_staIf) return;
  if (!ssid || !ssid[0] || strlen(ssid) > 32 || (pass && strlen(pass) > 64)) return;
  if (s_connected) {                           // gia collegata a un'altra rete: prima si scollega (al massimo 1 s)
    esp_wifi_disconnect();
    for (int i = 0; i < 200 && s_connected; i++) vTaskDelay(pdMS_TO_TICKS(5));
  }
  wifi_config_t conf; memset(&conf, 0, sizeof conf);
  conf.sta.scan_method = WIFI_FAST_SCAN;
  conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
  conf.sta.threshold.rssi = -127;
  conf.sta.pmf_cfg.capable = true;
  strncpy((char*)conf.sta.ssid, ssid, sizeof conf.sta.ssid);
  if (pass && pass[0]) {
    conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;        // almeno WPA2: niente reti aperte o WEP se c'e una password
    strncpy((char*)conf.sta.password, pass, sizeof conf.sta.password);
  }
  if (esp_wifi_set_config(WIFI_IF_STA, &conf) != ESP_OK) return;
  if (!s_staticIp) drvWifiStaConfigDhcp();
  s_first = true; s_tries = 0; s_want = true;
  s_status = DWS_OTHER;
  if (s_retryTimer) esp_timer_stop(s_retryTimer);
  esp_wifi_connect();
}

DrvWifiSta drvWifiStaStatus() { return s_status; }

DrvSsid drvWifiSsid() {
  DrvSsid r; r.s[0] = 0;
  wifi_ap_record_t info;
  if (s_started && esp_wifi_sta_get_ap_info(&info) == ESP_OK) { memcpy(r.s, info.ssid, 32); r.s[32] = 0; }
  return r;
}
int drvWifiRssi() {
  wifi_ap_record_t info;
  return (s_started && esp_wifi_sta_get_ap_info(&info) == ESP_OK) ? info.rssi : 0;
}

static uint32_t ipInfo(esp_netif_t* nif, int which) {       // 0 indirizzo, 1 gateway, 2 maschera
  esp_netif_ip_info_t info;
  if (!nif || esp_netif_get_ip_info(nif, &info) != ESP_OK) return 0;
  return toNum(which == 0 ? info.ip.addr : which == 1 ? info.gw.addr : info.netmask.addr);
}
uint32_t drvWifiLocalIp() { return ipInfo(s_staIf, 0); }
uint32_t drvWifiGateway() { return ipInfo(s_staIf, 1); }
uint32_t drvWifiMask() { return ipInfo(s_staIf, 2); }
uint32_t drvWifiDns(int i) {
  esp_netif_dns_info_t d;
  if (!s_staIf || i < 0 || i >= (int)ESP_NETIF_DNS_MAX || esp_netif_get_dns_info(s_staIf, (esp_netif_dns_type_t)i, &d) != ESP_OK) return 0;
  return toNum(d.ip.u_addr.ip4.addr);
}

// ---- hotspot ----
// Server DHCP: indirizzi da ip+1 per 10 client (come Arduino), maschera da /24 a /28. false se i numeri non vanno bene.
static bool apNetwork(uint32_t ip, uint32_t gw, uint32_t mask) {
  if (!s_apIf) return false;
  uint32_t host = ~mask;                       // parte "libera" della maschera
  int cidr = 32 - __builtin_popcount(host);
  if (cidr < 24 || cidr > 28) return false;
  esp_netif_dhcps_stop(s_apIf);                // "gia fermo" non e un errore
  esp_netif_ip_info_t info; memset(&info, 0, sizeof info);
  info.ip.addr = toEsp(ip); info.gw.addr = toEsp(gw); info.netmask.addr = toEsp(mask);
  if (esp_netif_set_ip_info(s_apIf, &info) != ESP_OK) return false;
  uint32_t start = ip + 1;
  if ((start & host) >= host - 10) start = (start & mask) + 1;           // non uscire dalla rete
  if (ip >= start && ip <= start + 10) return false;                     // la scheda non puo stare nell'elenco dei client
  dhcps_lease_t lease; memset(&lease, 0, sizeof lease);
  lease.enable = true;
  lease.start_ip.addr = toEsp(start); lease.end_ip.addr = toEsp(start + 10);
  if (esp_netif_dhcps_option(s_apIf, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, &lease, sizeof lease) != ESP_OK) return false;
  return esp_netif_dhcps_start(s_apIf) == ESP_OK;
}

// Nome, password e canale dell'hotspot. Se sono gia quelli in uso non si tocca niente (il Wi-Fi non si riavvia).
static bool apConfig(const char* ssid, const char* pass, int channel) {
  if (!ssid || !ssid[0] || strlen(ssid) > 32) return false;
  size_t pl = pass ? strlen(pass) : 0;
  if (pl > 0 && pl < 8) return false;                                    // WPA2 vuole almeno 8 caratteri
  if (pl > 63) return false;
  wifi_config_t conf; memset(&conf, 0, sizeof conf);
  conf.ap.channel = (uint8_t)channel;
  conf.ap.max_connection = 4;
  conf.ap.beacon_interval = 100;
  strncpy((char*)conf.ap.ssid, ssid, sizeof conf.ap.ssid);
  conf.ap.ssid_len = (uint8_t)strlen(ssid);
  if (pl) {
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;                     // senza TKIP, che e insicuro
    strncpy((char*)conf.ap.password, pass, sizeof conf.ap.password);
  }
  wifi_config_t cur;
  if (esp_wifi_get_config(WIFI_IF_AP, &cur) == ESP_OK && strncmp((char*)cur.ap.ssid, (char*)conf.ap.ssid, 32) == 0 &&
      strncmp((char*)cur.ap.password, (char*)conf.ap.password, 64) == 0 && cur.ap.channel == conf.ap.channel &&
      cur.ap.authmode == conf.ap.authmode && cur.ap.max_connection == conf.ap.max_connection) return true;
  return esp_wifi_set_config(WIFI_IF_AP, &conf) == ESP_OK;
}

static bool apEnsureOn() {                     // modo con hotspot e hotspot partito (attesa massima 1 s)
  DrvWifiMode m = drvWifiMode();
  if (m == DW_OFF) drvWifiSetMode(DW_AP);
  else if (m == DW_STA) drvWifiSetMode(DW_AP_STA);
  for (int i = 0; i < 200 && s_started && !s_apUp; i++) vTaskDelay(pdMS_TO_TICKS(5));
  return s_started && s_apUp;
}

bool drvWifiApStart(const char* ssid, const char* pass, int channel, uint32_t ip, uint32_t gw, uint32_t mask) {
  if (!apEnsureOn()) return false;
  if (!apNetwork(ip, gw, mask)) return false;
  return apConfig(ssid, pass, channel);
}
bool drvWifiApRetune(const char* ssid, const char* pass, int channel) {
  if (!apEnsureOn()) return false;
  return apConfig(ssid, pass, channel);
}
uint32_t drvWifiApIp() { return ipInfo(s_apIf, 0); }
int drvWifiApClients() {
  wifi_sta_list_t l;
  return (s_started && esp_wifi_ap_get_sta_list(&l) == ESP_OK) ? l.num : 0;
}
int drvWifiChannel() {
  uint8_t ch = 0; wifi_second_chan_t sc;
  return (s_started && esp_wifi_get_channel(&ch, &sc) == ESP_OK) ? ch : 0;
}

// ---- radio: paese, potenza, risparmio energia ----
bool drvWifiApplyRegion(const char* ccIn, int maxChannels, int txDbm) {
  if (drvWifiMode() == DW_OFF) return false;
  char cc[4]; snprintf(cc, sizeof cc, "%s", ccIn);
  // false = la scheda NON prende il paese dai router vicini (potrebbero annunciare regole diverse)
  esp_err_t e = esp_wifi_set_country_code(cc, false);
  if (e != ESP_OK && strcmp(cc, "01") != 0) { esp_wifi_set_country_code("01", false); strcpy(cc, "01"); }
  if (strcmp(cc, "01") == 0 || maxChannels == 11) {
    wifi_country_t wc; memset(&wc, 0, sizeof(wc));
    if (esp_wifi_get_country(&wc) == ESP_OK && wc.schan + wc.nchan - 1 > maxChannels) {
      wc.schan = 1; wc.nchan = maxChannels; wc.policy = WIFI_COUNTRY_POLICY_MANUAL;
      esp_wifi_set_country(&wc);
    }
  }
  esp_wifi_set_max_tx_power((int8_t)(txDbm * 4));       // unita di 0,25 dBm
  return true;
}
void drvWifiPowerSave(bool maxSaving) { esp_wifi_set_ps(maxSaving ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM); }
void drvWifiShutdown() { drvWifiDisconnect(true, false); drvWifiSetMode(DW_OFF); }

// ---- scansione delle reti vicine ----
#define SCAN_KEEP 40                            // reti ricordate (le altre si contano soltanto)
static wifi_ap_record_t* s_scan = nullptr;
static int s_scanKept = 0;

int drvWifiScanStart() {
  drvWifiScanClear();
  DrvWifiMode m = drvWifiMode();
  if (m == DW_OFF) drvWifiSetMode(DW_STA);
  else if (m == DW_AP) drvWifiSetMode(DW_AP_STA);
  if (!s_started) return -2;
  wifi_scan_config_t sc; memset(&sc, 0, sizeof sc);
  sc.show_hidden = true;                        // anche le reti nascoste
  sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  sc.scan_time.active.min = 100;
  sc.scan_time.active.max = 300;
  if (esp_wifi_scan_start(&sc, true) != ESP_OK) return -2;      // blocca fino alla fine
  uint16_t total = 0;
  esp_wifi_scan_get_ap_num(&total);
  uint16_t keep = total < SCAN_KEEP ? total : SCAN_KEEP;
  if (keep) s_scan = (wifi_ap_record_t*)calloc(keep, sizeof(wifi_ap_record_t));
  if (!s_scan) { esp_wifi_clear_ap_list(); return 0; }
  esp_wifi_scan_get_ap_records(&keep, s_scan);  // libera anche l'elenco interno
  s_scanKept = keep;
  return total;
}
DrvSsid drvWifiScanSsid(int i) {
  DrvSsid r; r.s[0] = 0;
  if (s_scan && i >= 0 && i < s_scanKept) { memcpy(r.s, s_scan[i].ssid, 32); r.s[32] = 0; }
  return r;
}
int  drvWifiScanRssi(int i) { return (s_scan && i >= 0 && i < s_scanKept) ? s_scan[i].rssi : 0; }
int  drvWifiScanChannel(int i) { return (s_scan && i >= 0 && i < s_scanKept) ? s_scan[i].primary : 0; }
bool drvWifiScanOpen(int i) { return !(s_scan && i >= 0 && i < s_scanKept) || s_scan[i].authmode == WIFI_AUTH_OPEN; }
void drvWifiScanClear() { free(s_scan); s_scan = nullptr; s_scanKept = 0; }

void drvWifiMac(bool ap, char* out, size_t n) {
  uint8_t m[6] = {0};
  if (!n) return;
  if (esp_read_mac(m, ap ? ESP_MAC_WIFI_SOFTAP : ESP_MAC_WIFI_STA) != ESP_OK) { out[0] = 0; return; }
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}
