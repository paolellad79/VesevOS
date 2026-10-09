// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_audit.cpp
#include "vos_audit.h"
#include "vos_eventbus.h"
#include "vos_config.h"
#include "vos_region.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_util.h"
#include "../devices/vos_led.h"
#include "vos_time.h"

#define MAX_AL 16
#define MAX_HIST 20
struct Alarm { String key; String text; uint8_t lv; bool ack; bool used; uint32_t since; };
static Alarm g_al[MAX_AL];
static String g_hist[MAX_HIST]; static int g_histN = 0;

static void hist(const String& line) {
  String l = timeNowStr() + "  " + line;
  if (g_histN < MAX_HIST) g_hist[g_histN++] = l;
  else { for (int i = 1; i < MAX_HIST; i++) g_hist[i - 1] = g_hist[i]; g_hist[MAX_HIST - 1] = l; }
}

static void ledUpdate() {
  int red = 0, yel = 0;
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && !g_al[i].ack) { if (g_al[i].lv == AUD_RED) red++; else if (g_al[i].lv == AUD_YELLOW) yel++; }
  ledSetAlarm(red ? 2 : (yel ? 1 : 0));
}

void auditEvent(AuditLevel lv, const String& key, const String& text) {
  if (lv == AUD_INFO) { vlog("AUDIT: %s", text.c_str()); hist(text); return; }
  int free = -1;
  for (int i = 0; i < MAX_AL; i++) {
    if (g_al[i].used && g_al[i].key == key) {
      if (g_al[i].text != text || g_al[i].lv != lv) { g_al[i].text = text; g_al[i].lv = lv; g_al[i].ack = false; }
      ledUpdate(); return;
    }
    if (!g_al[i].used && free < 0) free = i;
  }
  if (free < 0) free = MAX_AL - 1;
  g_al[free].used = true; g_al[free].key = key; g_al[free].text = text; g_al[free].lv = lv; g_al[free].ack = false; g_al[free].since = millis();
  vlog("AUDIT: %s %s", lv == AUD_RED ? "ROSSO" : "GIALLO", text.c_str());
  EventBus::publish("audit.new", key.c_str(), (int)lv);      // nuovo allarme (1 giallo, 2 rosso); il testo resta nell'audit
  hist(String(lv == AUD_RED ? "[ROSSO] " : "[GIALLO] ") + text);
  ledUpdate();
}

void auditClear(const String& key) {
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && g_al[i].key == key) { g_al[i].used = false; g_al[i].key = ""; EventBus::publish("audit.clear", key.c_str(), 0); }
  ledUpdate();
}

static void cond(bool on, AuditLevel lv, const char* key, const String& text) {
  if (on) auditEvent(lv, key, text); else auditClear(key);
}

static bool hasAdmin() {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN && cfg.users[i].hash.length() == 64) return true;
  return false;
}

// Valori vietati: si correggono sempre (anche se arrivano da un file ripristinato o modificato a mano)
void auditFix() {
  if (cfgApPassWeak(cfg.apPass)) {
    cfg.apPass = cfgNewApPass();
    auditEvent(AUD_RED, "appass", tr("Password dell'hotspot debole o uguale per tutti: sostituita con una nuova (vedi seriale o Rete > Punto di accesso)"));
  }
  if (cfg.country.length() && !regionValid(cfg.country)) {
    auditEvent(AUD_RED, "country", trf("Paese '%s' sconosciuto: tolto, valgono le regole prudenti", cfg.country.c_str()));
    cfg.country = "";
  }
  if (cfg.txDbm > regionMaxDbm()) {
    auditEvent(AUD_RED, "txpower", trf("Potenza %d dBm oltre il limite del paese: riportata a %d dBm", (int)cfg.txDbm, regionMaxDbm()));
    cfg.txDbm = 0;
  }
  if (!regionChannelOk(cfg.meshCh)) {
    auditEvent(AUD_RED, "meshch", trf("Canale %d non ammesso nel paese: riportato a 1", (int)cfg.meshCh));
    cfg.meshCh = 1;
  }
  if (cfg.antGain < 0 || cfg.antGain > 15) cfg.antGain = 0;
  if (cfg.fwMode > 3) cfg.fwMode = 0;
}

