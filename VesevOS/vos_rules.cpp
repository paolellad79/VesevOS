// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules.cpp
#include "vos_rules.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_net.h"
#include "vos_sys.h"
#include "vos_time.h"
#include "vos_pins.h"
#include "vos_shell.h"
#include <LittleFS.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define RULES_FILE   "/rules.txt"
#define MAX_RULES    24
#define MAX_RUNS     4
#define MAX_TEXT     4096
#define MAX_ACTS     8
#define LOOP_MAX     10      // piu di 10 esecuzioni in un minuto = antiloop

enum { TK_TIME = 0, TK_EVERY, TK_AFTER, TK_BOOT, TK_WIFIUP, TK_WIFIDOWN, TK_TEMP };
enum { CK_NONE = 0, CK_BETWEEN, CK_DAY };

struct Rule {
  bool     on;
  char     name[25];
  uint8_t  tk;
  uint16_t a;            // minuti del giorno (time), secondi (every/after), gradi (temp)
  uint8_t  dmask;        // giorni (bit 0 = lunedi)
  uint8_t  ck;
  uint16_t c1, c2;       // fascia oraria (minuti)
  uint8_t  cmask;
  String   acts;
  // stato in esecuzione
  uint32_t lastMs;
  int32_t  lastMin;
  bool     done, armed, off;
  uint32_t nfire;
  time_t   lastEpoch;
  uint32_t ring[LOOP_MAX];
  uint8_t  ringN;
};

struct Run { bool act; int8_t rule; uint8_t step; uint32_t wake; };

static Rule  g_r[MAX_RULES];
static int   g_n = 0;
static Run   g_run[MAX_RUNS];
static SemaphoreHandle_t g_mx = nullptr;
static uint64_t g_mine = 0;           // pin gestiti dalle regole
static bool  g_first = true;          // primo caricamento dopo l'avvio
static uint32_t g_startMs = 0;
static bool  g_bootFired = false;
static NetState g_prevNet = NET_BOOT;

static void ensureMx() { if (!g_mx) g_mx = xSemaphoreCreateMutex(); }
static bool pinMine(int g) { return g >= 0 && g < 64 && ((g_mine >> g) & 1); }

// ---------- utilita ----------
static bool hhmm(const String& s, uint16_t& out) {
  if (s.length() != 5 || s[2] != ':') return false;
  if (!isDigit(s[0]) || !isDigit(s[1]) || !isDigit(s[3]) || !isDigit(s[4])) return false;
  int h = s.substring(0, 2).toInt(), m = s.substring(3, 5).toInt();
  if (h > 23 || m > 59) return false;
  out = h * 60 + m;
  return true;
}

static bool dmaskParse(const String& s, uint8_t& out) {
  if (s.length() != 7) return false;
  uint8_t m = 0;
  for (int i = 0; i < 7; i++) {
    if (s[i] == '1') m |= (1 << i);
    else if (s[i] != '0') return false;
  }
  out = m;
  return true;
}

static String word(const String& s, int idx) {
  int i = 0, n = 0;
  int L = s.length();
  while (i < L) {
    while (i < L && s[i] == ' ') i++;
    if (i >= L) break;
    int j = i;
    while (j < L && s[j] != ' ') j++;
    if (n == idx) return s.substring(i, j);
    n++; i = j;
  }
  return "";
}

static String restOf(const String& s, int idx) {
  int i = 0, n = 0, L = s.length();
  while (i < L) {
    while (i < L && s[i] == ' ') i++;
    if (i >= L) break;
    if (n == idx) return s.substring(i);
    while (i < L && s[i] != ' ') i++;
    n++;
  }
  return "";
}

static String field(const String& line, int idx) {   // campi separati da '|'
  int p = 0;
  for (int k = 0; k < idx; k++) { p = line.indexOf('|', p); if (p < 0) return ""; p++; }
  int e = line.indexOf('|', p);
  return line.substring(p, e < 0 ? line.length() : e);
}

static int fieldCount(const String& line) {
  int c = 1;
  for (int i = 0; i < (int)line.length(); i++) if (line[i] == '|') c++;
  return c;
}

