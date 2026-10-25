// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.cpp
#include "vos_config.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/vos_drv_sys.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_nvs.h"
#include "vos_config_int.h"

#define CFG_FILE "/vesevos.conf"
#define CFG_TMP  "/vesevos.conf.tmp"
#define CFG_BAK  "/vesevos.conf.bak"

VosConfig cfg;

void cfgDefaults() {
  cfg.hostname = "vesevos"; cfg.domain = ""; cfg.lang = "it";
  cfg.setupDone = false;
  cfg.apSsid = "VesevOS";
  cfg.serialAuth = true; cfg.serialIdleMin = 0;
  cfg.serBaud = 115200; cfg.serEol = 0; cfg.serTx = 10; cfg.serEcho = true; cfg.serIn = true; cfg.serLogOut = true; cfg.serBanner = true;
  cfg.apQr = true;
  cfg.wpsOn = false; cfg.wpsPin = false;
  cfg.btnAct[0] = 1; cfg.btnAct[1] = 2; cfg.btnAct[2] = 0;   // 1 click: esce dal modo aereo; 2 click: WPS; 3 click: niente
  for (int i = 0; i < VOS_GPIO_COUNT; i++) cfg.pinNote[i] = "";
  cfg.apPass = "";                          // creata al primo avvio, unica per ogni scheda (vedi cfgLoad)
  cfg.staEnabled = false;
  cfg.staSsid = ""; cfg.staPass = "";
  cfg.staDhcp = true;
  cfg.ip = "192.168.1.50"; cfg.mask = "255.255.255.0";
  cfg.gw = "192.168.1.1"; cfg.dns1 = "192.168.1.1"; cfg.dns2 = "8.8.8.8";
  cfg.authSalt = ""; cfg.authHash = "";     // solo per i file vecchi
  for (int i = 0; i < VOS_MAX_USERS; i++) { cfg.users[i].name = ""; cfg.users[i].salt = ""; cfg.users[i].hash = ""; cfg.users[i].role = ROLE_GUEST; cfg.users[i].on = false; cfg.users[i].mfa = ""; cfg.users[i].mfaOn = false; cfg.users[i].mfaLast = 0; cfg.users[i].rec = ""; }
  cfg.ledMode = LED_STATE;
  cfg.ledColor = 0x0000FF;
  cfg.ledBrightness = 40;
  cfg.ledPin = VOS_PIN_LED_RGB;
  for (int i = 0; i < LEDSRC_COUNT; i++) cfg.ledSrcColor[i] = kLedSrcDefault[i];
  cfg.ledCpuThr = LED_CPU_THR_DEFAULT;
  cfg.banFails = 5; cfg.banSecs = 60; cfg.powBits = 14; cfg.mfaNoTime = 0;
  cfg.mqttAuto = false; cfg.mqttHost = ""; cfg.mqttUser = ""; cfg.mqttPass = ""; cfg.mqttPrefix = ""; cfg.mqttPort = 1883; cfg.mqttEvery = 30; cfg.mqttHa = true; cfg.mqttTls = false;
  cfg.airOn = 0; cfg.airExit = 0; cfg.airUntil = 0; cfg.airAt = 0;
  cfg.ntpOn = true; cfg.ntpServe = false; cfg.ntpAutoOff = false; cfg.ntpEvery = 60;
  cfg.ntpServer = "pool.ntp.org"; cfg.ntpServer2 = "";   // nessun secondo server di fabbrica (niente Google)
  cfg.tz = "CET-1CEST,M3.5.0,M10.5.0/3"; cfg.tzName = "Europe/Rome";
  cfg.cpuMhz = 0; cfg.cpuGov = 0; cfg.cpuUp = 80; cfg.cpuDown = 20; cfg.cpuHold = 10; cfg.logLevel = 2; cfg.statOn = false; cfg.pwMode = 0; cfg.pwAwake = 15; cfg.pwSleep = 10;
  cfg.dateFmt = 0; cfg.timeFmt = 0; cfg.tempUnit = 0; cfg.weekStart = 0; cfg.decSep = 0;
  cfg.country = ""; cfg.antExt = 0; cfg.antGain = 0; cfg.txDbm = 0;
  cfg.fwMode = 0; cfg.fwNtp = false; cfg.fwN = 0;
  cfg.bleOn = false;
  cfg.meshAuto = false; cfg.meshRole = 0; cfg.meshKey = ""; cfg.meshCh = 1;
  cfg.https = true;
  cfg.apOn = true; cfg.apCaptive = true; cfg.httpOn = true; cfg.httpPort = 80; cfg.httpsPort = 443; cfg.httpRedirect = false;
  cfg.dhcpOn = true; cfg.dhcpLease = 120; cfg.mdnsOn = true;
  cfg.wdTask = true; cfg.wdNet = true; cfg.wdRam = true; cfg.wdNetMin = 10; cfg.wdRamKb = 20; cfg.wdUpDays = 0; cfg.wdAt = -1; cfg.wdDays = 0x7F;
  cfg.wdTwdt = WD_TWDT_DEF;
  for (int i = 0; i < WD_LIM_N; i++) cfg.wdLim[i] = WD_LIMITS[i].def;
  cfg.sdEnabled = false;
  cfg.sdCs = 10; cfg.sdSck = 12; cfg.sdMiso = 13; cfg.sdMosi = 11;
}


