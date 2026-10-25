// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dev.cpp
// Registro delle periferiche (virtuali e hardware) con un'unica interfaccia: stato, acceso/spento, dettagli, azioni.
#include <math.h>
#include "vos_dev.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_parse.h"
#include "../core/vos_time.h"
#include <string>
#include <string_view>
#include "../core/vos_log.h"
#include "../core/vos_sys.h"
#include "../core/vos_cpupm.h"
#include "../drivers/vos_drv_console.h"
#include "../drivers/vos_drv_wifi.h"
#include "vos_led.h"
#include "vos_pins.h"
#include "vos_io.h"
#include "../core/vos_region.h"
#include "../core/vos_service.h"
#include "../drivers/vos_drv_console.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_fs.h"
#include <esp_heap_caps.h>
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_ota.h"

// ---- testi del registro (le chiavi per le traduzioni: la tabella usa i testi cosi come sono) ----
[[maybe_unused]] static void devKeys() {
  tr("Messaggi"); tr("Radio"); tr("Sicurezza"); tr("Misure"); tr("Ingressi e uscite"); tr("Luce"); tr("Pulsante");
  tr("Collegamenti"); tr("Sensori"); tr("Risorse");
  tr("Bluetooth"); tr("ESP-NOW"); tr("Statistiche"); tr("MFA (due passaggi)");
  tr("Pin (GPIO)"); tr("LED di stato"); tr("Tasto BOOT"); tr("USB"); tr("Temperatura del chip"); tr("Memoria"); tr("Processore"); tr("Radio 2,4 GHz");
}

// ---- righe "etichetta / valore" per le schede dell'hardware ----
struct Rows {
  HeapBuf& j;
  bool first = true;
  explicit Rows(HeapBuf& out) : j(out) { j.add("{\"rows\":["); }
  void add(const char* k, const char* v) {
    if (!first) j.add(',');
    first = false;
    j.add("[\""); j.addJson(k); j.add("\",\""); j.addJson(v); j.add("\"]");
  }
  // il valore si compone con printf (es. "%lu KB / %lu KB")
  void addf(const char* k, const char* fmt, ...) __attribute__((format(printf, 3, 4))) {
    char v[96];
    va_list ap; va_start(ap, fmt);
    vsnprintf(v, sizeof(v), fmt, ap);
    va_end(ap);
    add(k, v);
  }
  bool done() { j.add("]}"); return !j.oom() && !j.over(); }
  // chiude aggiungendo "lv": un numero che la pagina usa per colorare lo stato (0 normale, 1 calda, 2 alta)
  bool doneLevel(int lv) { j.add("],\"lv\":"); j.addUint((uint32_t)lv); j.add('}'); return !j.oom() && !j.over(); }
};
static unsigned long kb(size_t b) { return (unsigned long)(b / 1024); }
static const char* yn(bool b) { return b ? tr("si") : tr("no"); }

static int st_one() { return 1; }

// "a,b,c" -> a = "a", rest = "b,c" (senza virgola: a = tutto, rest vuoto)
static std::string_view splitFirst(std::string_view s, std::string_view& rest) {
  size_t p = s.find(',');
  if (p == std::string_view::npos) { rest = std::string_view(); return s; }
  rest = s.substr(p + 1);
  return s.substr(0, p);
}
// Un pezzo di testo con lo zero finale (le funzioni che lo ricevono vogliono un const char*). Piccolo: 31 caratteri al massimo.
struct Piece {
  char t[32];
  explicit Piece(std::string_view v) { size_t n = v.size() < sizeof(t) - 1 ? v.size() : sizeof(t) - 1; memcpy(t, v.data(), n); t[n] = 0; }
};

