// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.cpp
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "../drivers/vos_drv_fs.h"
#include <Preferences.h>
#include "vos_config_int.h"

#define CFG_FILE "/vesevos.conf"
#define CFG_TMP  "/vesevos.conf.tmp"
#define CFG_BAK  "/vesevos.conf.bak"

VosConfig cfg;

void cfgDefaults() {
  cfg.hostname = "vesevos"; cfg.domain = ""; cfg.lang = "it";
  cfg.setupDone = false;
  cfg.apSsid = "VesevOS";
  cfg.serialAuth = true;
  cfg.serBaud = 115200; cfg.serEol = 0; cfg.serTx = 10; cfg.serEcho = true; cfg.serIn = true; cfg.serLogOut = true; cfg.serBanner = true;
  cfg.apQr = true;
  for (int i = 0; i < 49; i++) cfg.pinNote[i] = "";
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
  cfg.banFails = 5; cfg.banSecs = 60; cfg.powBits = 14; cfg.mfaNoTime = 0;
  cfg.mqttAuto = false; cfg.mqttHost = ""; cfg.mqttUser = ""; cfg.mqttPass = ""; cfg.mqttPrefix = ""; cfg.mqttPort = 1883; cfg.mqttEvery = 30; cfg.mqttHa = true; cfg.mqttTls = false;
  cfg.airOn = 0; cfg.airExit = 0; cfg.airUntil = 0; cfg.airAt = 0;
  cfg.ntpOn = true; cfg.ntpServe = false; cfg.ntpAutoOff = false; cfg.ntpEvery = 60;
  cfg.ntpServer = "pool.ntp.org"; cfg.ntpServer2 = "";   // nessun secondo server di fabbrica (niente Google)
  cfg.tz = "CET-1CEST,M3.5.0,M10.5.0/3"; cfg.tzName = "Europe/Rome";
  cfg.cpuMhz = 0; cfg.logLevel = 2; cfg.statOn = false; cfg.pwMode = 0; cfg.pwAwake = 15; cfg.pwSleep = 10;
  cfg.dateFmt = 0; cfg.timeFmt = 0; cfg.tempUnit = 0; cfg.weekStart = 0; cfg.decSep = 0;
  cfg.country = ""; cfg.antExt = 0; cfg.antGain = 0; cfg.txDbm = 0;
  cfg.fwMode = 0; cfg.fwNtp = false; cfg.fwN = 0;
  cfg.meshAuto = false; cfg.meshRole = 0; cfg.meshKey = ""; cfg.meshCh = 1;
  cfg.https = true;
  cfg.apOn = true; cfg.apCaptive = true; cfg.httpOn = true; cfg.httpPort = 80; cfg.httpsPort = 443;
  cfg.dhcpOn = true; cfg.dhcpLease = 120; cfg.mdnsOn = true;
  cfg.wdTask = true; cfg.wdNet = true; cfg.wdRam = true; cfg.wdNetMin = 10; cfg.wdRamKb = 20; cfg.wdUpDays = 0; cfg.wdAt = -1; cfg.wdDays = 0x7F;
  cfg.sdEnabled = false;
  cfg.sdCs = 10; cfg.sdSck = 12; cfg.sdMiso = 13; cfg.sdMosi = 11;
}


// ---- chi cambia (per il registro delle modifiche) ----
static String g_origin = "sistema";
void cfgSetOrigin(const String& who) { g_origin = who; }
String cfgOrigin() { return g_origin; }

// ---- impronta del file salvato (per accorgersi di modifiche fatte fuori dal pannello) ----
static String fileHash(const char* path) {
  File f = drvFsOpen(path, "r");
  if (!f) return "";
  String t = f.readString(); f.close();
  return sha256Hex(t);
}
static void hashSave(const String& h) { Preferences p; if (p.begin("vos", false)) { p.putString("cfgh", h); p.end(); } }
bool cfgFileChanged() {
  if (!drvFsExists(CFG_FILE)) return false;
  Preferences p; String saved;
  if (p.begin("vos", true)) { saved = p.getString("cfgh", ""); p.end(); }
  if (!saved.length()) return false;                 // prima volta: niente da confrontare
  return fileHash(CFG_FILE) != saved;
}

static String g_lastPub;     // ultimo testo salvato SENZA password (per il registro delle modifiche)

static bool g_saveOk = true;
bool cfgLastSaveOk() { return g_saveOk; }
static bool saveFail(const char* why) { vlog("CFG: %s", why); g_saveOk = false; auditRefresh(); return false; }

// F2 (1.7.9a): la guida senza Wi-Fi spegne NTP e lo segna come "spento dalla guida".
// Quando poi arriva il Wi-Fi di casa (shell, pagina, Bluetooth) NTP si riaccende da solo.
// Se invece l'utente lo ha spento lui, resta spento.
bool cfgNtpAfterWifi() {
  if (!cfg.ntpAutoOff) return false;
  cfg.ntpAutoOff = false; cfg.ntpOn = true;
  vlog("TIME: NTP acceso (Wi-Fi di casa impostata): contatta %s%s%s; ogni richiesta mostra il tuo IP pubblico, per spegnere: ntp off", cfg.ntpServer.c_str(), cfg.ntpServer2.length() ? " e " : "", cfg.ntpServer2.c_str());
  return true;
}