// ---- chi cambia (per il registro delle modifiche) ----
static char g_origin[64] = "sistema";
void fwRuleSetName(FwRule& r, const char* text, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len && n < FW_NAME_MAX; i++) {
    unsigned char c = (unsigned char)text[i];
    if (c == '\r') continue;
    r.name[n++] = (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? (char)c : '?';
  }
  r.name[n] = 0;
}

void cfgSetOrigin(const char* who) { snprintf(g_origin, sizeof g_origin, "%s", who ? who : ""); }
void cfgSetOriginf(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(g_origin, sizeof g_origin, fmt, ap);
  va_end(ap);
}

// ---- impronta del file salvato (per accorgersi di modifiche fatte fuori dal pannello) ----
#define HASH_HEX 65     // SHA-256 in esadecimale + zero finale

// Legge tutto il file in coda a out (a pezzi di 256 byte). false se il file manca o non entra nel tetto di out.
static bool readFile(const char* path, HeapBuf& out) {
  VosFile f = drvFsOpen(path, "r");
  if (!f) return false;
  char chunk[256];
  int n;
  bool ok = true;
  while (ok && (n = f.read((uint8_t*)chunk, sizeof chunk)) > 0) ok = out.add(chunk, (size_t)n);
  f.close();
  return ok;
}

static bool fileHash(const char* path, char out[HASH_HEX]) {
  HeapBuf t(CFG_TEXT_MAX);
  if (!readFile(path, t)) return false;
  sha256HexOf(std::string_view(t.c_str(), t.length()), out);
  return true;
}
static void hashSave(const char* h) { DrvNvs p; if (p.begin("vos", false)) { p.putString("cfgh", h); p.end(); } }
bool cfgFileChanged() {
  if (!drvFsExists(CFG_FILE)) return false;
  DrvNvs p; char saved[HASH_HEX + 16] = "";
  if (p.begin("vos", true)) { p.getString("cfgh", saved, sizeof saved); p.end(); }
  if (!saved[0]) return false;                       // prima volta: niente da confrontare
  char now[HASH_HEX] = "";
  fileHash(CFG_FILE, now);                           // file illeggibile: impronta vuota, quindi "cambiato" (come prima)
  return strcmp(now, saved) != 0;
}

static HeapBuf g_lastPub(CFG_TEXT_MAX);   // ultimo testo salvato SENZA password (per il registro delle modifiche)

static bool g_saveOk = true;
bool cfgLastSaveOk() { return g_saveOk; }
static bool saveFail(const char* why) { vlog("CFG: %s", why); g_saveOk = false; auditRefresh(); return false; }