static bool act_pin(const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  if (!strcmp(act, "state")) return pinTestJson(out);
  if (!strcmp(act, "map"))   return pinMapJson(out);
  if (!strcmp(act, "info"))  return pinBoardJson(out);
  if (!strcmp(act, "notes")) return pinNotesJson(out);
  if (!strcmp(act, "test")) {                          // arg: "gpio,azione[,pull]"
    if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    std::string_view rest, pull;
    std::string_view g = splitFirst(arg, rest);
    std::string_view action = splitFirst(rest, pull);
    if (g.empty() || action.empty()) { err = tr("Valori non validi"); return false; }
    Piece a(action), p(pull);
    if (!pinTestRequest((int)viewToLong(g), a.t, p.t, err)) return false;
    return pinTestJson(out);
  }
  if (!strcmp(act, "note")) {                          // arg: "gpio,nome"
    if (role < ROLE_ADMIN) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    std::string_view nm;
    std::string_view g = splitFirst(arg, nm);
    if (g.empty()) { err = tr("Valori non validi"); return false; }
    if (!pinNoteSet((int)viewToLong(g), nm, err)) return false;
    return pinNotesJson(out);
  }
  err = tr("Azione non valida"); return false;
}

static int  st_led() { return cfg.ledMode == LED_OFF ? 0 : 1; }
static bool set_led(bool on, ErrBuf& err) {
  if (on && cfg.ledMode == LED_OFF) cfg.ledMode = LED_STATE;
  else if (!on) cfg.ledMode = LED_OFF;
  cfgSetOrigin("periferiche"); ledApplyConfig(); cfgSave(); return true;
}
static bool js_led(HeapBuf& out) {
  return out.addf("{\"mode\":%d,\"color\":%lu,\"br\":%d,\"pin\":%d}", (int)cfg.ledMode, (unsigned long)cfg.ledColor, (int)cfg.ledBrightness, (int)cfg.ledPin);
}
static bool act_led(const char* act, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
  if (!strcmp(act, "identify")) { ledIdentify(5000); return out.add("{\"ok\":true}"); }
  if (!strcmp(act, "set")) {                           // arg: "modo,colore,luminosita" (colore in decimale)
    std::string_view rest, br;
    std::string_view m = splitFirst(arg, rest);
    std::string_view col = splitFirst(rest, br);
    int mode = (int)viewToLong(m), bri = (int)viewToLong(br);
    if (m.empty() || mode < 0 || mode >= LED_MODE_COUNT || bri < 0 || bri > 255) { err = tr("Valori non validi"); return false; }
    if (mode >= LED_FLASH && mode != cfg.ledMode && role < ROLE_ADMIN) { err = tr("Il tuo ruolo non permette questo comando"); return false; }   // flash, wifi, cpu: solo Admin
    cfg.ledMode = mode; cfg.ledBrightness = bri;
    if (!col.empty()) { Piece c(col); cfg.ledColor = (uint32_t)strtoul(c.t, NULL, 10) & 0xFFFFFF; }
    cfgSetOrigin("periferiche"); ledApplyConfig(); cfgSave();
    return js_led(out);
  }
  err = tr("Azione non valida"); return false;
}

