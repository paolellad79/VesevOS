// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dev.cpp
// Registro delle periferiche (virtuali e hardware) con un'unica interfaccia: stato, acceso/spento, dettagli, azioni.
#include "vos_dev.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_sys.h"
#include "vos_net.h"
#include "vos_led.h"
#include "vos_pins.h"
#include "vos_region.h"
#include "vos_mqtt.h"
#include "vos_mesh.h"
#include "vos_ble.h"
#include "vos_mfa.h"
#include "vos_stats.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// ---- testi del registro (le chiavi per le traduzioni: la tabella usa i testi cosi come sono) ----
[[maybe_unused]] static void devKeys() {
  tr("Messaggi"); tr("Radio"); tr("Sicurezza"); tr("Misure"); tr("Ingressi e uscite"); tr("Luce"); tr("Pulsante");
  tr("Collegamenti"); tr("Sensori"); tr("Risorse");
  tr("Bluetooth"); tr("ESP-NOW"); tr("Statistiche"); tr("MFA (due passaggi)");
  tr("Pin (GPIO)"); tr("LED di stato"); tr("Tasto BOOT"); tr("USB"); tr("Temperatura del chip"); tr("Memoria"); tr("Processore"); tr("Radio 2,4 GHz");
}

// ---- righe "etichetta / valore" per le schede dell'hardware ----
struct Rows {
  String j; bool first = true;
  void add(const String& k, const String& v) { if (!first) j += ","; first = false; j += "[\"" + jsonEscape(k) + "\",\"" + jsonEscape(v) + "\"]"; }
  String done() { return "{\"rows\":[" + j + "]}"; }
};
static String kb(size_t b) { return String((unsigned long)(b / 1024)) + " KB"; }
static String yn(bool b) { return String(b ? tr("si") : tr("no")); }
static String errJson(const String& e) { return "{\"ok\":false,\"err\":\"" + jsonEscape(e) + "\"}"; }

// ================= periferiche virtuali =================
#if VOS_WITH_MQTT
static int  st_mqtt() { return mqttConnected() ? 1 : mqttRunning() ? 2 : 0; }
static bool set_mqtt(bool on, String& err) {
  if (!on) { mqttStop(); return true; }
  if (!cfg.mqttHost.length()) { err = tr("Manca l'indirizzo del broker"); return false; }
  mqttStart(); return true;
}
static bool act_mqtt(const String& a, const String& arg, int role, String& out, String& err) {
  if (a == "test") {
    if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) { err = tr("MQTT non collegato"); return false; }
    out = "{\"ok\":true}"; return true;
  }
  err = tr("Azione non valida"); return false;
}
#endif
#if VOS_WITH_MESH
static int  st_mesh() { return meshRunning() ? 1 : 0; }
static bool set_mesh(bool on, String& err) { if (!on) { meshStop(); return true; } return meshStart(err); }
#endif
#if VOS_WITH_BLE
static int  st_ble() { return bleRunning() ? 1 : 0; }
static bool set_ble(bool on, String& err) { if (!on) { bleStop(); return true; } return bleStart(err); }
#endif
#if VOS_WITH_MFA
static int  st_mfa() { for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && mfaOn(i)) return 1; return 0; }
static String js_mfa() {
  int n = 0, t = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length()) { t++; if (mfaOn(i)) n++; }
  return "{\"users\":" + String(t) + ",\"on\":" + String(n) + "}";
}
#endif
#if VOS_WITH_STATS
static int  st_stat() { return cfg.statOn ? 1 : 0; }
static bool set_stat(bool on, String& err) {
  if (on && !cfg.statOn) statsReset();
  cfg.statOn = on; cfgSetOrigin("periferiche"); cfgSave(); return true;
}
#endif

// ================= hardware (Device Manager) =================
static int st_one() { return 1; }

