// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_selftest.cpp
// Autodiagnosi: 16 prove, una per volta in un task a parte (cede il processore, non blocca il watchdog).
// Solo lettura, tranne un file di prova in LittleFS (subito cancellato). Le prove attive (LED, MQTT) solo se richieste.
#include "vos_selftest.h"
#include "../drivers/vos_drv_task.h"
#include "../core/vos_strbuf.h"
#include <ctype.h>
#include <string.h>
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_sys.h"
#include "../net/vos_net.h"
#include "../core/vos_hooks.h"
#include "../core/vos_service.h"
#include "../security/vos_tls.h"
#include "../core/vos_time.h"
#include "../core/vos_audit.h"
#include "../security/vos_auth.h"
#include "../core/vos_region.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_led.h"
#include "../devices/vos_sd.h"
#include "../core/vos_log.h"
#include "../core/vos_diario.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_fs.h"
#include <esp_system.h>
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_ota.h"
#include <stdarg.h>
#include <cmath>
#include <algorithm>
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"

static const int NT = 16;                 // numero di prove
struct SelfEntry { uint8_t st; uint16_t ms; char name[28]; char det[224]; };
static SelfEntry* g_res = nullptr;        // allocato a ogni avvio (circa 2,7 KB), libero con selftestClear()
static volatile uint8_t g_state = 0;      // 0 mai lanciata, 1 in corso, 2 finita
static volatile int g_n = 0;              // prova in corso
static bool g_names = false, g_active = false;
static uint32_t g_totMs = 0;
static char g_when[32];