static bool js_boot(HeapBuf& out) {
  Rows r(out);
  r.add(tr("Premuto adesso"), yn(!drvGpioRead(VOS_PIN_BOOT)));
  r.add(tr("Click veloci"), tr("1, 2 o 3: azioni scelte qui sopra"));
  r.add(tr("Tenuto 2 secondi"), tr("filtro IP"));
  r.add(tr("Tenuto 8 secondi"), tr("password"));
  r.add(tr("Tenuto 20 secondi"), tr("ripristino di fabbrica"));
  return r.done();
}
static bool js_usb(HeapBuf& out) {
  Rows r(out);
  r.add(tr("Collegamento"), tr("USB nativo (seriale e JTAG)"));
  r.add(tr("Piedini"), "GPIO19 (D-), GPIO20 (D+)");
  r.add(tr("Monitor seriale aperto"), yn(drvConIsOpen()));
  return r.done();
}
// la vecchia voce "temp" resta come alias: stesse righe della sezione Temperatura della voce Processore
static bool js_temp(HeapBuf& out) {
  Rows r(out);
  char tt[16];
  fmtTemp(sysCpuTemp(), tt, sizeof tt);
  r.add(tr("Temperatura del chip"), tt);
  r.add(tr("Sotto 80 C la velocita non cala"), tr("sopra 80 C scende a 80 MHz"));
  return r.done();
}
static bool js_mem(HeapBuf& out) {
  Rows r(out);
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  r.addf(tr("RAM interna libera"), "%lu KB / %lu KB", kb(heap_caps_get_free_size(caps)), kb(heap_caps_get_total_size(caps)));
  r.addf(tr("Pezzo piu grande"), "%lu KB", kb(heap_caps_get_largest_free_block(caps)));
  r.addf(tr("Minimo raggiunto"), "%lu KB", kb(heap_caps_get_minimum_free_size(caps)));
  size_t tp = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  if (tp) r.addf(tr("PSRAM libera"), "%lu KB / %lu KB", kb(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), kb(tp));
  else r.add(tr("PSRAM libera"), tr("assente"));
  r.addf(tr("Flash del chip"), "%lu KB", kb(drvFlashSize()));
  r.addf(tr("Programma"), "%lu KB / %lu KB", kb(drvSketchSize()), kb(drvSketchSize() + drvFreeSketchSpace()));
  r.addf(tr("File (LittleFS)"), "%lu KB / %lu KB", kb(drvFsUsed()), kb(drvFsTotal()));
  return r.done();
}
// voce unica Processore: prima i dati che cambiano (temperatura, velocita, carico), poi quelli fissi (chip, core, modo)
static bool js_cpu(HeapBuf& out) {
  Rows r(out);
  float t = sysCpuTemp();
  int lv = cpuPmThermal() >= THERM_REDUCED ? 2 : cpuPmThermal() == THERM_WARN ? 1 : 0;   // soglie in core/vos_cpupm.cpp
  char tt[16];
  fmtTemp(t, tt, sizeof tt);
  r.add(tr("Temperatura"), tt);
  r.add(tr("Stato termico"), lv == 2 ? tr("alta: velocita ridotta") : lv == 1 ? tr("calda") : tr("normale"));
  r.addf(tr("Velocita"), "%lu MHz", (unsigned long)drvCpuMhz());
  r.addf(tr("Carico"), "%d %%", (int)sysCpuPercent());
  r.addf(tr("Chip"), "%s rev %d", drvChipModel(), (int)drvChipRevision());
  r.addf(tr("Core"), "%d", (int)drvChipCores());
  if (cfg.cpuMhz) r.addf(tr("Governor"), "%d MHz", (int)cfg.cpuMhz);
  else r.add(tr("Governor"), cpuChoiceName());
  r.addf(tr("Tetto"), "%lu MHz", (unsigned long)cpuPmLimitMhz());
  r.add(tr("Motivo"), cpuPmWhyText(cpuPmWhy()));
  if (!cfg.cpuMhz) r.addf(tr("Parametri"), tr("salita %d%%, discesa %d%%, attesa %d s"), (int)cfg.cpuUp, (int)cfg.cpuDown, (int)cfg.cpuHold);
  return r.doneLevel(lv);
}
static int st_rad() { return drvWifiMode() != DW_OFF ? 1 : 0; }
static bool js_rad(HeapBuf& out) {
  Rows r(out);
  r.add(tr("Radio"), "Wi-Fi 2,4 GHz + Bluetooth LE");
  r.add(tr("Paese"), cfg.country.length() ? cfg.country.c_str() : tr("non scelto"));
  r.addf(tr("Canali ammessi"), "1-%d", regionChannels());
  r.addf(tr("Limite del paese"), "%d dBm", (int)regionLimitDbm());
  r.addf(tr("Potenza in uso"), "%d dBm", (int)regionTxDbm());
  if (cfg.antExt) r.addf(tr("Antenna"), "%s (%d dBi)", tr("esterna"), (int)cfg.antGain);
  else r.add(tr("Antenna"), tr("interna"));
  char mac[24];
  drvWifiMac(false, mac, sizeof mac);
  r.add("MAC Wi-Fi", mac);
  return r.done();
}

