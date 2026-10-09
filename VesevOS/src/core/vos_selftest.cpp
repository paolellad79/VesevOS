// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_selftest.cpp
// Autodiagnosi: 16 prove, una per volta in un task a parte (cede il processore, non blocca il watchdog).
// Solo lettura, tranne un file di prova in LittleFS (subito cancellato). Le prove attive (LED, MQTT) solo se richieste.
#include "vos_selftest.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_sys.h"
#include "../net/vos_net.h"
#include "../interfaces/vos_web.h"
#include "../security/vos_tls.h"
#include "vos_time.h"
#include "vos_audit.h"
#include "../security/vos_auth.h"
#include "../packages/vos_mfa.h"
#include "vos_region.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_led.h"
#include "vos_log.h"
#include "../packages/vos_mqtt.h"
#include "../packages/vos_mesh.h"
#include "../packages/vos_ble.h"
#include "vos_diario.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_fs.h"
#include <esp_system.h>
#include "../drivers/vos_drv_gpio.h"

static const int NT = 16;                 // numero di prove
struct SelfEntry { uint8_t st; uint16_t ms; char name[28]; char det[224]; };
static SelfEntry* g_res = nullptr;        // allocato a ogni avvio (circa 2,7 KB), libero con selftestClear()
static volatile uint8_t g_state = 0;      // 0 mai lanciata, 1 in corso, 2 finita
static volatile int g_n = 0;              // prova in corso
static bool g_names = false, g_active = false;
static uint32_t g_totMs = 0;
static String g_when;

// ---------- aiuti ----------
static void title(const char* n) { if (g_res) { strncpy(g_res[g_n].name, n, sizeof(g_res[g_n].name) - 1); g_res[g_n].name[sizeof(g_res[g_n].name) - 1] = 0; } }
static void rep(uint8_t st, const String& d) {
  if (!g_res) return;
  SelfEntry& e = g_res[g_n];
  e.st = st; strncpy(e.det, d.c_str(), sizeof(e.det) - 1); e.det[sizeof(e.det) - 1] = 0;
}
static const char* yn(bool b) { return b ? tr("si") : tr("no"); }
static const char* onoff(bool b) { return b ? tr("acceso") : tr("spento"); }
static String maskMac(const String& m) { return g_names ? m : (m.length() >= 5 ? String("**:**:**:**:") + m.substring(m.length() - 5) : String("**")); }
static String maskIp(const String& ip) {
  if (g_names) return ip;
  int a = ip.indexOf('.'), b = a < 0 ? -1 : ip.indexOf('.', a + 1);
  return b < 0 ? String("x.x.x.x") : ip.substring(0, b) + ".x.x";
}
static uint8_t pctSt(int p) { return p > 75 ? SELF_ERR : p > 60 ? SELF_WARN : SELF_OK; }