// ---------- aiuti ----------
static void title(const char* n) { if (g_res) { strncpy(g_res[g_n].name, n, sizeof(g_res[g_n].name) - 1); g_res[g_n].name[sizeof(g_res[g_n].name) - 1] = 0; } }
static void rep(uint8_t st, const char* d) {
  if (!g_res) return;
  SelfEntry& e = g_res[g_n];
  e.st = st; strncpy(e.det, d, sizeof(e.det) - 1); e.det[sizeof(e.det) - 1] = 0;
}
// Come rep(), con il dettaglio scritto come printf (il formato e un testo tradotto con tr()); se non ci sta si taglia.
static void repf(uint8_t st, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void repf(uint8_t st, const char* fmt, ...) {
  if (!g_res) return;
  SelfEntry& e = g_res[g_n];
  e.st = st;
  va_list ap; va_start(ap, fmt);
  vsnprintf(e.det, sizeof(e.det), fmt, ap);
  va_end(ap);
}
static void repAdd(const char* t) { if (g_res) strncat(g_res[g_n].det, t, sizeof(g_res[g_n].det) - strlen(g_res[g_n].det) - 1); }   // aggiunge testo al dettaglio
static const char* yn(bool b) { return b ? tr("si") : tr("no"); }
static const char* onoff(bool b) { return b ? tr("acceso") : tr("spento"); }
// Nascondono parte di MAC e IP se non si vogliono i nomi reali nel report. Scrivono in "out" (capacita cap) e ritornano out.
static const char* maskMac(const char* m, char* out, size_t cap) {
  size_t n = strlen(m);
  if (g_names) snprintf(out, cap, "%s", m);
  else if (n >= 5) snprintf(out, cap, "**:**:**:**:%s", m + n - 5);
  else snprintf(out, cap, "**");
  return out;
}
static const char* maskIp(const char* ip, char* out, size_t cap) {
  if (g_names) { snprintf(out, cap, "%s", ip); return out; }
  const char* a = strchr(ip, '.');
  const char* b = a ? strchr(a + 1, '.') : nullptr;
  if (!b) snprintf(out, cap, "x.x.x.x");
  else snprintf(out, cap, "%.*s.x.x", (int)(b - ip), ip);
  return out;
}
static uint8_t pctSt(int p) { return p > 75 ? SELF_ERR : p > 60 ? SELF_WARN : SELF_OK; }

// ---------- le prove ----------
static void tSys() {
  title(tr("Sistema"));
  const char* r = sysResetText(); uint8_t st = SELF_OK;
  char up[64]; uptimeTo(sysUptimeSec(), up, sizeof up);
  int rr = (int)esp_reset_reason();
  if (rr == 4 || rr == 5 || rr == 6 || rr == 7 || rr == 9) st = SELF_WARN;   // panic, watchdog, brownout
  repf(st, tr("Versione %s, %s rev %u, %u core, flash %u MB, sketch %u KB (liberi %u KB), ultimo reset: %s, avvii %u, acceso da %s"),
    VOS_VERSION, drvChipModel(), (unsigned)drvChipRevision(), (unsigned)drvChipCores(), (unsigned)(drvFlashSize() / 1048576),
    (unsigned)(drvSketchSize() / 1024), (unsigned)(drvFreeSketchSpace() / 1024), r, (unsigned)sysBootCount(), up);
}
static void tRam() {
  title(tr("Memoria RAM"));
  uint32_t tot = drvHeapTotal(), fr = drvHeapFree();
  int used = tot ? (int)(100 - 100ULL * fr / tot) : 0;
  uint8_t st = pctSt(used);
  if (drvHeapMinFree() < 20 * 1024 && st < SELF_WARN) st = SELF_WARN;
  char db[224]; StrBuf d(db, sizeof db);
  d.addf(tr("RAM usata %d%%, libera %u KB, minima %u KB, blocco grande %u KB"), used, (unsigned)(fr / 1024), (unsigned)(drvHeapMinFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
  if (drvPsramTotal()) {
    int pu = (int)(100 - 100ULL * drvPsramFree() / drvPsramTotal());
    d.addf(tr("; PSRAM usata %d%%"), pu);
    if (pctSt(pu) > st) st = pctSt(pu);
  }
  rep(st, d.c_str());
}
static void tCpu() {
  title(tr("CPU e temperatura"));
  int sum = 0, mx = 0, n = 0;
  uint32_t t0 = drvMillis();
  for (int i = 0; i < 5; i++) { int c = sysCpuPercent(); sum += c; if (c > mx) mx = c; n++; vTaskDelay(400 / portTICK_PERIOD_MS); }
  unsigned secs = (unsigned)((drvMillis() - t0 + 500) / 1000);
  int avg = n ? sum / n : 0;
  float tc = sysCpuTemp();
  uint8_t st = avg > 85 ? SELF_ERR : avg > 60 ? SELF_WARN : SELF_OK;
  char db[224]; StrBuf d(db, sizeof db);
  d.addf(tr("CPU media %d%% (massimo %d%%) in %u secondi, %u MHz"), avg, mx, secs, (unsigned)drvCpuMhz());
  if (std::isnan(tc)) { d.add(tr("; temperatura non disponibile")); if (st < SELF_WARN) st = SELF_WARN; }
  else {
    d.addf(tr("; temperatura %d C"), (int)(tc + 0.5f));
    uint8_t ts = tc > 85 ? SELF_ERR : tc > 70 ? SELF_WARN : SELF_OK;
    if (ts > st) st = ts;
  }
  if (st >= SELF_WARN && avg > 60) d.add(tr("; vedi Sistema > Task per il task che consuma di piu"));
  rep(st, d.c_str());
}
static void tFs() {
  title(tr("Flash e file"));
  size_t tot = drvFsTotal(), us = drvFsUsed();
  char pat[161], back[162];                                       // 20 gruppi casuali da 8 cifre esadecimali
  for (int i = 0; i < 20; i++) snprintf(pat + 8 * i, 9, "%08x", (unsigned)drvRandom32());
  const char* path = "/selftest.tmp"; bool ok = false;
  VosFile f = drvFsOpen(path, "w");
  if (f) {
    f.print(pat); f.close();
    VosFile g = drvFsOpen(path, "r");
    if (g) { size_t n = g.read((uint8_t*)back, sizeof(back) - 1); g.close(); back[n] = 0; ok = (strcmp(back, pat) == 0); }
    drvFsRemove(path);
  }
  uint8_t st = ok ? SELF_OK : SELF_ERR;
  if (ok && tot && us * 100 / tot > 85) st = SELF_WARN;
  repf(st, tr("File: %u KB usati su %u KB; scrittura e lettura di prova: %s"), (unsigned)(us / 1024), (unsigned)(tot / 1024), ok ? tr("riuscita") : tr("FALLITA"));
}
static void tCfg() {
  title(tr("Configurazione della scheda"));
  int red = auditCount(AUD_RED), yel = auditCount(AUD_YELLOW);
  uint8_t st = SELF_OK;
  if (!cfgLastSaveOk() || red) st = SELF_ERR; else if (yel || !cfg.setupDone) st = SELF_WARN;
  repf(st, tr("Ultimo salvataggio: %s; guida finita: %s; avvisi rossi %d, gialli %d (scheda Stato conformita)"), cfgLastSaveOk() ? tr("riuscito") : tr("FALLITO"), yn(cfg.setupDone), red, yel);
}
static void tNet() {
  title(tr("Rete Wi-Fi"));
  NetState ns = netState(); uint8_t st = SELF_OK;
  char db[224]; StrBuf d(db, sizeof db);
  if (ns == NET_AIR) { rep(SELF_WARN, tr("Modo aereo: radio spenta")); return; }
  d.addf(tr("Stato %d, modo Wi-Fi %d"), (int)ns, (int)drvWifiMode());
  if (ns == NET_CLIENT_OK) {
    int rs = drvWifiRssi(); char ipm[20], ip[16]; netIpTo(ip, sizeof ip);
    d.addf(tr("; Wi-Fi di casa %s, segnale %d dBm, IP %s"), g_names ? drvWifiSsid().s : "***", rs, maskIp(ip, ipm, sizeof(ipm)));
    if (rs < -85) st = SELF_ERR; else if (rs < -75) st = SELF_WARN;
  } else if (cfg.staEnabled) { d.add("; ").add(tr("Wi-Fi di casa configurata ma NON collegata")); st = SELF_WARN; }
  if (cfg.apOn) d.addf(tr("; hotspot con %d client"), (int)drvWifiApClients());
  d.addf(tr("; DHCP %s, DNS %s, mDNS %s"), onoff(cfg.dhcpOn), onoff(netCaptive()), onoff(cfg.mdnsOn));
  { char mac[20], mm[24]; netMacTo(false, mac, sizeof mac); d.addf(tr("; MAC %s"), maskMac(mac, mm, sizeof(mm))); }
  rep(st, d.c_str());
}
// MQTT, ESP-NOW e Bluetooth: lo stato arriva dal registro dei servizi (acceso = non SVC_OFF; MQTT collegato = SVC_ON)
static bool svcRun(const char* id) { return serviceState(id) != SVC_OFF; }

static void tWeb() {
  title(tr("Pagina web"));
  bool https = hookWebHttpsUp(); uint8_t st = SELF_OK;
  if (!cfg.httpOn && !cfg.https) st = SELF_ERR;
  else if (cfg.https && !https) st = tlsPending() ? SELF_WARN : SELF_ERR;
  else if (!cfg.https) st = SELF_WARN;
  auto fpAll = tlsFingerprint(); char fp[9]; snprintf(fp, sizeof fp, "%.8s", fpAll);   // prime 8 cifre dell'impronta
  char pend[64] = ""; if (tlsPending()) snprintf(pend, sizeof pend, " %s", tr("(nuovo in attesa del riavvio)"));
  repf(st, tr("HTTP %s porta %u; HTTPS %s porta %u (in funzione: %s); certificato %s, impronta %s...%s"), onoff(cfg.httpOn), (unsigned)cfg.httpPort, onoff(cfg.https), (unsigned)cfg.httpsPort, yn(https),
    tlsCustom() ? tr("caricato da te") : tr("autofirmato dalla scheda"), fp, pend);
}
static void tTime() {
  title(tr("Ora e data"));
  bool v = timeValid(); uint32_t last = timeLastSync(); uint8_t st = SELF_OK;
  if (!v) st = SELF_WARN; else if (cfg.ntpOn && !last && netState() == NET_CLIENT_OK) st = SELF_WARN;
  char now[32]; timeNowFmt(now, sizeof now);
  repf(st, tr("Ora valida: %s; NTP %s; ultima sincronizzazione: %s; fuso %s"), yn(v), onoff(cfg.ntpOn), last ? now : tr("mai"), cfg.tzName.length() ? cfg.tzName.c_str() : "-");
}
static void tSvc() {
  title(tr("Servizi attivi"));
  repf(SELF_OK, tr("Hotspot %s, DHCP %s, HTTP %s, HTTPS %s, MQTT %s (collegato: %s), ESP-NOW %s, Bluetooth %s, NTP %s, energia modo %d, statistiche %s"),
    onoff(cfg.apOn), onoff(cfg.dhcpOn), onoff(cfg.httpOn), onoff(hookWebHttpsUp()), onoff(svcRun("mqtt")), yn(serviceState("mqtt") == SVC_ON), onoff(svcRun("mesh")), onoff(svcRun("ble")), onoff(cfg.ntpOn), (int)cfg.pwMode, onoff(cfg.statOn));
}
static void tTasks() {
  title(tr("Task e watchdog"));
  uint32_t me = uxTaskGetStackHighWaterMark(NULL), lp = 0;
  TaskHandle_t h = xTaskGetHandle("loopTask"); if (h) lp = uxTaskGetStackHighWaterMark(h);
  uint8_t st = SELF_OK; if (me < 600 || (h && lp < 600)) st = SELF_WARN;
  repf(st, tr("Task %u; stack libero minimo: prova %u B, loop %u B; watchdog task %s, rete %s, RAM %s; task led %s"),
    (unsigned)uxTaskGetNumberOfTasks(), (unsigned)me, (unsigned)lp, onoff(cfg.wdTask), onoff(cfg.wdNet), onoff(cfg.wdRam), yn(sysTaskRunning("led")));
}
static void tSec() {
  title(tr("Sicurezza e accessi"));
  int adm = 0, mfa = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) { adm++; if (hookMfaOn(i)) mfa++; }
  uint8_t st = SELF_OK;
  if (!authIsSet()) st = SELF_ERR; else if (!cfg.https || !cfg.serialAuth || !cfg.powBits || cfg.banFails > 10) st = SELF_WARN;
  repf(st, tr("Password impostata: %s; HTTPS %s; seriale protetta: %s; prova di lavoro al login %u bit; blocco IP dopo %u errori; amministratori %d (con MFA %d)"),
    yn(authIsSet()), onoff(cfg.https), yn(cfg.serialAuth), (unsigned)cfg.powBits, (unsigned)cfg.banFails, adm, mfa);
}
static void tRadio() {
  title(tr("Radio e antenna"));
  uint8_t st = SELF_OK; int ch = drvWifiChannel();
  if (!cfg.country.length()) st = SELF_WARN;
  if (regionTxDbm() > regionMaxDbm() || (ch > 0 && !regionChannelOk(ch))) st = SELF_ERR;
  repf(st, tr("Paese %s; canali 1-%d (in uso %d); potenza %d dBm (massimo %d, limite del paese %d); antenna %s"),
    cfg.country.length() ? cfg.country.c_str() : tr("NON scelto"), regionChannels(), ch, regionTxDbm(), regionMaxDbm(), regionLimitDbm(), cfg.antExt ? tr("esterna") : tr("interna"));
}
static void tPer() {
  title(tr("Periferiche e tasto BOOT"));
  bool boot = !drvGpioRead(VOS_PIN_BOOT);
  repf(boot ? SELF_WARN : SELF_OK, tr("LED sul pin %u, modo %u; pin registrati %d; tasto BOOT %s"), (unsigned)cfg.ledPin, (unsigned)cfg.ledMode, pinCount(), boot ? tr("PREMUTO") : tr("rilasciato"));
#if VOS_WITH_SD
  if (sdConfigured()) {                                             // senza periferica "sd" in io.cfg non si dice nulla
  char sd[112]; sdText(sd, sizeof sd);                              // non montata non e un errore: si monta a richiesta
  repAdd("; "); repAdd(sd);
  if (g_active && sdUp()) {                                         // prova attiva: scrive e rilegge un file sulla microSD (subito cancellato)
    char o[96], e[96];
    if (sdProve(o, sizeof o, e, sizeof e)) { char m[48]; snprintf(m, sizeof m, tr("; prova microSD %s"), tr("riuscita")); repAdd(m); }
    else { repAdd("; "); repAdd(e); if (g_res) g_res[g_n].st = SELF_WARN; }
  }
  }
#endif
}
// quante volte compare 'ago' in 'testo', senza distinguere maiuscole e minuscole (ago in minuscolo)
static int countNoCase(const std::string& testo, const char* ago) {
  size_t m = strlen(ago); int k = 0;
  for (size_t i = 0; i + m <= testo.size(); i++) {
    size_t j = 0;
    while (j < m && tolower((unsigned char)testo[i + j]) == ago[j]) j++;
    if (j == m) { k++; i += m - 1; }
  }
  return k;
}
static void tLog() {
  title(tr("Registro della scheda"));
  std::string l = logGet(60);
  int n = 0;
  for (char ch : l) if (ch == '\n') n++;
  int bad = countNoCase(l, "errore") + countNoCase(l, "watchdog");
  repf(bad ? SELF_WARN : SELF_OK, tr("Righe nel registro: %d; righe con errore o watchdog: %d"), n, bad);
}
static void tActLed() {
  title(tr("LED (prova attiva)"));
  if (!g_active) { rep(SELF_SKIP, tr("Solo con le prove attive")); return; }
  ledIdentify(2000); rep(SELF_OK, tr("Il LED fa l'arcobaleno per 2 secondi: guardalo"));
}
static void tActMqtt() {
  title(tr("MQTT (prova attiva)"));
  if (!g_active) { rep(SELF_SKIP, tr("Solo con le prove attive")); return; }
  if (!svcRun("mqtt")) { rep(SELF_SKIP, tr("MQTT fermo")); return; }
  if (serviceState("mqtt") != SVC_ON) { rep(SELF_WARN, tr("MQTT avviato ma non collegato al broker")); return; }
  bool ok = hookMqttPublish("selftest", "ok");
  rep(ok ? SELF_OK : SELF_ERR, ok ? tr("Messaggio di prova inviato al broker") : tr("Invio del messaggio di prova fallito"));
}

typedef void (*SelfFn)();
static const SelfFn TESTS[NT] = { tSys, tRam, tCpu, tFs, tCfg, tNet, tWeb, tTime, tSvc, tTasks, tSec, tRadio, tPer, tLog, tActLed, tActMqtt };

static void selfTask(void*) {
  uint32_t t0 = drvMillis();
  for (int i = 0; i < NT; i++) {
    g_n = i;
    uint32_t a = drvMillis();
    TESTS[i]();
    g_res[i].ms = (uint16_t)std::min<uint32_t>(65000, drvMillis() - a);
    vTaskDelay(30 / portTICK_PERIOD_MS);      // cede il processore: nessuna prova pesa piu di una frazione di secondo
  }
  g_totMs = drvMillis() - t0;
  g_state = 2;
  vTaskDelete(NULL);
}

// ---------- interfaccia ----------
bool selftestRunning() { return g_state == 1; }
int  selftestTotal() { return NT; }
int  selftestDone() { return g_state == 2 ? NT : (g_state == 1 ? (int)g_n : 0); }
void selftestClear() { if (g_state != 1) { free(g_res); g_res = nullptr; g_state = 0; } }

bool selftestStart(bool active, bool names, ErrBuf& err) {
  if (g_state == 1) { err = tr("Una prova e gia in corso"); return false; }
  if (drvHeapFree() < 30 * 1024) { err = tr("RAM troppo bassa per la diagnosi: chiudi le altre pagine e riprova"); return false; }
  free(g_res);
  g_res = (SelfEntry*)drvMallocBig(NT * sizeof(SelfEntry));   // in PSRAM se c'e: risparmia RAM interna
  if (g_res) memset(g_res, 0, NT * sizeof(SelfEntry));
  if (!g_res) { err = tr("Memoria insufficiente"); return false; }
  g_active = active; g_names = names; g_n = 0; g_totMs = 0;
  char now[32]; timeNowFmt(now, sizeof now);
  snprintf(g_when, sizeof(g_when), "%s", timeValid() ? now : tr("ora non valida"));
  g_state = 1;
  if (!drvTaskCreate(selfTask, "selftest", 5120, NULL, 1, NULL, DRV_CORE_ANY)) {   // core: automatico (prima: core 1 fisso)
    g_state = 0; free(g_res); g_res = nullptr; err = tr("Impossibile avviare la prova"); return false; }
  vlog("DIAGNOSI: avviata (prove attive %s)", active ? "si" : "no");
  return true;
}

static const char* stTag(uint8_t st) { return st == SELF_OK ? "OK" : st == SELF_WARN ? "AVVISO" : st == SELF_ERR ? "ERRORE" : st == SELF_SKIP ? "SALTATA" : "..."; }
static bool ready(int i) { return g_res && i >= 0 && i < NT && g_res[i].st != SELF_PEND && (g_state == 2 || i < g_n); }

// "[OK] Nome: dettaglio" (al piu ~265 caratteri: tag, nome da 27, dettaglio da 223): un solo blocco di heap.
std::string selftestLine(int i) {
  if (!ready(i)) return std::string();
  StrText t(288);
  t.buf().addf("[%s] %s: %s", stTag(g_res[i].st), g_res[i].name, g_res[i].det);
  return t.take();
}

bool selftestJson(HeapBuf& j) {
  char t[200];
  snprintf(t, sizeof t, "{\"state\":%d,\"total\":%d,\"cur\":%d,\"active\":%s,\"names\":%s,\"ms\":%lu,\"list\":[",
           (int)g_state, NT, (int)(g_state == 1 ? g_n : -1), g_active ? "true" : "false", g_names ? "true" : "false", (unsigned long)g_totMs);
  j.add(t);
  bool first = true;
  for (int i = 0; i < NT; i++) {
    if (!ready(i)) continue;
    if (!first) j.add(',');
    first = false;
    snprintf(t, sizeof t, "{\"i\":%d,\"n\":\"", i);
    j.add(t); j.addJson(g_res[i].name, strlen(g_res[i].name));
    snprintf(t, sizeof t, "\",\"s\":%d,\"ms\":%d,\"d\":\"", (int)g_res[i].st, (int)g_res[i].ms);
    j.add(t); j.addJson(g_res[i].det, strlen(g_res[i].det));
    j.add("\"}");
  }
  return j.add("]}");
}

// aggiunge una riga al report (printf; una riga e lunga al massimo ~300 caratteri)
static void rl(HeapBuf& r, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static void rl(HeapBuf& r, const char* fmt, ...) {
  char t[320];
  va_list ap; va_start(ap, fmt);
  vsnprintf(t, sizeof t, fmt, ap);
  va_end(ap);
  r.add(t);
}

// false = non c'e un report (prova non finita) oppure manca la memoria; il report si controlla con out.oom()/over()
bool selftestReport(HeapBuf& r) {
  if (!g_res || g_state != 2) return false;
  int c[5] = {0, 0, 0, 0, 0};
  for (int i = 0; i < NT; i++) c[g_res[i].st < 5 ? g_res[i].st : 0]++;
  char mac[20], id[5] = "----"; netMacTo(false, mac, sizeof mac);
  if (strlen(mac) >= 17) snprintf(id, sizeof id, "%c%c%c%c", mac[12], mac[13], mac[15], mac[16]);   // ultimi 2 byte del MAC
  r.add("VesevOS - REPORT AUTODIAGNOSI\n");
  rl(r, tr("Versione: %s   Scheda: %s (ID %s)   Data: %s"), VOS_VERSION, VOS_BOARD, id, g_when); r.add('\n');
  rl(r, tr("Nomi e dati: %s   Prove attive: %s   Durata: %u ms"), g_names ? tr("REALI") : tr("oscurati"), yn(g_active), (unsigned)g_totMs); r.add('\n');
  rl(r, tr("Riepilogo: %d OK, %d avvisi, %d errori, %d saltate"), c[SELF_OK], c[SELF_WARN], c[SELF_ERR], c[SELF_SKIP]); r.add('\n');
  r.add("------------------------------------------------------------\n");
  for (int i = 0; i < NT; i++) {
    rl(r, "%02d [%s] %s (%d ms)\n    %s\n", i + 1, stTag(g_res[i].st), g_res[i].name, (int)g_res[i].ms, g_res[i].det);
  }
  r.add("------------------------------------------------------------\n");
  r.add(tr("Diario dei riavvii:")); r.add('\n'); r.add(diaryText(8, g_names).c_str()); r.add('\n');
  if (g_names) {
    r.add(tr("Ultime righe del registro:")); r.add('\n'); r.add(logGet(20).c_str()); r.add('\n');
  } else {
    r.add(tr("Registro omesso (puo contenere indirizzi e nomi). Per includerlo rifai la prova con i nomi reali.")); r.add('\n');
  }
  r.add(tr("Questo report non contiene password, chiavi o token.")); r.add('\n');
  return !r.oom() && !r.over();
}
