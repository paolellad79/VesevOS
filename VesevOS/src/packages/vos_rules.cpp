// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules.cpp
#include "vos_rules.h"
#include "../drivers/vos_drv_task.h"
#include "vos_mqtt.h"
#include "vos_berry.h"
#include "vos_app.h"
#include "../core/vos_hooks.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_parse.h"
#include "../core/vos_log.h"
#include "../core/vos_wd.h"
#include "../net/vos_net.h"
#include "../core/vos_sys.h"
#include "../core/vos_time.h"
#include "../devices/vos_pins.h"
#include "../drivers/vos_drv_fs.h"
#include <time.h>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_print.h"

#include "vos_rules_int.h"
#include "../drivers/vos_drv_time.h"

namespace vosrules { Rule* g_r = nullptr; }           // tabella delle regole: esiste solo se c'e almeno una regola (con 0 regole non occupa RAM)
static bool  g_taskUp = false;        // il task "rules" parte solo quando serve
namespace vosrules { int   g_n = 0; }
namespace vosrules { Run   g_run[MAX_RUNS]; }
namespace vosrules { SemaphoreHandle_t g_mx = nullptr; }
static uint64_t g_mine = 0;           // pin gestiti dalle regole
static uint32_t g_startMs = 0;
static NetState g_prevNet = NET_BOOT;

void vosrules::ensureMx() { if (!g_mx) g_mx = xSemaphoreCreateMutex(); }
static bool pinMine(int g) { return g >= 0 && g < 64 && ((g_mine >> g) & 1); }

// ---------- utilita (lavorano su viste del testo: non copiano) ----------
using std::string_view;

static bool hhmm(string_view s, uint16_t& out) {
  if (s.size() != 5 || s[2] != ':') return false;
  const int pos[4] = {0, 1, 3, 4};
  for (int p : pos) if (!isdigit((unsigned char)s[p])) return false;
  int h = (s[0] - '0') * 10 + (s[1] - '0'), m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  out = h * 60 + m;
  return true;
}

static bool dmaskParse(string_view s, uint8_t& out) {
  if (s.size() != 7) return false;
  uint8_t m = 0;
  for (int i = 0; i < 7; i++) {
    if (s[i] == '1') m |= (1 << i);
    else if (s[i] != '0') return false;
  }
  out = m;
  return true;
}

// parola numero idx (separate da spazi); vista vuota se non c'e
static string_view word(string_view s, int idx) {
  size_t i = 0;
  int n = 0;
  while (i < s.size()) {
    while (i < s.size() && s[i] == ' ') i++;
    if (i >= s.size()) break;
    size_t j = i;
    while (j < s.size() && s[j] != ' ') j++;
    if (n == idx) return s.substr(i, j - i);
    n++; i = j;
  }
  return string_view();
}

// tutto il testo dalla parola numero idx in poi
static string_view restOf(string_view s, int idx) {
  size_t i = 0;
  int n = 0;
  while (i < s.size()) {
    while (i < s.size() && s[i] == ' ') i++;
    if (i >= s.size()) break;
    if (n == idx) return s.substr(i);
    while (i < s.size() && s[i] != ' ') i++;
    n++;
  }
  return string_view();
}

// campo numero idx di una riga (campi separati da '|')
static string_view field(string_view line, int idx) {
  size_t p = 0;
  for (int k = 0; k < idx; k++) {
    p = line.find('|', p);
    if (p == string_view::npos) return string_view();
    p++;
  }
  size_t e = line.find('|', p);
  return line.substr(p, e == string_view::npos ? string_view::npos : e - p);
}

static int fieldCount(string_view line) {
  int c = 1;
  for (char ch : line) if (ch == '|') c++;
  return c;
}

// n-esima azione (separate da ';'), senza spazi ai lati
static string_view actAt(string_view acts, int n) {
  size_t p = 0;
  for (int k = 0; k < n; k++) {
    p = acts.find(';', p);
    if (p == string_view::npos) return string_view();
    p++;
  }
  size_t e = acts.find(';', p);
  return trimView(acts.substr(p, e == string_view::npos ? string_view::npos : e - p));
}