static String actAt(const String& acts, int n) {     // n-esima azione (separate da ';')
  int p = 0;
  for (int k = 0; k < n; k++) { p = acts.indexOf(';', p); if (p < 0) return ""; p++; }
  int e = acts.indexOf(';', p);
  String a = acts.substring(p, e < 0 ? acts.length() : e);
  a.trim();
  return a;
}

static int actCount(const String& acts) {
  if (!acts.length()) return 0;
  int c = 1;
  for (int i = 0; i < (int)acts.length(); i++) if (acts[i] == ';') c++;
  return c;
}

static bool hex6ok(const String& s) {
  if (s.length() != 6) return false;
  for (int i = 0; i < 6; i++) if (!isHexadecimalDigit(s[i])) return false;
  return true;
}

// ---------- controllo di una azione ----------
static bool actionCheck(const String& a, String& err) {
  String w = word(a, 0), a1 = word(a, 1), a2 = word(a, 2);
  if (w == "led-color") { if (!hex6ok(a1)) { err = tr("Colore non valido (6 cifre esadecimali)"); return false; } }
  else if (w == "led") { if (!(a1 == "state" || a1 == "heartbeat" || a1 == "fixed" || a1 == "off")) { err = tr("Azione LED non valida"); return false; } }
  else if (w == "led-bright") { int v = a1.toInt(); if (a1 == "" || v < 0 || v > 255) { err = tr("Luminosita da 0 a 255"); return false; } }
  else if (w == "led2") { if (!(a1 == "on" || a1 == "off" || a1 == "heartbeat")) { err = tr("Azione LED non valida"); return false; } }
  else if (w == "gpio") {
    int g = a1.toInt();
    if (a1 == "" || !(a2 == "0" || a2 == "1")) { err = tr("Uso: gpio <pin> 0|1"); return false; }
    if (!pinMine(g)) {
      const char* why = pinTestBlock(g);
      if (why) { err = String("GPIO") + g + ": " + why; return false; }
    }
  }
  else if (w == "wait") { int v = a1.toInt(); if (a1 == "" || v < 1 || v > 3600) { err = tr("Attesa da 1 a 3600 secondi"); return false; } }
  else if (w == "note") { if (restOf(a, 1).length() < 1) { err = tr("Manca il testo della nota"); return false; } }
  else if (w == "reboot") { }
  else if (w == "ntp") { if (a1 != "sync") { err = tr("Azione non ammessa"); return false; } }
  else { err = tr("Azione non ammessa"); return false; }
  return true;
}