// ================= la tabella =================
static const DevOps DEVS[] = {
  { "pin",  "Pin (GPIO)",        "Ingressi e uscite", DEV_HW, "pin",  ROLE_GUEST, st_one, nullptr, pinsJson,    act_pin },
  { "led",  "LED di stato",      "Luce",       DEV_HW,   "led",  ROLE_GUEST, st_led,  set_led,  js_led,         act_led },
  { "boot", "Tasto BOOT",        "Pulsante",   DEV_HW,   "btn", ROLE_GUEST, st_one,  nullptr,  js_boot,        nullptr },
  { "usb",  "USB",               "Collegamenti", DEV_HW, "usb",  ROLE_GUEST, st_one,  nullptr,  js_usb,         nullptr },
  { "temp", "Temperatura del chip", "Sensori", DEV_HW,   "cpu",  ROLE_GUEST, st_one,  nullptr,  js_temp,        nullptr },
  { "mem",  "Memoria",           "Risorse",    DEV_HW,   "mem",  ROLE_GUEST, st_one,  nullptr,  js_mem,         nullptr },
  { "cpu",  "Processore",        "Risorse",    DEV_HW,   "cpu",  ROLE_GUEST, st_one,  nullptr,  js_cpu,         nullptr },
  { "rad",  "Radio 2,4 GHz",     "Radio",      DEV_HW,   "rad",  ROLE_GUEST, st_rad,  nullptr,  js_rad,         nullptr },
};
#define NHW ((int)(sizeof(DEVS) / sizeof(DEVS[0])))

// periferiche virtuali dei package (MQTT, ESP-NOW, Bluetooth, MFA, statistiche): le iscrive il package (devSetVirtual), stanno in testa
static const DevOps* g_virt = nullptr;
static int g_nvirt = 0;
void devSetVirtual(const DevOps* list, int n) { g_virt = list; g_nvirt = list ? n : 0; }

#define NDEV (g_nvirt + NHW)
int devCount() { return NDEV; }
const DevOps* devAt(int i) { return (i < 0 || i >= NDEV) ? nullptr : i < g_nvirt ? &g_virt[i] : &DEVS[i - g_nvirt]; }
const DevOps* devFind(const char* id) {
  for (int i = 0; i < NDEV; i++) if (!strcmp(id, devAt(i)->id)) return devAt(i);
  return nullptr;
}

bool devListJson(HeapBuf& j) {
  j.add("{\"dev\":[");
  for (int i = 0; i < NDEV; i++) {
    const DevOps& d = *devAt(i);
    if (i) j.add(',');
    j.addf("{\"id\":\"%s\",\"label\":\"", d.id);
    j.addJson(tr(d.label));
    j.add("\",\"cap\":\"");
    j.addJson(tr(d.cap));
    j.addf("\",\"kind\":%d,\"tab\":\"%s\",\"state\":%d,\"sw\":%d,\"act\":%d}", (int)d.kind, d.tab, d.state(), d.set ? 1 : 0, d.act ? 1 : 0);
  }
  for (int k = 0; k < ioCount(); k++) {                 // periferiche create dall'utente (classi)
    const char* n = ioNameAt(k);
    if (NDEV || k) j.add(',');
    j.addf("{\"id\":\"%s\",\"label\":\"%s (", n, n);
    j.addJson(tr(ioClassName(n)));
    j.add(")\",\"cap\":\"");
    j.addJson(tr("Periferiche"));
    j.addf("\",\"kind\":1,\"tab\":\"io\",\"state\":%d,\"sw\":%d,\"act\":1}", ioState(n), ioCanSwitch(n) ? 1 : 0);
  }
  return j.add("]}");
}