void auditRefresh() {
  cond(!cfgLastSaveOk(), AUD_RED, "saveerr", tr("Salvataggio della configurazione fallito: le modifiche si perdono al riavvio (memoria piena?)"));
  cond(!cfg.setupDone, AUD_YELLOW, "setupnd", tr("Guida di configurazione non finita: hotspot e HTTP restano forzati accesi a ogni avvio"));
  cond(cfg.setupDone && !hasAdmin(), AUD_RED, "noadmin", tr("Nessun amministratore attivo: tieni premuto BOOT 8 s per crearlo di nuovo"));
  cond(!cfg.country.length(), AUD_YELLOW, "nocountry", tr("Paese non scelto: radio con regole prudenti (canali 1-11). Sceglilo in Sistema > Localizzazione"));
  cond(!cfg.serialAuth, AUD_YELLOW, "serial", tr("Seriale senza password: chi collega il cavo USB entra senza password"));
  cond(cfg.mqttPass.length() && !cfg.mqttTls, AUD_YELLOW, "mqtttls", tr("MQTT con password ma senza cifratura (mqtts): la password viaggia in chiaro"));
  cond(!cfg.https, AUD_YELLOW, "https", tr("HTTPS spento: pagina e password viaggiano senza cifratura sulla rete di casa"));
  cond(!cfg.wdTask && !cfg.wdNet && !cfg.wdRam, AUD_YELLOW, "wd", tr("Watchdog spento: se un servizio si blocca la scheda non si riprende da sola"));
  cond(cfg.fwMode == 2 && cfg.fwN == 0, AUD_YELLOW, "fwempty", tr("Filtro IP 'lista consentita' senza voci: entra solo chi e collegato all'hotspot"));
  cond(cfg.antExt && cfg.antGain > 0, AUD_YELLOW, "antenna", trf("Antenna esterna %d dBi: potenza ridotta a %d dBm. Usa solo antenne adatte (responsabilita di chi la monta)", (int)cfg.antGain, regionMaxDbm()));
}

// Registro delle modifiche: confronta il testo di prima e di dopo (senza password) riga per riga
static String sectionOf(const String& txt, int pos) {
  int p = txt.lastIndexOf("config ", pos);
  if (p < 0) return "";
  int q1 = txt.indexOf('\'', p), q2 = q1 >= 0 ? txt.indexOf('\'', q1 + 1) : -1;
  return (q1 >= 0 && q2 > q1) ? txt.substring(q1 + 1, q2) : String("");
}

void auditDiff(const String& o, const String& n, const String& origin) {
  int i = 0, shown = 0;
  while (i < (int)n.length()) {
    int e = n.indexOf('\n', i); if (e < 0) e = n.length();
    String line = n.substring(i, e);
    if (line.startsWith("\toption ") && o.indexOf(line) < 0) {
      String sec = sectionOf(n, i);
      String rest = line.substring(8);
      int sp = rest.indexOf(' ');
      String k = sp > 0 ? rest.substring(0, sp) : rest, v = sp > 0 ? rest.substring(sp + 1) : String("");
      if (shown < 6) auditEvent(AUD_INFO, "", trf("%s.%s = %s (%s)", sec.c_str(), k.c_str(), v.c_str(), origin.c_str()));
      shown++;
    }
    i = e + 1;
  }
  if (shown > 6) auditEvent(AUD_INFO, "", trf("... e altre %d modifiche (%s)", shown - 6, origin.c_str()));
}

void auditTick() {
  static uint32_t last = 0;
  if (last && millis() - last < 60000UL) return;
  last = millis(); if (!last) last = 1;
  if (cfgFileChanged()) {
    auditEvent(AUD_YELLOW, "tamper", tr("Configurazione modificata fuori dal pannello: controllata e corretta"));
    cfgSetOrigin("file modificato"); cfgSave();   // il file torna uguale alla configurazione in uso (controllata)
  }
}

void auditInit() {
  if (cfgFileChanged()) {
    auditEvent(AUD_YELLOW, "tamper", tr("Configurazione modificata fuori dal pannello: controllata e corretta"));
    cfgSetOrigin("file modificato"); cfgSave();
  }
  auditRefresh();
}

int auditCount(AuditLevel lv) {
  int n = 0;
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && !g_al[i].ack && g_al[i].lv == lv) n++;
  return n;
}

String auditJson() {
  String j = "{\"alarms\":[";
  bool first = true;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used) continue;
    if (!first) j += ","; first = false;
    j += "{\"n\":" + String(i + 1) + ",\"lv\":" + String(g_al[i].lv) + ",\"ack\":" + String(g_al[i].ack ? "true" : "false") +
         ",\"key\":\"" + jsonEscape(g_al[i].key) + "\",\"text\":\"" + jsonEscape(g_al[i].text) + "\"}";
  }
  j += "],\"history\":[";
  for (int i = g_histN - 1; i >= 0; i--) { j += "\"" + jsonEscape(g_hist[i]) + "\""; if (i) j += ","; }
  return j + "]}";
}

String auditText() {
  String t; int n = 0;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used) continue;
    t += String(i + 1) + "  " + (g_al[i].lv == AUD_RED ? tr("ROSSO ") : tr("GIALLO")) + (g_al[i].ack ? " (visto)  " : "  ") + g_al[i].text + "\n"; n++;
  }
  if (!n) t = String(tr("Nessun allarme: configurazione in regola")) + "\n";
  return t;
}

bool auditAck(int n) {
  bool any = false;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used || (n && i != n - 1)) continue;
    g_al[i].ack = true; any = true;
  }
  ledUpdate();
  return any;
}