// ---------- lettura di una riga ----------
static bool parseLine(const String& line, Rule& r, String& err) {
  if (fieldCount(line) != 5) { err = tr("La riga deve avere 5 campi"); return false; }
  String on = field(line, 0), nm = field(line, 1), tg = field(line, 2), cd = field(line, 3), ac = field(line, 4);
  on.trim(); nm.trim(); tg.trim(); cd.trim(); ac.trim();
  if (on != "0" && on != "1") { err = tr("Il primo campo e 1 o 0"); return false; }
  r.on = (on == "1");
  nm = cleanAscii(nm);
  if (nm.length() < 1 || nm.length() > 24) { err = tr("Nome da 1 a 24 caratteri"); return false; }
  strncpy(r.name, nm.c_str(), 24); r.name[24] = 0;
  String t0 = word(tg, 0), t1 = word(tg, 1), t2 = word(tg, 2);
  r.a = 0; r.dmask = 0x7F;
  if (t0 == "time") {
    r.tk = TK_TIME;
    if (!hhmm(t1, r.a) || !dmaskParse(t2, r.dmask) || r.dmask == 0) { err = tr("Orario non valido (HH:MM e 7 cifre per i giorni)"); return false; }
  } else if (t0 == "every") {
    r.tk = TK_EVERY; int v = t1.toInt();
    if (t1 == "" || v < 5 || v > 86400) { err = tr("Ogni: da 5 a 86400 secondi"); return false; }
    r.a = v;
  } else if (t0 == "after") {
    r.tk = TK_AFTER; int v = t1.toInt();
    if (t1 == "" || v < 1 || v > 86400) { err = tr("Dopo: da 1 a 86400 secondi"); return false; }
    r.a = v;
  } else if (t0 == "boot") { r.tk = TK_BOOT; }
  else if (t0 == "wifi") {
    if (t1 == "up") r.tk = TK_WIFIUP; else if (t1 == "down") r.tk = TK_WIFIDOWN;
    else { err = tr("Wi-Fi: up oppure down"); return false; }
  } else if (t0 == "temp") {
    r.tk = TK_TEMP; int v = t1.toInt();
    if (t1 == "" || v < 30 || v > 110) { err = tr("Temperatura da 30 a 110"); return false; }
    r.a = v;
  } else { err = tr("Evento non riconosciuto"); return false; }
  r.ck = CK_NONE; r.c1 = r.c2 = 0; r.cmask = 0x7F;
  if (cd != "-" && cd != "") {
    String c0 = word(cd, 0);
    if (c0 == "between") {
      if (!hhmm(word(cd, 1), r.c1) || !hhmm(word(cd, 2), r.c2)) { err = tr("Fascia oraria non valida"); return false; }
      r.ck = CK_BETWEEN;
    } else if (c0 == "day") {
      if (!dmaskParse(word(cd, 1), r.cmask) || r.cmask == 0) { err = tr("Giorni non validi"); return false; }
      r.ck = CK_DAY;
    } else { err = tr("Condizione non riconosciuta"); return false; }
  }
  int na = actCount(ac);
  if (na < 1 || na > MAX_ACTS) { err = tr("Da 1 a 8 azioni"); return false; }
  for (int i = 0; i < na; i++) {
    String a = actAt(ac, i);
    if (!actionCheck(a, err)) return false;
  }
  r.acts = ac;
  return true;
}

// ---------- caricamento ----------
static void resetState(Rule& r) {
  r.lastMs = millis(); r.lastMin = -1; r.done = false; r.armed = false; r.off = false;
  r.nfire = 0; r.lastEpoch = 0; r.ringN = 0;
}

static bool parseAll(const String& text, Rule* out, int& n, String& err) {
  n = 0;
  int p = 0, ln = 0;
  while (p <= (int)text.length()) {
    int e = text.indexOf('\n', p);
    if (e < 0) e = text.length();
    String line = text.substring(p, e);
    p = e + 1; ln++;
    line.replace("\r", ""); line.trim();
    if (!line.length() || line[0] == '#') continue;
    if (n >= MAX_RULES) { err = trf("Al massimo %d regole", MAX_RULES); return false; }
    String e2;
    if (!parseLine(line, out[n], e2)) { err = trf("Riga %d: %s", ln, e2.c_str()); return false; }
    resetState(out[n]);
    n++;
    if (e >= (int)text.length()) break;
  }
  return true;
}

static void load(bool boot) {
  ensureMx();
  String text;
  File f = LittleFS.open(RULES_FILE, "r");
  if (f) { text = f.readString(); f.close(); }
  static Rule tmp[MAX_RULES];
  int n = 0; String err;
  if (!parseAll(text, tmp, n, err)) { vlog("RULES: file non valido (%s)", err.c_str()); n = 0; }
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < MAX_RUNS; i++) g_run[i].act = false;
  g_n = n;
  for (int i = 0; i < n; i++) {
    g_r[i] = tmp[i];
    // dopo una modifica le regole "after"/"boot" non devono ripartire da sole
    if (!boot) { if (g_r[i].tk == TK_AFTER || g_r[i].tk == TK_BOOT) g_r[i].done = true; }
    if (g_r[i].tk == TK_TEMP) g_r[i].armed = sysCpuTemp() > g_r[i].a;
  }
  xSemaphoreGive(g_mx);
  vlog("RULES: %d regole caricate", n);
}

String rulesText() {
  File f = LittleFS.open(RULES_FILE, "r");
  if (!f) return "";
  String s = f.readString(); f.close();
  return s;
}