static bool splitFirst(const String& s, String& a, String& b) {   // "a,b,c" -> a="a", b="b,c"
  int p = s.indexOf(',');
  if (p < 0) { a = s; b = ""; return false; }
  a = s.substring(0, p); b = s.substring(p + 1); return true;
}

static bool act_pin(const String& a, const String& arg, int role, String& out, String& err) {
  if (a == "state") { out = pinTestJson(); return true; }
  if (a == "map")   { out = pinMapJson(); return true; }
  if (a == "info")  { out = pinBoardJson(); return true; }
  if (a == "notes") { out = pinNotesJson(); return true; }
  if (a == "test") {                                   // arg: "gpio,azione[,pull]"
    if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    String g, rest, act, pull; splitFirst(arg, g, rest); splitFirst(rest, act, pull);
    if (!g.length() || !act.length()) { err = tr("Valori non validi"); return false; }
    if (!pinTestRequest(g.toInt(), act, pull, err)) return false;
    out = pinTestJson(); return true;
  }
  if (a == "note") {                                   // arg: "gpio,nome"
    if (role < ROLE_ADMIN) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    String g, nm; splitFirst(arg, g, nm);
    if (!g.length()) { err = tr("Valori non validi"); return false; }
    if (!pinNoteSet(g.toInt(), nm, err)) return false;
    out = pinNotesJson(); return true;
  }
  err = tr("Azione non valida"); return false;
}

static int  st_led() { return cfg.ledMode == LED_OFF ? 0 : 1; }
static bool set_led(bool on, String& err) {
  if (on && cfg.ledMode == LED_OFF) cfg.ledMode = LED_STATE;
  else if (!on) cfg.ledMode = LED_OFF;
  cfgSetOrigin("periferiche"); ledApplyConfig(); cfgSave(); return true;
}
static String js_led() {
  return "{\"mode\":" + String((int)cfg.ledMode) + ",\"color\":" + String((unsigned long)cfg.ledColor) +
         ",\"br\":" + String((int)cfg.ledBrightness) + ",\"pin\":" + String((int)cfg.ledPin) + "}";
}
static bool act_led(const String& a, const String& arg, int role, String& out, String& err) {
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (a == "identify") { ledIdentify(5000); out = "{\"ok\":true}"; return true; }
  if (a == "set") {                                    // arg: "modo,colore,luminosita" (colore in decimale)
    String m, rest, col, br; splitFirst(arg, m, rest); splitFirst(rest, col, br);
    int mode = m.toInt(), bri = br.toInt();
    if (!m.length() || mode < 0 || mode > 3 || bri < 0 || bri > 255) { err = tr("Valori non validi"); return false; }
    cfg.ledMode = mode; cfg.ledBrightness = bri;
    if (col.length()) cfg.ledColor = (uint32_t)strtoul(col.c_str(), NULL, 10) & 0xFFFFFF;
    cfgSetOrigin("periferiche"); ledApplyConfig(); cfgSave();
    out = js_led(); return true;
  }
  err = tr("Azione non valida"); return false;
}