static int actCount(string_view acts) {
  if (acts.empty()) return 0;
  int c = 1;
  for (char ch : acts) if (ch == ';') c++;
  return c;
}

static bool hex6ok(string_view s) {
  if (s.size() != 6) return false;
  for (char ch : s) if (!isxdigit((unsigned char)ch)) return false;
  return true;
}

// Copia una parola in un buffer con lo zero finale (per le funzioni che vogliono un const char*). Se non ci sta si taglia:
// i nomi validi sono piu corti, quindi un nome tagliato resta non valido.
static void wordTo(char* out, size_t cap, string_view w) {
  size_t n = w.size() < cap - 1 ? w.size() : cap - 1;
  memcpy(out, w.data(), n);
  out[n] = 0;
}

// ---------- controllo di una azione ----------
static bool actionCheck(string_view a, ErrBuf& err) {
  string_view w = word(a, 0), a1 = word(a, 1), a2 = word(a, 2);
  if (w == "led-color") { if (!hex6ok(a1)) { err = tr("Colore non valido (6 cifre esadecimali)"); return false; } }
  else if (w == "led") { if (!(a1 == "state" || a1 == "heartbeat" || a1 == "fixed" || a1 == "off")) { err = tr("Azione LED non valida"); return false; } }
  else if (w == "led-bright") { long v = viewToLong(a1); if (a1.empty() || v < 0 || v > 255) { err = tr("Luminosita da 0 a 255"); return false; } }
  else if (w == "led2") { }   // LED aggiuntivo tolto nella 1.7.0: azione vecchia accettata e ignorata
  else if (w == "gpio") {
    int g = (int)viewToLong(a1);
    if (a1.empty() || !(a2 == "0" || a2 == "1")) { err = tr("Uso: gpio <pin> 0|1"); return false; }
    if (!pinMine(g)) {
      const char* why = pinTestBlock(g);
      if (why) { err.setf("GPIO%d: %s", g, why); return false; }
    }
  }
  else if (w == "airplane") { if (!(a1 == "on" || a1 == "off")) { err = tr("Uso: airplane on <come> | airplane off"); return false; } }
  else if (w == "wait") { long v = viewToLong(a1); if (a1.empty() || v < 1 || v > 3600) { err = tr("Attesa da 1 a 3600 secondi"); return false; } }
  else if (w == "note") { if (restOf(a, 1).empty()) { err = tr("Manca il testo della nota"); return false; } }
  else if (w == "reboot") { }
  else if (w == "mqtt") { if (a1.size() < 1 || a1.size() > 60) { err = tr("Uso: mqtt <argomento> <testo>"); return false; } }
  else if (w == "script") { char nm[32]; wordTo(nm, sizeof nm, a1); if (!berryNameValid(nm)) { err = tr("Nome script non valido (1-24 caratteri: lettere minuscole, numeri, trattino, sottotrattino)"); return false; } }
  else if (w == "app") { if (!utilNameOk(a1.data(), a1.size())) { err = tr("Nome app non valido"); return false; } }
  else if (w == "appstop") { }
  else if (w == "ntp") { if (a1 != "sync") { err = tr("Azione non ammessa"); return false; } }
  else { err = tr("Azione non ammessa"); return false; }
  return true;
}