// Testo per la shell, una riga per periferica. Il nome si allarga a 6 colonne.
bool devListText(HeapBuf& t) {
  for (int i = 0; i < NDEV; i++) {
    const DevOps& d = *devAt(i);
    int s = d.state();
    t.addf("%-6s%s %s  %s%s%s%s\n", d.id, d.kind == DEV_HW ? tr("hardware ") : tr("virtuale "),
           s == 1 ? tr("acceso") : s == 2 ? tr("con problema") : tr("spento"), tr(d.label),
           d.set ? "" : " (", d.set ? "" : tr("solo stato"), d.set ? "" : ")");
  }
  for (int k = 0; k < ioCount(); k++) {
    const char* n = ioNameAt(k);
    char st[48];
    snprintf(st, sizeof st, "%s", ioState(n) == 1 ? tr("acceso") : tr("spento"));
    if (!strcmp(ioClassName(n), "i2c")) {
      int tv = 0; ErrBuf e;
      if (ioGet(n, tv, e)) snprintf(st, sizeof st, "%.1f C", tv / 100.0f);
      else snprintf(st, sizeof st, "%s", tr("non risponde"));
    }
    t.addf("%-6s%s %s  %s (%s)\n", n, tr("hardware "), st, n, tr(ioClassName(n)));
  }
  return !t.oom() && !t.over();
}

bool devStatusJson(const char* id, int role, HeapBuf& j) {
  if (ioHas(id)) {
    if (role < ROLE_OPER) return false;
    int v = 0; ErrBuf e; ioGet(id, v, e);
    return j.addf("{\"id\":\"%s\",\"kind\":1,\"class\":\"%s\",\"state\":%d,\"sw\":%d,\"data\":{\"v\":%d}}",
                  id, ioClassName(id), ioState(id), ioCanSwitch(id) ? 1 : 0, v);
  }
  const DevOps* d = devFind(id);
  if (!d || role < d->jsonRole) return false;
  j.addf("{\"id\":\"%s\",\"label\":\"", d->id);
  j.addJson(tr(d->label));
  j.addf("\",\"kind\":%d,\"state\":%d,\"sw\":%d,\"data\":", (int)d->kind, d->state(), d->set ? 1 : 0);
  if (d->json) d->json(j); else j.add("{}");
  return j.add('}');
}

// {"v":N} nella risposta di devAct
static bool addValue(HeapBuf& out, int v) { return out.addf("{\"v\":%d}", v); }

bool devSet(const char* id, bool on, ErrBuf& err) {
  if (ioHas(id)) return ioSet(id, on ? 1 : 0, ROLE_ADMIN, err);       // chi arriva qui e gia Admin (percorso e shell)
  const DevOps* d = devFind(id);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (!d->set) { err = tr("Questa periferica non si accende o si spegne da qui"); return false; }
  bool ok = d->set(on, err);
  if (ok) vlog("PERIFERICHE: %s %s", d->id, on ? "acceso" : "spento");
  return ok;
}

bool devAct(const char* id, const char* a, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  if (ioHas(id)) {
    if (!strcmp(a, "get")) { int v = 0; if (!ioGet(id, v, err)) return false; return addValue(out, v); }
    if (!strcmp(a, "set") || !strcmp(a, "toggle")) {
      int v = 0;
      if (!strcmp(a, "toggle")) { ioGet(id, v, err); v = !v; } else v = atol(arg) ? 1 : 0;
      if (!ioSet(id, v, role, err)) return false;
      return addValue(out, v);
    }
    if (!strcmp(a, "pulse")) { if (!ioPulse(id, (uint32_t)atol(arg), role, err)) return false; return out.add("{\"ok\":true}"); }
    if (!strcmp(a, "blink")) { if (!ioBlink(id, (uint32_t)atol(arg), role, err)) return false; return out.add("{\"ok\":true}"); }
    err = tr("Azione non valida"); return false;
  }
  const DevOps* d = devFind(id);
  if (!d) { err = tr("Periferica sconosciuta"); return false; }
  if (!d->act) { err = tr("Questa periferica non ha azioni"); return false; }
  return d->act(a, arg, role, out, err);
}

bool devOffId(const char* id) { ErrBuf e; return devSet(id, false, e); }