// ---------- le prove ----------
static void tSys() {
  title(tr("Sistema"));
  String r = sysResetReason(); uint8_t st = SELF_OK;
  int rr = (int)esp_reset_reason();
  if (rr == 4 || rr == 5 || rr == 6 || rr == 7 || rr == 9) st = SELF_WARN;   // panic, watchdog, brownout
  rep(st, trf("Versione %s, %s rev %u, %u core, flash %u MB, sketch %u KB (liberi %u KB), ultimo reset: %s, avvii %u, acceso da %s",
    VOS_VERSION, ESP.getChipModel(), (unsigned)ESP.getChipRevision(), (unsigned)ESP.getChipCores(), (unsigned)(ESP.getFlashChipSize() / 1048576),
    (unsigned)(ESP.getSketchSize() / 1024), (unsigned)(ESP.getFreeSketchSpace() / 1024), r.c_str(), (unsigned)sysBootCount(), uptimeStr(sysUptimeSec()).c_str()));
}
static void tRam() {
  title(tr("Memoria RAM"));
  uint32_t tot = ESP.getHeapSize(), fr = ESP.getFreeHeap();
  int used = tot ? (int)(100 - 100ULL * fr / tot) : 0;
  uint8_t st = pctSt(used);
  if (ESP.getMinFreeHeap() < 20 * 1024 && st < SELF_WARN) st = SELF_WARN;
  String d = trf("RAM usata %d%%, libera %u KB, minima %u KB, blocco grande %u KB", used, (unsigned)(fr / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
  if (ESP.getPsramSize()) {
    int pu = (int)(100 - 100ULL * ESP.getFreePsram() / ESP.getPsramSize());
    d += trf("; PSRAM usata %d%%", pu);
    if (pctSt(pu) > st) st = pctSt(pu);
  }
  rep(st, d);
}
static void tCpu() {
  title(tr("CPU e temperatura"));
  int sum = 0, mx = 0, n = 0;
  uint32_t t0 = millis();
  for (int i = 0; i < 5; i++) { int c = sysCpuPercent(); sum += c; if (c > mx) mx = c; n++; vTaskDelay(400 / portTICK_PERIOD_MS); }
  unsigned secs = (unsigned)((millis() - t0 + 500) / 1000);
  int avg = n ? sum / n : 0;
  float tc = sysCpuTemp();
  uint8_t st = avg > 85 ? SELF_ERR : avg > 60 ? SELF_WARN : SELF_OK;
  String d = trf("CPU media %d%% (massimo %d%%) in %u secondi, %u MHz", avg, mx, secs, (unsigned)ESP.getCpuFreqMHz());
  if (isnan(tc)) { d += tr("; temperatura non disponibile"); if (st < SELF_WARN) st = SELF_WARN; }
  else {
    d += trf("; temperatura %d C", (int)(tc + 0.5f));
    uint8_t ts = tc > 85 ? SELF_ERR : tc > 70 ? SELF_WARN : SELF_OK;
    if (ts > st) st = ts;
  }
  if (st >= SELF_WARN && avg > 60) d += tr("; vedi Sistema > Task per il task che consuma di piu");
  rep(st, d);
}
static void tFs() {
  title(tr("Flash e file"));
  size_t tot = drvFsTotal(), us = drvFsUsed();
  String pat; for (int i = 0; i < 20; i++) pat += randomHex(8);
  const char* path = "/selftest.tmp"; bool ok = false;
  File f = drvFsOpen(path, "w");
  if (f) { f.print(pat); f.close(); File g = drvFsOpen(path, "r"); if (g) { String back = g.readString(); g.close(); ok = (back == pat); } drvFsRemove(path); }
  uint8_t st = ok ? SELF_OK : SELF_ERR;
  if (ok && tot && us * 100 / tot > 85) st = SELF_WARN;
  rep(st, trf("File: %u KB usati su %u KB; scrittura e lettura di prova: %s", (unsigned)(us / 1024), (unsigned)(tot / 1024), ok ? tr("riuscita") : tr("FALLITA")));
}
static void tCfg() {
  title(tr("Configurazione della scheda"));
  int red = auditCount(AUD_RED), yel = auditCount(AUD_YELLOW);
  uint8_t st = SELF_OK;
  if (!cfgLastSaveOk() || red) st = SELF_ERR; else if (yel || !cfg.setupDone) st = SELF_WARN;
  rep(st, trf("Ultimo salvataggio: %s; guida finita: %s; avvisi rossi %d, gialli %d (scheda Compliant)", cfgLastSaveOk() ? tr("riuscito") : tr("FALLITO"), yn(cfg.setupDone), red, yel));
}
static void tNet() {
  title(tr("Rete Wi-Fi"));
  NetState ns = netState(); uint8_t st = SELF_OK; String d;
  if (ns == NET_AIR) { rep(SELF_WARN, tr("Modo aereo: radio spenta")); return; }
  d = trf("Stato %d, modo Wi-Fi %d", (int)ns, (int)drvWifiMode());
  if (ns == NET_CLIENT_OK) {
    int rs = drvWifiRssi();
    d += trf("; Wi-Fi di casa %s, segnale %d dBm, IP %s", g_names ? drvWifiSsid().c_str() : "***", rs, maskIp(netIpString()).c_str());
    if (rs < -85) st = SELF_ERR; else if (rs < -75) st = SELF_WARN;
  } else if (cfg.staEnabled) { d += String("; ") + tr("Wi-Fi di casa configurata ma NON collegata"); st = SELF_WARN; }
  if (cfg.apOn) d += trf("; hotspot con %d client", (int)drvWifiApClients());
  d += trf("; DHCP %s, DNS %s, mDNS %s", onoff(cfg.dhcpOn), onoff(netCaptive()), onoff(cfg.mdnsOn));
  d += trf("; MAC %s", maskMac(netMac(false)).c_str());
  rep(st, d);
}
static void tWeb() {
  title(tr("Pagina web"));
  bool https = webHttpsUp(); uint8_t st = SELF_OK;
  if (!cfg.httpOn && !cfg.https) st = SELF_ERR;
  else if (cfg.https && !https) st = tlsPending() ? SELF_WARN : SELF_ERR;
  else if (!cfg.https) st = SELF_WARN;
  String fp = tlsFingerprint(); fp = fp.length() > 8 ? fp.substring(0, 8) : fp;
  rep(st, trf("HTTP %s porta %u; HTTPS %s porta %u (in funzione: %s); certificato %s, impronta %s...%s", onoff(cfg.httpOn), (unsigned)cfg.httpPort, onoff(cfg.https), (unsigned)cfg.httpsPort, yn(https),
    tlsCustom() ? tr("caricato da te") : tr("autofirmato dalla scheda"), fp.c_str(), tlsPending() ? (String(" ") + tr("(nuovo in attesa del riavvio)")).c_str() : ""));
}
static void tTime() {
  title(tr("Ora e data"));
  bool v = timeValid(); uint32_t last = timeLastSync(); uint8_t st = SELF_OK;
  if (!v) st = SELF_WARN; else if (cfg.ntpOn && !last && netState() == NET_CLIENT_OK) st = SELF_WARN;
  rep(st, trf("Ora valida: %s; NTP %s; ultima sincronizzazione: %s; fuso %s", yn(v), onoff(cfg.ntpOn), last ? timeNowStr().c_str() : tr("mai"), cfg.tzName.length() ? cfg.tzName.c_str() : "-"));
}
static void tSvc() {
  title(tr("Servizi attivi"));
  rep(SELF_OK, trf("Hotspot %s, DHCP %s, HTTP %s, HTTPS %s, MQTT %s (collegato: %s), ESP-NOW %s, Bluetooth %s, NTP %s, energia modo %d, statistiche %s",
    onoff(cfg.apOn), onoff(cfg.dhcpOn), onoff(cfg.httpOn), onoff(webHttpsUp()), onoff(mqttRunning()), yn(mqttConnected()), onoff(meshRunning()), onoff(bleRunning()), onoff(cfg.ntpOn), (int)cfg.pwMode, onoff(cfg.statOn)));
}
static void tTasks() {
  title(tr("Task e watchdog"));
  uint32_t me = uxTaskGetStackHighWaterMark(NULL), lp = 0;
  TaskHandle_t h = xTaskGetHandle("loopTask"); if (h) lp = uxTaskGetStackHighWaterMark(h);
  uint8_t st = SELF_OK; if (me < 600 || (h && lp < 600)) st = SELF_WARN;
  rep(st, trf("Task %u; stack libero minimo: prova %u B, loop %u B; watchdog task %s, rete %s, RAM %s; task led %s",
    (unsigned)uxTaskGetNumberOfTasks(), (unsigned)me, (unsigned)lp, onoff(cfg.wdTask), onoff(cfg.wdNet), onoff(cfg.wdRam), yn(sysTaskRunning("led"))));
}
static void tSec() {
  title(tr("Sicurezza e accessi"));
  int adm = 0, mfa = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) { adm++; if (mfaOn(i)) mfa++; }
  uint8_t st = SELF_OK;
  if (!authIsSet()) st = SELF_ERR; else if (!cfg.https || !cfg.serialAuth || !cfg.powBits || cfg.banFails > 10) st = SELF_WARN;
  rep(st, trf("Password impostata: %s; HTTPS %s; seriale protetta: %s; prova di lavoro al login %u bit; blocco IP dopo %u errori; amministratori %d (con MFA %d)",
    yn(authIsSet()), onoff(cfg.https), yn(cfg.serialAuth), (unsigned)cfg.powBits, (unsigned)cfg.banFails, adm, mfa));
}
static void tRadio() {
  title(tr("Radio e antenna"));
  uint8_t st = SELF_OK; int ch = drvWifiChannel();
  if (!cfg.country.length()) st = SELF_WARN;
  if (regionTxDbm() > regionMaxDbm() || (ch > 0 && !regionChannelOk(ch))) st = SELF_ERR;
  rep(st, trf("Paese %s; canali 1-%d (in uso %d); potenza %d dBm (massimo %d, limite del paese %d); antenna %s",
    cfg.country.length() ? cfg.country.c_str() : tr("NON scelto"), regionChannels(), ch, regionTxDbm(), regionMaxDbm(), regionLimitDbm(), cfg.antExt ? tr("esterna") : tr("interna")));
}
static void tPer() {
  title(tr("Periferiche e tasto BOOT"));
  bool boot = !drvGpioRead(VOS_PIN_BOOT);
  rep(boot ? SELF_WARN : SELF_OK, trf("LED sul pin %u, modo %u; pin registrati %d; tasto BOOT %s", (unsigned)cfg.ledPin, (unsigned)cfg.ledMode, pinCount(), boot ? tr("PREMUTO") : tr("rilasciato")));
}
static void tLog() {
  title(tr("Registro della scheda"));
  String l = logGet(60); int n = 0, bad = 0;
  for (int i = 0; i < (int)l.length(); i++) if (l[i] == '\n') n++;
  String low = l; low.toLowerCase();
  for (int p = 0; (p = low.indexOf("errore", p)) >= 0; p += 6) bad++;
  for (int p = 0; (p = low.indexOf("watchdog", p)) >= 0; p += 8) bad++;
  rep(bad ? SELF_WARN : SELF_OK, trf("Righe nel registro: %d; righe con errore o watchdog: %d", n, bad));
}
static void tActLed() {
  title(tr("LED (prova attiva)"));
  if (!g_active) { rep(SELF_SKIP, tr("Solo con le prove attive")); return; }
  ledIdentify(2000); rep(SELF_OK, tr("Il LED fa l'arcobaleno per 2 secondi: guardalo"));
}
static void tActMqtt() {
  title(tr("MQTT (prova attiva)"));
  if (!g_active) { rep(SELF_SKIP, tr("Solo con le prove attive")); return; }
  if (!mqttRunning()) { rep(SELF_SKIP, tr("MQTT fermo")); return; }
  if (!mqttConnected()) { rep(SELF_WARN, tr("MQTT avviato ma non collegato al broker")); return; }
  bool ok = mqttPublishRel("selftest", "ok", false);
  rep(ok ? SELF_OK : SELF_ERR, ok ? tr("Messaggio di prova inviato al broker") : tr("Invio del messaggio di prova fallito"));
}

typedef void (*SelfFn)();
static const SelfFn TESTS[NT] = { tSys, tRam, tCpu, tFs, tCfg, tNet, tWeb, tTime, tSvc, tTasks, tSec, tRadio, tPer, tLog, tActLed, tActMqtt };

static void selfTask(void*) {
  uint32_t t0 = millis();
  for (int i = 0; i < NT; i++) {
    g_n = i;
    uint32_t a = millis();
    TESTS[i]();
    g_res[i].ms = (uint16_t)min<uint32_t>(65000, millis() - a);
    vTaskDelay(30 / portTICK_PERIOD_MS);      // cede il processore: nessuna prova pesa piu di una frazione di secondo
  }
  g_totMs = millis() - t0;
  g_state = 2;
  vTaskDelete(NULL);
}

// ---------- interfaccia ----------
bool selftestRunning() { return g_state == 1; }
int  selftestTotal() { return NT; }
int  selftestDone() { return g_state == 2 ? NT : (g_state == 1 ? (int)g_n : 0); }
void selftestClear() { if (g_state != 1) { free(g_res); g_res = nullptr; g_state = 0; } }

bool selftestStart(bool active, bool names, String& err) {
  if (g_state == 1) { err = tr("Una prova e gia in corso"); return false; }
  if (ESP.getFreeHeap() < 30 * 1024) { err = tr("RAM troppo bassa per la diagnosi: chiudi le altre pagine e riprova"); return false; }
  free(g_res);
  g_res = (SelfEntry*)(psramFound() ? ps_malloc(NT * sizeof(SelfEntry)) : malloc(NT * sizeof(SelfEntry)));   // in PSRAM se c'e: risparmia RAM interna
  if (g_res) memset(g_res, 0, NT * sizeof(SelfEntry));
  if (!g_res) { err = tr("Memoria insufficiente"); return false; }
  g_active = active; g_names = names; g_n = 0; g_totMs = 0;
  g_when = timeValid() ? timeNowStr() : String(tr("ora non valida"));
  g_state = 1;
  if (xTaskCreatePinnedToCore(selfTask, "selftest", 5120, NULL, 1, NULL, 1) != pdPASS) { g_state = 0; free(g_res); g_res = nullptr; err = tr("Impossibile avviare la prova"); return false; }
  vlog("DIAGNOSI: avviata (prove attive %s)", active ? "si" : "no");
  return true;
}

static const char* stTag(uint8_t st) { return st == SELF_OK ? "OK" : st == SELF_WARN ? "AVVISO" : st == SELF_ERR ? "ERRORE" : st == SELF_SKIP ? "SALTATA" : "..."; }
static bool ready(int i) { return g_res && i >= 0 && i < NT && g_res[i].st != SELF_PEND && (g_state == 2 || i < g_n); }

String selftestLine(int i) {
  if (!ready(i)) return "";
  return String("[") + stTag(g_res[i].st) + "] " + g_res[i].name + ": " + g_res[i].det;
}

String selftestJson() {
  String j = "{\"state\":" + String((int)g_state) + ",\"total\":" + String(NT) + ",\"cur\":" + String((int)(g_state == 1 ? g_n : -1)) +
             ",\"active\":" + (g_active ? "true" : "false") + ",\"names\":" + (g_names ? "true" : "false") + ",\"ms\":" + String((unsigned long)g_totMs) + ",\"list\":[";
  bool first = true;
  for (int i = 0; i < NT; i++) {
    if (!ready(i)) continue;
    if (!first) j += ","; first = false;
    j += "{\"i\":" + String(i) + ",\"n\":\"" + jsonEscape(g_res[i].name) + "\",\"s\":" + String((int)g_res[i].st) + ",\"ms\":" + String((int)g_res[i].ms) + ",\"d\":\"" + jsonEscape(g_res[i].det) + "\"}";
  }
  return j + "]}";
}

String selftestReport() {
  if (!g_res || g_state != 2) return "";
  int c[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < NT; i++) c[g_res[i].st < 5 ? g_res[i].st : 0]++;
  String m = netMac(false), id = m.length() >= 17 ? m.substring(12, 14) + m.substring(15, 17) : String("----");
  String r; r.reserve(4096);
  r += "VesevOS - REPORT AUTODIAGNOSI\n";
  r += trf("Versione: %s   Scheda: %s (ID %s)   Data: %s", VOS_VERSION, VOS_BOARD, id.c_str(), g_when.c_str()); r += "\n";
  r += trf("Nomi e dati: %s   Prove attive: %s   Durata: %u ms", g_names ? tr("REALI") : tr("oscurati"), yn(g_active), (unsigned)g_totMs); r += "\n";
  r += trf("Riepilogo: %d OK, %d avvisi, %d errori, %d saltate", c[SELF_OK], c[SELF_WARN], c[SELF_ERR], c[SELF_SKIP]); r += "\n";
  r += "------------------------------------------------------------\n";
  for (int i = 0; i < NT; i++) {
    char hd[48]; snprintf(hd, sizeof(hd), "%02d [%s] ", i + 1, stTag(g_res[i].st));
    r += hd; r += g_res[i].name; r += " ("; r += String((int)g_res[i].ms); r += " ms)\n    "; r += g_res[i].det; r += "\n";
  }
  r += "------------------------------------------------------------\n";
  r += tr("Diario dei riavvii:"); r += "\n"; r += diaryText(8, g_names); r += "\n";
  if (g_names) {
    r += tr("Ultime righe del registro:"); r += "\n"; r += logGet(20); r += "\n";
  } else {
    r += tr("Registro omesso (puo contenere indirizzi e nomi). Per includerlo rifai la prova con i nomi reali."); r += "\n";
  }
  r += tr("Questo report non contiene password, chiavi o token."); r += "\n";
  return r;
}