// ---------- lettura di una riga ----------
static bool parseLine(string_view line, Rule& r, ErrBuf& err) {
  if (fieldCount(line) != 5) { err = tr("La riga deve avere 5 campi"); return false; }
  string_view on = trimView(field(line, 0)), nm = trimView(field(line, 1)), tg = trimView(field(line, 2));
  string_view cd = trimView(field(line, 3)), ac = trimView(field(line, 4));
  if (on != "0" && on != "1") { err = tr("Il primo campo e 1 o 0"); return false; }
  r.on = (on == "1");
  size_t nl = 0;                                      // nome pulito (come cleanAscii): niente \r, i caratteri strani diventano '?'
  for (char ch : nm) {
    unsigned char c = (unsigned char)ch;
    if (c == '\r') continue;
    if (nl < 24) r.name[nl] = (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? ch : '?';
    nl++;
  }
  if (nl < 1 || nl > 24) { err = tr("Nome da 1 a 24 caratteri"); return false; }
  r.name[nl] = 0;
  string_view t0 = word(tg, 0), t1 = word(tg, 1), t2 = word(tg, 2);
  r.a = 0; r.dmask = 0x7F;
  if (t0 == "time") {
    r.tk = TK_TIME;
    if (!hhmm(t1, r.a) || !dmaskParse(t2, r.dmask) || r.dmask == 0) { err = tr("Orario non valido (HH:MM e 7 cifre per i giorni)"); return false; }
  } else if (t0 == "every") {
    r.tk = TK_EVERY; long v = viewToLong(t1);
    if (t1.empty() || v < 5 || v > 86400) { err = tr("Ogni: da 5 a 86400 secondi"); return false; }
    r.a = v;
  } else if (t0 == "after") {
    r.tk = TK_AFTER; long v = viewToLong(t1);
    if (t1.empty() || v < 1 || v > 86400) { err = tr("Dopo: da 1 a 86400 secondi"); return false; }
    r.a = v;
  } else if (t0 == "boot") { r.tk = TK_BOOT; }
  else if (t0 == "wifi") {
    if (t1 == "up") r.tk = TK_WIFIUP; else if (t1 == "down") r.tk = TK_WIFIDOWN;
    else { err = tr("Wi-Fi: up oppure down"); return false; }
  } else if (t0 == "temp") {
    r.tk = TK_TEMP; long v = viewToLong(t1);
    if (t1.empty() || v < 30 || v > 110) { err = tr("Temperatura da 30 a 110"); return false; }
    r.a = v;
  } else { err = tr("Evento non riconosciuto"); return false; }
  r.ck = CK_NONE; r.c1 = r.c2 = 0; r.cmask = 0x7F;
  if (cd != "-" && !cd.empty()) {
    string_view c0 = word(cd, 0);
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
    if (!actionCheck(actAt(ac, i), err)) return false;
  }
  r.acts.assign(ac.data(), ac.size());
  return true;
}

// ---------- caricamento ----------
static void resetState(Rule& r) {
  r.lastMs = drvMillis(); r.lastMin = -1; r.done = false; r.armed = false; r.off = false;
  r.nfire = 0; r.lastEpoch = 0; r.ringN = 0;
}

static bool parseAll(string_view text, Rule* out, int& n, ErrBuf& err) {
  n = 0;
  std::string clean;                                  // i \r si tolgono ovunque (anche in mezzo alla riga): copia solo se ce ne sono
  if (text.find('\r') != string_view::npos) {
    clean.reserve(text.size());
    for (char ch : text) if (ch != '\r') clean += ch;
    text = clean;
  }
  size_t p = 0;
  int ln = 0;
  while (p <= text.size()) {
    size_t e = text.find('\n', p);
    if (e == string_view::npos) e = text.size();
    string_view line = trimView(text.substr(p, e - p));
    p = e + 1; ln++;
    if (line.empty() || line[0] == '#') continue;
    if (n >= MAX_RULES) { err.setf(tr("Al massimo %d regole"), MAX_RULES); return false; }
    ErrBuf e2;
    if (!parseLine(line, out[n], e2)) { err.setf(tr("Riga %d: %s"), ln, e2.c_str()); return false; }
    resetState(out[n]);
    n++;
    if (e >= text.size()) break;
  }
  return true;
}

static void rulesTask(void*);
static void ensureTask() {
  if (g_taskUp) return;
  // core: automatico (prima: core 0 fisso)
  if (drvTaskCreate(rulesTask, "rules", 6144, NULL, 1, NULL, DRV_CORE_ANY)) g_taskUp = true;
  else vlog("RULES: task non avviato (memoria)");
}

static void load(bool boot) {
  ensureMx();
  HeapBuf text(RULES_READ_MAX);
  bool readOk = rulesText(text);
  Rule* tmp = new (std::nothrow) Rule[MAX_RULES]();       // appoggio temporaneo (>= 256 byte: va in PSRAM), liberato a fine funzione
  int n = 0; ErrBuf err;
  if (!tmp || !readOk) { vlog("RULES: memoria insufficiente"); }
  else if (!parseAll(string_view(text.c_str(), text.length()), tmp, n, err)) { vlog("RULES: file non valido (%s)", err.c_str()); n = 0; }
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < MAX_RUNS; i++) g_run[i].act = false;
  if (n > 0 && !g_r) { g_r = new (std::nothrow) Rule[MAX_RULES](); if (!g_r) { vlog("RULES: memoria insufficiente"); n = 0; } }
  g_n = n;
  for (int i = 0; i < n; i++) {
    g_r[i] = std::move(tmp[i]);
    // dopo una modifica le regole "after"/"boot" non devono ripartire da sole
    if (!boot) { if (g_r[i].tk == TK_AFTER || g_r[i].tk == TK_BOOT) g_r[i].done = true; }
    if (g_r[i].tk == TK_TEMP) g_r[i].armed = sysCpuTemp() > g_r[i].a;
  }
  if (n == 0 && g_r) { delete[] g_r; g_r = nullptr; }
  xSemaphoreGive(g_mx);
  delete[] tmp;
  vlog("RULES: %d regole caricate", n);
  if (n > 0) ensureTask();
}