// F2 (1.7.9a): la guida senza Wi-Fi spegne NTP e lo segna come "spento dalla guida".
// Quando poi arriva il Wi-Fi di casa (shell, pagina, Bluetooth) NTP si riaccende da solo.
// Se invece l'utente lo ha spento lui, resta spento.
bool cfgNtpAfterWifi() {
  if (!cfg.ntpAutoOff) return false;
  cfg.ntpAutoOff = false; cfg.ntpOn = true;
  vlog("TIME: NTP acceso (Wi-Fi di casa impostata): contatta %s%s%s; ogni richiesta mostra il tuo IP pubblico, per spegnere: ntp off", cfg.ntpServer.c_str(), cfg.ntpServer2.empty() ? "" : " e ", cfg.ntpServer2.c_str());
  return true;
}

bool cfgSave() {
  auditFix();                                        // valori fuori regola: corretti prima di salvare
  HeapBuf s(CFG_TEXT_MAX);
  if (!cfgExport(s, true)) return saveFail("memoria insufficiente per il testo");
  VosFile f = drvFsOpen(CFG_TMP, "w");
  if (!f) return saveFail("errore apertura tmp");
  size_t w = f.write((const uint8_t*)s.c_str(), s.length());
  f.close();
  if (w != s.length()) { drvFsRemove(CFG_TMP); return saveFail("scrittura incompleta (memoria piena?)"); }
  if (drvFsExists(CFG_FILE)) {
    drvFsRemove(CFG_BAK);
    drvFsRename(CFG_FILE, CFG_BAK);
  }
  if (!drvFsRename(CFG_TMP, CFG_FILE)) return saveFail("rename fallito");
  g_saveOk = true;
  char h[HASH_HEX];
  sha256HexOf(std::string_view(s.c_str(), s.length()), h);
  hashSave(h);
  HeapBuf pub(CFG_TEXT_MAX);
  if (cfgExport(pub, false)) {
    if (g_lastPub.length()) auditDiff(g_lastPub.c_str(), pub.c_str(), g_origin);
    g_lastPub.clear(); g_lastPub.add(pub.c_str(), pub.length());
  }
  auditRefresh();
  return true;
}

static bool loadFile(const char* path) {
  HeapBuf t(CFG_TEXT_MAX);
  if (!readFile(path, t)) return false;
  ErrBuf err;
  if (!cfgImport(t.c_str(), t.length(), err)) { vlog("CFG: %s: %s", path, err.c_str()); return false; }
  return true;
}

// Password casuale per l'hotspot: 12 caratteri senza simboli che si confondono, a gruppi di 4 ("k7Hm-pQ4x-Tr9a")
FixedStr<16> cfgNewApPass() {
  static const char* A = "abcdefghjkmnpqrstuvwxyz23456789";   // solo minuscole e cifre, senza lettere simili (facile da scrivere sul telefono)
  size_t n = strlen(A);
  FixedStr<16> p;
  char t[13];
  for (int i = 0; i < 12; i++) {
    uint32_t r;
    do { r = drvRandom32() & 0xFF; } while (r >= (256 / n) * n);   // nessuna preferenza per alcune lettere
    t[i] = A[r % n];
  }
  t[12] = 0;
  p = t;
  return p;
}

bool cfgApPassWeak(const char* p) {
  return strlen(p) < 8 || !strcmp(p, "vesevos123") || !strcmp(p, "12345678") || !strcmp(p, "password") || !strcmp(p, "vesevos1");
}

