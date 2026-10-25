// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_led.cpp
// Comando "led" della shell (stile shell classica) e i vecchi nomi led-color e led-bright (le regole li usano ancora).
//   led                              stato: trigger, colore, luminosita
//   led trigger [nome]               senza nome mostra; con nome: none | state | heartbeat | flash | wifi | cpu | default-on   (cambiare: Admin)
//   led state|heartbeat|fixed|off    vecchie forme del modo, restano all'Operatore (le regole le usano)
//   led color RRGGBB                 colore del modo fisso e del battito (Operatore, come prima)
//   led color <sorgente> RRGGBB      colore di una sorgente: flash-read flash-write wifi-rx wifi-tx cpu   (Admin)
//   led threshold [10-95]            soglia di carico CPU del trigger cpu   (cambiare: Admin)
//   led brightness 0-255             luminosita (Operatore, come prima)
// Chi puo fare cosa lo decide la tabella dei ruoli (vos_shell_roles.cpp): "led" e dell'Operatore perche le regole lo usano; qui si
// controlla il ruolo per le forme nuove che CAMBIANO qualcosa.
#include "vos_shell_int.h"
#include "../devices/vos_led_trigger.h"

namespace {

struct SrcName { const char* name; uint8_t src; };
const SrcName kSources[] = {
  {"flash-read", LEDSRC_FLASH_R}, {"flash-write", LEDSRC_FLASH_W},
  {"wifi-rx", LEDSRC_WIFI_RX}, {"wifi-tx", LEDSRC_WIFI_TX}, {"cpu", LEDSRC_CPU},
};

bool sourceFromName(const Word& w, uint8_t& src) {
  for (const SrcName& s : kSources) {
    if (w == s.name) { src = s.src; return true; }
  }
  return false;
}

// Esattamente 6 cifre esadecimali -> colore 0xRRGGBB
bool parseHex(const Word& w, uint32_t& out) {
  if (w.length() != 6) return false;
  for (size_t i = 0; i < 6; i++) {
    char ch = w.c_str()[i];
    bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    if (!hex) return false;
  }
  out = (uint32_t)strtoul(w.c_str(), nullptr, 16) & 0xFFFFFF;
  return true;
}

void setMode(uint8_t mode) {
  cfg.ledMode = mode;
  ledApplyConfig();
  cfgSave();
}

void showStatus(OutSink& o) {
  o.println(trf("Trigger: %s", ledTrigName(cfg.ledMode)));
  o.println(trf("Colore: %06lX", (unsigned long)(cfg.ledColor & 0xFFFFFF)));
  o.println(trf("Luminosita: %d", (int)cfg.ledBrightness));
  if (cfg.ledMode < LED_FLASH) return;
  for (const SrcName& s : kSources)
    o.println(trf("  %s: %06lX", s.name, (unsigned long)(cfg.ledSrcColor[s.src] & 0xFFFFFF)));
  if (cfg.ledMode == LED_CPU) o.println(trf("Soglia CPU: %d%%", (int)cfg.ledCpuThr));
}

void trigger(const Word& name, OutSink& o, int role) {
  if (name.empty()) {
    o.println(trf("Trigger: %s", ledTrigName(cfg.ledMode)));
    o.println(tr("Disponibili: none state heartbeat flash wifi cpu default-on"));
    return;
  }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  uint8_t mode;
  if (!ledTrigFromName(name.c_str(), mode)) { o.println(tr("Uso: led trigger none|state|heartbeat|flash|wifi|cpu|default-on")); return; }
  setMode(mode);
  o.println(trf("Trigger: %s", ledTrigName(mode)));
}

// led color RRGGBB (oldName = led-color). Con un nome di sorgente davanti serve l'Admin.
void color(const Word& first, const Word& second, OutSink& o, int role, bool oldName) {
  uint8_t src;
  uint32_t rgb;
  if (!oldName && sourceFromName(first, src)) {
    if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
    if (!parseHex(second, rgb)) { o.println(tr("Uso: led color <flash-read|flash-write|wifi-rx|wifi-tx|cpu> RRGGBB")); return; }
    cfg.ledSrcColor[src] = rgb;
    cfgSave();
    o.println(trf("Colore %s: %06lX", first.c_str(), (unsigned long)rgb));
    return;
  }
  if (!parseHex(first, rgb)) { o.println(oldName ? tr("Uso: led-color RRGGBB") : tr("Uso: led color RRGGBB")); return; }
  cfg.ledColor = rgb;
  cfgSave();
  o.println(trf("Colore: %06lX", (unsigned long)rgb));
}

void brightness(const Word& val, OutSink& o, bool oldName) {
  int v = val.toInt();
  if (val.empty() || v < 0 || v > 255) { o.println(oldName ? tr("Uso: led-bright 0-255") : tr("Uso: led brightness 0-255")); return; }
  cfg.ledBrightness = v;
  cfgSave();
  o.println(trf("Luminosita: %d", v));
}

void threshold(const Word& val, OutSink& o, int role) {
  if (val.empty()) { o.println(trf("Soglia CPU: %d%%", (int)cfg.ledCpuThr)); return; }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  long v = val.toInt();
  if (v < 10 || v > 95) { o.println(tr("Uso: led threshold 10-95")); return; }
  cfg.ledCpuThr = (uint8_t)v;
  cfgSave();
  o.println(trf("Soglia CPU: %d%%", (int)v));
}

}  // namespace

bool shCmdLed(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  if (c == "led-color") { color(a1, Word(), o, role, true); return true; }       // vecchi nomi: le regole li usano ancora
  if (c == "led-bright") { brightness(a1, o, true); return true; }
  if (c != "led") return false;

  uint8_t mode;
  if (a1.empty()) showStatus(o);
  else if (a1 == "trigger") trigger(argAt(line, 2), o, role);
  else if (a1 == "color") color(argAt(line, 2), argAt(line, 3), o, role, false);
  else if (a1 == "brightness") brightness(argAt(line, 2), o, false);
  else if (a1 == "threshold") threshold(argAt(line, 2), o, role);
  else if ((a1 == "state" || a1 == "heartbeat" || a1 == "fixed" || a1 == "off") && ledTrigFromName(a1.c_str(), mode)) {
    setMode(mode);                                                               // vecchie forme: restano all'Operatore
    o.println(trf("LED: %s", a1.c_str()));
  }
  else o.println(tr("Uso: led [trigger [nome]|color [sorgente] RRGGBB|brightness 0-255|threshold 10-95|state|heartbeat|fixed|off]"));
  return true;
}