bool rulesSave(string_view text, ErrBuf& err) {
  if (text.size() > MAX_TEXT) { err = tr("File troppo grande"); return false; }
  Rule* tmp = new (std::nothrow) Rule[MAX_RULES]();
  if (!tmp) { err = tr("Memoria insufficiente"); return false; }
  int n;
  bool parsed = parseAll(text, tmp, n, err);
  delete[] tmp;
  if (!parsed) return false;
  VosFile f = drvFsOpen("/rules.tmp", "w");
  if (!f) { err = tr("Scrittura non riuscita"); return false; }
  size_t w = f.write((const uint8_t*)text.data(), text.size()); f.close();
  if (w != text.size()) { drvFsRemove("/rules.tmp"); err = tr("Scrittura non riuscita"); return false; }
  drvFsRemove(RULES_FILE);
  if (!drvFsRename("/rules.tmp", RULES_FILE)) { err = tr("Scrittura non riuscita"); return false; }
  load(false);
  vlog("RULES: regole salvate (%d)", n);
  return true;
}

int rulesCount() { return g_n; }

// ---------- esecuzione delle azioni ----------
class NullPrint : public OutSink {
 public:
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override { return n; }
};

static void doGpio(int g, int v, const char* rn) {
  if (!pinMine(g)) {
    const char* why = pinTestBlock(g);
    if (why || !pinClaim(g, "Automazioni", "Pin gestito da una regola", false)) { vlog("RULES %s: GPIO%d non usabile", rn, g); return; }
    g_mine |= ((uint64_t)1 << g);
    drvGpioMode(g, DG_OUTPUT);
  }
  drvGpioWrite(g, v != 0);
}