bool cfgSave() {
  auditFix();                                        // valori fuori regola: corretti prima di salvare
  String s = cfgExport(true);
  File f = drvFsOpen(CFG_TMP, "w");
  if (!f) return saveFail("errore apertura tmp");
  size_t w = f.print(s);
  f.close();
  if (w != s.length()) { drvFsRemove(CFG_TMP); return saveFail("scrittura incompleta (memoria piena?)"); }
  if (drvFsExists(CFG_FILE)) {
    drvFsRemove(CFG_BAK);
    drvFsRename(CFG_FILE, CFG_BAK);
  }
  if (!drvFsRename(CFG_TMP, CFG_FILE)) return saveFail("rename fallito");
  g_saveOk = true;
  hashSave(sha256Hex(s));
  String pub = cfgExport(false);
  if (g_lastPub.length()) auditDiff(g_lastPub, pub, g_origin);
  g_lastPub = pub;
  auditRefresh();
  return true;
}

static bool loadFile(const char* path) {
  File f = drvFsOpen(path, "r");
  if (!f) return false;
  String t = f.readString();
  f.close();
  String err;
  if (!cfgImport(t, err)) { vlog("CFG: %s: %s", path, err.c_str()); return false; }
  return true;
}

// Password casuale per l'hotspot: 12 caratteri senza simboli che si confondono, a gruppi di 4 ("k7Hm-pQ4x-Tr9a")
String cfgNewApPass() {
  static const char* A = "abcdefghjkmnpqrstuvwxyz23456789";   // solo minuscole e cifre, senza lettere simili (facile da scrivere sul telefono)
  size_t n = strlen(A);
  String p;
  for (int i = 0; i < 12; i++) {
    uint32_t r;
    do { r = esp_random() & 0xFF; } while (r >= (256 / n) * n);   // nessuna preferenza per alcune lettere
    p += A[r % n];
  }
  return p;
}

bool cfgApPassWeak(const String& p) {
  return p.length() < 8 || p == "vesevos123" || p == "12345678" || p == "password" || p == "vesevos1";
}

bool cfgLoad() {
  cfgDefaults(); g_cfgHadSetupKey = false;
  bool found = loadFile(CFG_FILE);
  if (!found && loadFile(CFG_BAK)) { vlog("CFG: ripristinato da .bak"); found = true; }
  // file della 1.7.0: la password diventa l'utente "admin" e la scheda risulta gia configurata
  bool any = false;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length()) any = true;
  if (!any && cfg.authHash.length() == 64 && cfg.authSalt.length()) {
    cfg.users[0].name = "admin"; cfg.users[0].salt = cfg.authSalt; cfg.users[0].hash = cfg.authHash;
    cfg.users[0].role = ROLE_ADMIN; cfg.users[0].on = true;
    cfg.setupDone = true;
    vlog("CFG: password della versione precedente -> utente 'admin'");
  }
  cfg.authSalt = ""; cfg.authHash = "";
  // file di prima della 1.7.1 (senza la riga "setup") con almeno un utente e il Wi-Fi di casa: gia in uso, la guida non serve.
  // Dalla 1.7.2 il Wi-Fi salvato da solo NON basta piu: la guida deve essere finita davvero.
  if (found && !g_cfgHadSetupKey && !cfg.setupDone && cfg.staEnabled && cfg.staSsid.length()) {
    for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].hash.length()) { cfg.setupDone = true; break; }
  }
  // sicurezza: ne HTTP ne HTTPS acceso, o stessa porta -> si torna ai valori di fabbrica
  if ((!cfg.httpOn && !cfg.https) || cfg.httpPort == cfg.httpsPort) { cfgSvcRecover(); vlog("CFG: servizi web non validi, valori di fabbrica"); }
  // guida non finita (primo avvio o reset): hotspot, HTTP, HTTPS e porte standard, cosi si entra sempre
  if (!cfg.setupDone) cfgSvcRecover();
  // hotspot: mai una password uguale per tutti (leggi UE RED/EN 18031, CRA; Regno Unito PSTI)
  if (cfgApPassWeak(cfg.apPass)) {
    cfg.apPass = cfgNewApPass();
    vlog("CFG: nuova password unica per l'hotspot (vedi la schermata di benvenuto sulla seriale)");
    cfgSetOrigin("primo avvio");
    cfgSave();
  }
  g_lastPub = cfgExport(false);
  return found;
}

bool cfgSvcRecover() {
  bool ch = !cfg.apOn || !cfg.apCaptive || !cfg.dhcpOn || !cfg.httpOn || !cfg.https || cfg.httpPort != 80 || cfg.httpsPort != 443;
  cfg.apOn = true; cfg.apCaptive = true; cfg.dhcpOn = true; cfg.httpOn = true; cfg.https = true; cfg.httpPort = 80; cfg.httpsPort = 443;
  return ch;
}

String cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort) {
  if (!httpOn && !httpsOn) return tr("Deve restare acceso almeno un protocollo web (HTTP o HTTPS), altrimenti resti chiuso fuori");
  if (httpOn && httpPort != 80 && (httpPort < 1024 || httpPort > 65535)) return tr("Porta HTTP non valida: 80 oppure da 1024 a 65535");
  if (httpsOn && httpsPort != 443 && (httpsPort < 1024 || httpsPort > 65535)) return tr("Porta HTTPS non valida: 443 oppure da 1024 a 65535");
  if (httpOn && httpsOn && httpPort == httpsPort) return tr("HTTP e HTTPS non possono usare la stessa porta");
  if (!apOn && !(cfg.staEnabled && cfg.staSsid.length())) return tr("Per spegnere il Punto di accesso serve prima la Wi-Fi di casa configurata");
  return "";
}

void cfgFactoryReset() {
  drvFsRemove(CFG_FILE);
  drvFsRemove(CFG_BAK);
  drvFsRemove(CFG_TMP);
  drvFsRemove("/mqtt-ca.pem");
  Preferences p;
  if (p.begin("vos", false)) { p.remove("cfgh"); p.remove("audit"); p.end(); }
  if (p.begin("tls", false)) { p.clear(); p.end(); }
  cfgDefaults();
}