bool rulesSave(const String& text, String& err) {
  if (text.length() > MAX_TEXT) { err = tr("File troppo grande"); return false; }
  static Rule tmp[MAX_RULES];
  int n;
  if (!parseAll(text, tmp, n, err)) return false;
  File f = LittleFS.open("/rules.tmp", "w");
  if (!f) { err = tr("Scrittura non riuscita"); return false; }
  size_t w = f.print(text); f.close();
  if (w != text.length()) { LittleFS.remove("/rules.tmp"); err = tr("Scrittura non riuscita"); return false; }
  LittleFS.remove(RULES_FILE);
  if (!LittleFS.rename("/rules.tmp", RULES_FILE)) { err = tr("Scrittura non riuscita"); return false; }
  load(false);
  vlog("RULES: regole salvate (%d)", n);
  return true;
}

int rulesCount() { return g_n; }

// ---------- esecuzione delle azioni ----------
class NullPrint : public Print {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override { return n; }
};

static void doGpio(int g, int v, const char* rn) {
  if (!pinMine(g)) {
    const char* why = pinTestBlock(g);
    if (why || !pinClaim(g, "Automazioni", "Pin gestito da una regola", false)) { vlog("RULES %s: GPIO%d non usabile", rn, g); return; }
    g_mine |= ((uint64_t)1 << g);
    pinMode(g, OUTPUT);
  }
  digitalWrite(g, v ? HIGH : LOW);
}

// Esegue una azione. Ritorna i millisecondi di attesa richiesti (0 = nessuna).
static uint32_t doAction(const String& a, const char* rn) {
  String w = word(a, 0);
  if (w == "wait") return (uint32_t)word(a, 1).toInt() * 1000UL;
  if (w == "note") { vlog("RULES %s: %s", rn, restOf(a, 1).c_str()); return 0; }
  if (w == "gpio") { doGpio(word(a, 1).toInt(), word(a, 2).toInt(), rn); return 0; }
  if (w == "reboot") {
    if (millis() < 60000UL) { vlog("RULES %s: riavvio ignorato (meno di 60 s dall'avvio)", rn); return 0; }
    vlog("RULES %s: riavvio", rn);
    delay(300); ESP.restart();
    return 0;
  }
  NullPrint np;
  shellExec(a, np, true);
  return 0;
}

// ---------- avvio di una regola ----------
static void fire(int i) {            // chiamare con il mutex preso
  Rule& r = g_r[i];
  if (!r.on || r.off) return;
  uint32_t now = millis();
  // antiloop: troppe esecuzioni in un minuto -> la regola si ferma
  if (r.ringN >= LOOP_MAX && now - r.ring[r.ringN % LOOP_MAX] < 60000UL) {
    r.off = true;
    vlog("RULES %s: troppe esecuzioni, regola fermata (antiloop)", r.name);
    return;
  }
  r.ring[r.ringN % LOOP_MAX] = now; r.ringN++;
  for (int k = 0; k < MAX_RUNS; k++) {
    if (!g_run[k].act) { g_run[k].act = true; g_run[k].rule = i; g_run[k].step = 0; g_run[k].wake = 0; return; }
  }
  vlog("RULES %s: troppe regole in esecuzione, saltata", r.name);
}

static bool condOk(const Rule& r) {
  if (r.ck == CK_NONE) return true;
  if (!timeValid()) return false;
  time_t t = time(nullptr);
  struct tm tmv; localtime_r(&t, &tmv);
  uint16_t m = tmv.tm_hour * 60 + tmv.tm_min;
  int day = (tmv.tm_wday + 6) % 7;
  if (r.ck == CK_DAY) return (r.cmask >> day) & 1;
  return (r.c1 <= r.c2) ? (m >= r.c1 && m <= r.c2) : (m >= r.c1 || m <= r.c2);
}

bool rulesRunNow(int index, String& err) {
  ensureMx();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  bool ok = (index >= 0 && index < g_n);
  if (!ok) err = tr("Regola inesistente");
  else { g_r[index].off = false; g_r[index].ringN = 0; if (!g_r[index].on) { err = tr("Regola spenta"); ok = false; } else fire(index); }
  xSemaphoreGive(g_mx);
  return ok;
}