bool cfgLoad() {
  cfgDefaults(); g_cfgHadSetupKey = false;
  bool found = loadFile(CFG_FILE);
  if (!found && loadFile(CFG_BAK)) { vlog("CFG: ripristinato da .bak"); found = true; }
  // file della 1.7.0: la password diventa l'utente "admin" e la scheda risulta gia configurata
  bool any = false;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (!cfg.users[i].name.empty()) any = true;
  if (!any && cfg.authHash.length() == 64 && !cfg.authSalt.empty()) {
    cfg.users[0].name = "admin"; cfg.users[0].salt = cfg.authSalt; cfg.users[0].hash = cfg.authHash;
    cfg.users[0].role = ROLE_ADMIN; cfg.users[0].on = true;
    cfg.setupDone = true;
    vlog("CFG: password della versione precedente -> utente 'admin'");
  }
  cfg.authSalt = ""; cfg.authHash = "";
  // file di prima della 1.7.1 (senza la riga "setup") con almeno un utente e il Wi-Fi di casa: gia in uso, la guida non serve.
  // Dalla 1.7.2 il Wi-Fi salvato da solo NON basta piu: la guida deve essere finita davvero.
  if (found && !g_cfgHadSetupKey && !cfg.setupDone && cfg.staEnabled && !cfg.staSsid.empty()) {
    for (int i = 0; i < VOS_MAX_USERS; i++) if (!cfg.users[i].name.empty() && !cfg.users[i].hash.empty()) { cfg.setupDone = true; break; }
  }
  // sicurezza: ne HTTP ne HTTPS acceso, o stessa porta -> si torna ai valori di fabbrica
  if ((!cfg.httpOn && !cfg.https) || cfg.httpPort == cfg.httpsPort) { cfgSvcRecover(); vlog("CFG: servizi web non validi, valori di fabbrica"); }
  // guida non finita (primo avvio o reset): hotspot, HTTP, HTTPS e porte standard, cosi si entra sempre
  if (!cfg.setupDone) cfgSvcRecover();
  // hotspot: mai una password uguale per tutti (leggi UE RED/EN 18031, CRA; Regno Unito PSTI)
  if (cfgApPassWeak(cfg.apPass.c_str())) {
    cfg.apPass = cfgNewApPass().c_str();
    vlog("CFG: nuova password unica per l'hotspot (vedi la schermata di benvenuto sulla seriale)");
    cfgSetOrigin("primo avvio");
    cfgSave();
  }
  g_lastPub.clear();
  cfgExport(g_lastPub, false);
  return found;
}

bool cfgSvcRecover() {
  bool ch = !cfg.apOn || !cfg.apCaptive || !cfg.dhcpOn || !cfg.httpOn || !cfg.https || cfg.httpPort != 80 || cfg.httpsPort != 443;
  cfg.apOn = true; cfg.apCaptive = true; cfg.dhcpOn = true; cfg.httpOn = true; cfg.https = true; cfg.httpPort = 80; cfg.httpsPort = 443; cfg.httpRedirect = false;   // ripristino: nessun reindirizzamento, si entra sempre
  return ch;
}

const char* cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort) {
  if (!httpOn && !httpsOn) return tr("Deve restare acceso almeno un protocollo web (HTTP o HTTPS), altrimenti resti chiuso fuori");
  if (httpOn && httpPort != 80 && (httpPort < 1024 || httpPort > 65535)) return tr("Porta HTTP non valida: 80 oppure da 1024 a 65535");
  if (httpsOn && httpsPort != 443 && (httpsPort < 1024 || httpsPort > 65535)) return tr("Porta HTTPS non valida: 443 oppure da 1024 a 65535");
  if (httpOn && httpsOn && httpPort == httpsPort) return tr("HTTP e HTTPS non possono usare la stessa porta");
  if (!apOn && !(cfg.staEnabled && !cfg.staSsid.empty())) return tr("Per spegnere il Punto di accesso serve prima la Wi-Fi di casa configurata");
  return nullptr;
}

void cfgFactoryReset() {
  drvFsRemove(CFG_FILE);
  drvFsRemove(CFG_BAK);
  drvFsRemove(CFG_TMP);
  drvFsRemove("/mqtt-ca.pem");
  DrvNvs p;
  if (p.begin("vos", false)) { p.remove("cfgh"); p.remove("audit"); p.end(); }
  if (p.begin("tls", false)) { p.clear(); p.end(); }
  cfgDefaults();
}
