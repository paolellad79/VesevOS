// VesevOS Recovery - rete Wi-Fi e mDNS (vedi rec_net.h).
#include "rec_net.h"
#include "rec_auth.h"
#include "rec_config.h"
#include "rec_led.h"
#include "rec_util.h"
#include "rec_version.h"
#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_random.h>
#include <mdns.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#define GOT_IP_BIT BIT0

static bool g_net = false, g_ap = false;
static volatile bool g_wantSta = false;             // vero = se cade il collegamento, riprova
static std::string g_apPass;
static EventGroupHandle_t g_events;
static esp_netif_t *g_staIf = nullptr, *g_apIf = nullptr;

static void onEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { if (g_wantSta) esp_wifi_connect(); }
  else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(g_events, GOT_IP_BIT);
}

bool recNetUp() { return g_net; }
bool recNetIsAp() { return g_ap; }

std::string recNetIp() {
  esp_netif_ip_info_t ip;
  if (!g_net || esp_netif_get_ip_info(g_ap ? g_apIf : g_staIf, &ip) != ESP_OK) return "0.0.0.0";
  char b[16];
  snprintf(b, sizeof b, IPSTR, IP2STR(&ip.ip));
  return b;
}

void recNetInfo() {
  if (!g_net) { printf("[recovery] rete non avviata\n"); return; }
  if (g_ap) printf("[recovery] HOTSPOT 'VesevOS-recovery' password %s - apri http://%s\n", g_apPass.c_str(), recNetIp().c_str());
  else printf("[recovery] rete di casa: apri http://%s oppure http://%s.local\n", recNetIp().c_str(), recNetName().c_str());
  if (!rc.adminOk) printf("[recovery] nessun amministratore: nella pagina scrivi il codice %s\n", recAuthCode().c_str());
}

// Il paese viene SOLO dalla configurazione (non si adottano le regole annunciate dai router vicini). Senza paese: "01" (mondo, il piu prudente).
static void applyCountry() {
  if (rc.country.size() == 2) {
    if (esp_wifi_set_country_code(rc.country.c_str(), false) == ESP_OK) return;
    printf("[recovery] paese '%s' non accettato: uso quello prudente (01)\n", rc.country.c_str());
  }
  esp_wifi_set_country_code("01", false);
}

// Indirizzo fisso (se la configurazione lo chiede e i numeri sono validi); altrimenti DHCP.
static void applyStaticIp() {
  if (rc.dhcp || rc.ip.empty()) return;
  esp_netif_ip_info_t info = {};
  if (esp_netif_str_to_ip4(rc.ip.c_str(), &info.ip) != ESP_OK || esp_netif_str_to_ip4(rc.mask.c_str(), &info.netmask) != ESP_OK ||
      esp_netif_str_to_ip4(rc.gw.c_str(), &info.gw) != ESP_OK) {
    printf("[recovery] indirizzo fisso non valido: uso il DHCP\n");
    return;
  }
  esp_netif_dns_info_t d1 = {}, d2 = {};
  d1.ip.type = d2.ip.type = ESP_IPADDR_TYPE_V4;
  if (esp_netif_str_to_ip4(rc.dns1.c_str(), &d1.ip.u_addr.ip4) != ESP_OK) d1.ip.u_addr.ip4 = info.gw;
  if (esp_netif_str_to_ip4(rc.dns2.c_str(), &d2.ip.u_addr.ip4) != ESP_OK) d2.ip.u_addr.ip4.addr = 0;
  esp_netif_dhcpc_stop(g_staIf);
  esp_netif_set_ip_info(g_staIf, &info);
  esp_netif_set_dns_info(g_staIf, ESP_NETIF_DNS_MAIN, &d1);
  esp_netif_set_dns_info(g_staIf, ESP_NETIF_DNS_BACKUP, &d2);
}

static bool connectHome() {
  wifi_config_t wc = {};
  strlcpy((char*)wc.sta.ssid, rc.ssid.c_str(), sizeof wc.sta.ssid);
  strlcpy((char*)wc.sta.password, rc.pass.c_str(), sizeof wc.sta.password);
  wc.sta.threshold.authmode = rc.pass.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
  wc.sta.pmf_cfg.capable = true;
  wc.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
  esp_wifi_set_mode(WIFI_MODE_STA);
  applyCountry();
  esp_wifi_set_config(WIFI_IF_STA, &wc);
  g_wantSta = true;
  printf("[recovery] mi collego alla rete di casa (fino a %d s)...\n", (int)(STA_WAIT_MS / 1000));
  if (esp_wifi_start() != ESP_OK) { printf("[recovery] ERRORE: Wi-Fi non parte\n"); return false; }
  uint32_t t0 = recMillis();
  while (recMillis() - t0 < STA_WAIT_MS) {
    if (xEventGroupWaitBits(g_events, GOT_IP_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(50)) & GOT_IP_BIT) return true;
    recLedTick();
  }
  return false;
}

static void startHotspot() {
  g_wantSta = false;
  esp_wifi_stop();
  esp_wifi_set_mode(WIFI_MODE_AP);
  applyCountry();
  static const char* ab = "abcdefghijkmnpqrstuvwxyz23456789";
  g_apPass.clear();
  for (int i = 0; i < 12; i++) g_apPass += ab[esp_random() % 32];
  wifi_config_t wc = {};
  strlcpy((char*)wc.ap.ssid, "VesevOS-recovery", sizeof wc.ap.ssid);
  wc.ap.ssid_len = strlen("VesevOS-recovery");
  strlcpy((char*)wc.ap.password, g_apPass.c_str(), sizeof wc.ap.password);
  wc.ap.channel = 1;                                  // canale 1, massimo 2 collegati
  wc.ap.max_connection = 2;
  wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
  esp_wifi_set_config(WIFI_IF_AP, &wc);
  g_ap = esp_wifi_start() == ESP_OK;
  if (!g_ap) printf("[recovery] ERRORE: hotspot non partito\n");
}

static void startMdns(const std::string& name) {
  if (mdns_init() != ESP_OK || mdns_hostname_set(name.c_str()) != ESP_OK) printf("[recovery] mDNS non partito: il nome %s.local non sara visibile\n", name.c_str());
}

void recNetStart() {
  recAuthNewCode();
  std::string name = recNetName();
  g_events = xEventGroupCreate();
  esp_netif_init();
  esp_event_loop_create_default();
  g_staIf = esp_netif_create_default_wifi_sta();
  g_apIf = esp_netif_create_default_wifi_ap();
  esp_netif_set_hostname(g_staIf, name.c_str());       // il nome va dato prima di collegarsi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);              // niente copie della password nella flash
  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onEvent, nullptr);
  g_net = true;
  bool home = false;
  if (rc.staOn && rc.ssid.size()) {
    applyStaticIp();
    home = connectHome();
  }
  if (home) {
    esp_wifi_set_ps(WIFI_PS_NONE);                     // pagina piu pronta durante il caricamento
    startMdns(name);
  } else startHotspot();
  recNetInfo();
}