static String js_boot() {
  Rows r;
  r.add(tr("Premuto adesso"), yn(digitalRead(VOS_PIN_BOOT) == LOW));
  r.add(tr("Pressione breve"), tr("modo aereo"));
  r.add(tr("Tenuto 2 secondi"), tr("filtro IP"));
  r.add(tr("Tenuto 8 secondi"), tr("password"));
  r.add(tr("Tenuto 20 secondi"), tr("ripristino di fabbrica"));
  return r.done();
}
static String js_usb() {
  Rows r;
  r.add(tr("Collegamento"), tr("USB nativo (seriale e JTAG)"));
  r.add(tr("Piedini"), "GPIO19 (D-), GPIO20 (D+)");
  r.add(tr("Monitor seriale aperto"), yn((bool)Serial));
  return r.done();
}
static String js_temp() {
  Rows r;
  r.add(tr("Temperatura del chip"), String(sysCpuTemp(), 1) + " C");
  r.add(tr("Sotto 80 C la velocita non cala"), tr("sopra 80 C scende a 80 MHz"));
  return r.done();
}
static String js_mem() {
  Rows r;
  size_t fi = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), ti = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  r.add(tr("RAM interna libera"), kb(fi) + " / " + kb(ti));
  r.add(tr("Pezzo piu grande"), kb(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  r.add(tr("Minimo raggiunto"), kb(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  size_t tp = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  r.add(tr("PSRAM libera"), tp ? kb(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) + " / " + kb(tp) : String(tr("assente")));
  r.add(tr("Flash del chip"), kb(ESP.getFlashChipSize()));
  r.add(tr("Programma"), kb(ESP.getSketchSize()) + " / " + kb(ESP.getSketchSize() + ESP.getFreeSketchSpace()));
  r.add(tr("File (LittleFS)"), kb(LittleFS.usedBytes()) + " / " + kb(LittleFS.totalBytes()));
  return r.done();
}
static String js_cpu() {
  Rows r;
  r.add(tr("Chip"), String(ESP.getChipModel()) + " rev " + String((int)ESP.getChipRevision()));
  r.add(tr("Core"), String((int)ESP.getChipCores()));
  r.add(tr("Velocita"), String((unsigned long)ESP.getCpuFreqMHz()) + " MHz");
  r.add(tr("Modo velocita"), cfg.cpuMhz ? String(cfg.cpuMhz) + " MHz" : String(tr("automatico")));
  r.add(tr("Carico"), String(sysCpuPercent()) + " %");
  return r.done();
}
static int st_rad() { return WiFi.getMode() != WIFI_OFF ? 1 : 0; }
static String js_rad() {
  Rows r;
  r.add(tr("Radio"), "Wi-Fi 2,4 GHz + Bluetooth LE");
  r.add(tr("Paese"), cfg.country.length() ? cfg.country : String(tr("non scelto")));
  r.add(tr("Canali ammessi"), "1-" + String(regionChannels()));
  r.add(tr("Limite del paese"), String(regionLimitDbm()) + " dBm");
  r.add(tr("Potenza in uso"), String(regionTxDbm()) + " dBm");
  r.add(tr("Antenna"), cfg.antExt ? String(tr("esterna")) + " (" + String((int)cfg.antGain) + " dBi)" : String(tr("interna")));
  r.add("MAC Wi-Fi", netMac(false));
  return r.done();
}

// ================= la tabella =================
static const DevOps DEVS[] = {
#if VOS_WITH_MQTT
  { "mqtt", "MQTT",              "Messaggi",   DEV_VIRT, "mqtt", ROLE_OPER,  st_mqtt, set_mqtt, mqttStatusJson, act_mqtt },
#endif
#if VOS_WITH_MESH
  { "mesh", "ESP-NOW",           "Radio",      DEV_VIRT, "mesh", ROLE_GUEST, st_mesh, set_mesh, meshJson,       nullptr },
#endif
#if VOS_WITH_BLE
  { "ble",  "Bluetooth",         "Radio",      DEV_VIRT, "ble",  ROLE_ADMIN, st_ble,  set_ble,  bleJson,        nullptr },
#endif
#if VOS_WITH_MFA
  { "mfa",  "MFA (due passaggi)", "Sicurezza", DEV_VIRT, "mfa",  ROLE_GUEST, st_mfa,  nullptr,  js_mfa,         nullptr },
#endif
#if VOS_WITH_STATS
  { "stat", "Statistiche",       "Misure",     DEV_VIRT, "stat", ROLE_OPER,  st_stat, set_stat, statsJson,      nullptr },
#endif
  { "pin",  "Pin (GPIO)",        "Ingressi e uscite", DEV_HW, "pin",  ROLE_GUEST, st_one, nullptr, pinsJson,    act_pin },
  { "led",  "LED di stato",      "Luce",       DEV_HW,   "led",  ROLE_GUEST, st_led,  set_led,  js_led,         act_led },
  { "boot", "Tasto BOOT",        "Pulsante",   DEV_HW,   "btn", ROLE_GUEST, st_one,  nullptr,  js_boot,        nullptr },
  { "usb",  "USB",               "Collegamenti", DEV_HW, "usb",  ROLE_GUEST, st_one,  nullptr,  js_usb,         nullptr },
  { "temp", "Temperatura del chip", "Sensori", DEV_HW,   "temp", ROLE_GUEST, st_one,  nullptr,  js_temp,        nullptr },
  { "mem",  "Memoria",           "Risorse",    DEV_HW,   "mem",  ROLE_GUEST, st_one,  nullptr,  js_mem,         nullptr },
  { "cpu",  "Processore",        "Risorse",    DEV_HW,   "cpu",  ROLE_GUEST, st_one,  nullptr,  js_cpu,         nullptr },
  { "rad",  "Radio 2,4 GHz",     "Radio",      DEV_HW,   "rad",  ROLE_GUEST, st_rad,  nullptr,  js_rad,         nullptr },
};
#define NDEV ((int)(sizeof(DEVS) / sizeof(DEVS[0])))

int devCount() { return NDEV; }
const DevOps* devAt(int i) { return (i >= 0 && i < NDEV) ? &DEVS[i] : nullptr; }
const DevOps* devFind(const String& id) {
  for (int i = 0; i < NDEV; i++) if (id == DEVS[i].id) return &DEVS[i];
  return nullptr;
}

String devListJson() {
  String j = "{\"dev\":[";
  for (int i = 0; i < NDEV; i++) {
    const DevOps& d = DEVS[i];
    if (i) j += ",";
    j += "{\"id\":\"" + String(d.id) + "\",\"label\":\"" + jsonEscape(tr(d.label)) + "\",\"cap\":\"" + jsonEscape(tr(d.cap)) +
         "\",\"kind\":" + String((int)d.kind) + ",\"tab\":\"" + String(d.tab) + "\",\"state\":" + String(d.state()) +
         ",\"sw\":" + String(d.set ? 1 : 0) + ",\"act\":" + String(d.act ? 1 : 0) + "}";
  }
  return j + "]}";
}

String devListText() {
  String t;
  for (int i = 0; i < NDEV; i++) {
    const DevOps& d = DEVS[i];
    int s = d.state();
    String id = d.id; while (id.length() < 6) id += ' ';
    t += id + (d.kind == DEV_HW ? tr("hardware ") : tr("virtuale ")) + " " + (s == 1 ? tr("acceso") : s == 2 ? tr("con problema") : tr("spento")) +
         "  " + tr(d.label) + (d.set ? "" : String(" (") + tr("solo stato") + ")") + "\n";
  }
  return t;
}

String devStatusJson(const String& id, int role) {
  const DevOps* d = devFind(id);
  if (!d || role < d->jsonRole) return "";
  return "{\"id\":\"" + String(d->id) + "\",\"label\":\"" + jsonEscape(tr(d->label)) + "\",\"kind\":" + String((int)d->kind) +
         ",\"state\":" + String(d->state()) + ",\"sw\":" + String(d->set ? 1 : 0) + ",\"data\":" + (d->json ? d->json() : String("{}")) + "}";
}

bool devSet(const String& id, bool on, String& err) {
  const DevOps* d = devFind(id);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (!d->set) { err = tr("Questa periferica non si accende o si spegne da qui"); return false; }
  bool ok = d->set(on, err);
  if (ok) vlog("PERIFERICHE: %s %s", d->id, on ? "acceso" : "spento");
  return ok;
}

bool devAct(const String& id, const String& a, const String& arg, int role, String& out, String& err) {
  const DevOps* d = devFind(id);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (!d->act) { err = tr("Questa periferica non ha azioni"); return false; }
  out = "";
  return d->act(a, arg, role, out, err);
}