static void stepRuns() {
  uint32_t now = millis();
  for (int k = 0; k < MAX_RUNS; k++) {
    Run& u = g_run[k];
    if (!u.act) continue;
    if (u.wake && (int32_t)(now - u.wake) < 0) continue;
    u.wake = 0;
    Rule& r = g_r[u.rule];
    int na = actCount(r.acts);
    while (u.step < na) {
      String a = actAt(r.acts, u.step);
      u.step++;
      uint32_t wt = doAction(a, r.name);
      if (wt) { u.wake = millis() + wt; if (!u.wake) u.wake = 1; break; }
    }
    if (u.step >= na && !u.wake) {
      u.act = false;
      r.nfire++; r.lastEpoch = timeValid() ? time(nullptr) : 0;
    }
  }
}

static void evalTriggers() {
  uint32_t now = millis();
  bool tv = timeValid();
  int32_t minKey = -1; uint16_t mins = 0; int day = 0;
  if (tv) {
    time_t t = time(nullptr);
    struct tm tmv; localtime_r(&t, &tmv);
    minKey = (int32_t)(t / 60); mins = tmv.tm_hour * 60 + tmv.tm_min; day = (tmv.tm_wday + 6) % 7;
  }
  NetState ns = netState();
  bool up = (ns == NET_CLIENT_OK) && g_prevNet != NET_CLIENT_OK;
  bool down = (ns != NET_CLIENT_OK) && g_prevNet == NET_CLIENT_OK;
  g_prevNet = ns;
  float temp = sysCpuTemp();
  for (int i = 0; i < g_n; i++) {
    Rule& r = g_r[i];
    if (!r.on || r.off) continue;
    bool go = false;
    switch (r.tk) {
      case TK_TIME:
        if (tv && mins == r.a && ((r.dmask >> day) & 1) && r.lastMin != minKey) { go = true; r.lastMin = minKey; }
        break;
      case TK_EVERY:
        if (now - r.lastMs >= (uint32_t)r.a * 1000UL) { go = true; r.lastMs = now; }
        break;
      case TK_AFTER:
        if (!r.done && now >= (uint32_t)r.a * 1000UL) { go = true; r.done = true; }
        break;
      case TK_BOOT:
        if (!r.done && now - g_startMs > 3000UL) { go = true; r.done = true; }
        break;
      case TK_WIFIUP:   go = up;   break;
      case TK_WIFIDOWN: go = down; break;
      case TK_TEMP:
        if (!r.armed && temp > r.a) { go = true; r.armed = true; }
        else if (r.armed && temp < (float)r.a - 3.0f) r.armed = false;
        break;
    }
    if (go && condOk(r)) fire(i);
  }
}

static void rulesTask(void*) {
  uint32_t lastEval = 0;
  for (;;) {
    xSemaphoreTake(g_mx, portMAX_DELAY);
    uint32_t now = millis();
    if (now - lastEval >= 1000UL) { lastEval = now; evalTriggers(); }
    stepRuns();
    xSemaphoreGive(g_mx);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

String rulesStatusJson() {
  String j = "[";
  ensureMx();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < g_n; i++) {
    if (i) j += ",";
    String last = "";
    if (g_r[i].lastEpoch) {
      struct tm tmv; localtime_r(&g_r[i].lastEpoch, &tmv);
      char b[24]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
      last = b;
    }
    bool run = false;
    for (int k = 0; k < MAX_RUNS; k++) if (g_run[k].act && g_run[k].rule == i) run = true;
    j += "{\"i\":" + String(i) + ",\"last\":\"" + last + "\",\"n\":" + String((unsigned long)g_r[i].nfire) +
         ",\"off\":" + (g_r[i].off ? "true" : "false") + ",\"run\":" + (run ? "true" : "false") + "}";
  }
  xSemaphoreGive(g_mx);
  return j + "]";
}

void rulesInit() {
  ensureMx();
  g_startMs = millis();
  g_prevNet = netState();
  load(true);
  xTaskCreatePinnedToCore(rulesTask, "rules", 6144, NULL, 1, NULL, 0);
}