// Esegue una azione. Ritorna i millisecondi di attesa richiesti (0 = nessuna).
static uint32_t doAction(string_view a, const char* rn) {
  string_view w = word(a, 0);
  if (w == "wait") return (uint32_t)viewToLong(word(a, 1)) * 1000UL;
  if (w == "led2") return 0;   // tolto nella 1.7.0
  if (w == "note") { string_view t = restOf(a, 1); vlog("RULES %s: %.*s", rn, (int)t.size(), t.data()); return 0; }
  if (w == "gpio") { doGpio((int)viewToLong(word(a, 1)), (int)viewToLong(word(a, 2)), rn); return 0; }
  if (w == "mqtt") {
    char topic[64];                                   // al massimo 60 caratteri (lo controlla actionCheck)
    wordTo(topic, sizeof topic, word(a, 1));
    std::string text(restOf(a, 2));
    if (!mqttPublishRel(topic, text.c_str())) vlog("RULES %s: MQTT non collegato", rn);
    return 0;
  }
  char name[32];
  wordTo(name, sizeof name, word(a, 1));
  if (w == "script") { ErrBuf e; if (!berryQueue(name, e, rn)) vlog("RULES %s: script %s non avviato: %s", rn, name, e.c_str()); return 0; }
  if (w == "app") { ErrBuf e; if (!appStart(name, e, 2, rn)) vlog("RULES %s: app %s non avviata: %s", rn, name, e.c_str()); return 0; }
  if (w == "appstop") { char cur[32]; ErrBuf e; if (berryAppRunning(cur, sizeof cur) && !appStop(cur, e, 2)) vlog("RULES %s: app %s non fermata: %s", rn, cur, e.c_str()); return 0; }
  if (w == "reboot") {
    if (drvMillis() < 60000UL) { vlog("RULES %s: riavvio ignorato (meno di 60 s dall'avvio)", rn); return 0; }
    vlog("RULES %s: riavvio", rn);
    char b[40]; snprintf(b, sizeof(b), "regola %s", rn); sysReboot(b, 300);
    return 0;
  }
  NullPrint np;
  std::string line(a);                                // la shell vuole un const char* con lo zero finale
  hookShellExec(line.c_str(), np, true);
  return 0;
}

// ---------- avvio di una regola ----------
static void fire(int i) {            // chiamare con il mutex preso
  Rule& r = g_r[i];
  if (!r.on || r.off) return;
  uint32_t now = drvMillis();
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

bool rulesRunNow(int index, ErrBuf& err) {
  ensureMx();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  bool ok = (index >= 0 && index < g_n);
  if (!ok) err = tr("Regola inesistente");
  else { g_r[index].off = false; g_r[index].ringN = 0; if (!g_r[index].on) { err = tr("Regola spenta"); ok = false; } else fire(index); }
  xSemaphoreGive(g_mx);
  return ok;
}

static void stepRuns() {
  uint32_t now = drvMillis();
  for (int k = 0; k < MAX_RUNS; k++) {
    Run& u = g_run[k];
    if (!u.act) continue;
    if (u.wake && (int32_t)(now - u.wake) < 0) continue;
    u.wake = 0;
    Rule& r = g_r[u.rule];
    int na = actCount(r.acts);
    while (u.step < na) {
      string_view a = actAt(r.acts, u.step);
      u.step++;
      uint32_t wt = doAction(a, r.name);
      if (wt) { u.wake = drvMillis() + wt; if (!u.wake) u.wake = 1; break; }
    }
    if (u.step >= na && !u.wake) {
      u.act = false;
      r.nfire++; r.lastEpoch = timeValid() ? time(nullptr) : 0;
    }
  }
}

static void evalTriggers() {
  uint32_t now = drvMillis();
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
    uint32_t now = drvMillis();
    if (now - lastEval >= 1000UL) { lastEval = now; evalTriggers(); }
    stepRuns();
    xSemaphoreGive(g_mx);
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

bool rulesAction(const char* action, ErrBuf& err, const char* who) {
  string_view a = trimView(action ? action : "");
  if (word(a, 0) == "wait") { err = tr("Azione non ammessa"); return false; }
  if (!actionCheck(a, err)) return false;
  doAction(a, who);
  return true;
}

void rulesInit() {
  ensureMx();
  g_startMs = drvMillis();
  g_prevNet = netState();
  load(true);            // il task parte da solo se ci sono regole (ensureTask)
}
