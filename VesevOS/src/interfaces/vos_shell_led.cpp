// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_led.cpp
// Comando "led_rgb1" della shell (stile shell classica) e i vecchi nomi led-color e led-bright (le regole li usano ancora).
//   led_rgb1                              stato: trigger, colore, luminosita
//   led_rgb1 trigger [nome]               senza nome mostra; con nome: none | state | heartbeat | flash | wifi | cpu | default-on   (cambiare: Admin)
//   led_rgb1 state|heartbeat|fixed|off    vecchie forme del modo, restano all'Operatore (le regole le usano)
//   led_rgb1 color RRGGBB                 colore del modo fisso e del battito (Operatore, come prima)
//   led_rgb1 color <sorgente> RRGGBB      colore di una sorgente: flash-read flash-write wifi-rx wifi-tx cpu   (Admin)
//   led_rgb1 threshold [10-95]            soglia di carico CPU del trigger cpu   (cambiare: Admin)
//   led_rgb1 brightness 0-255             luminosita (Operatore, come prima)
//   led_rgb1 test                         prova dei colori puri rosso, verde, blu, bianco (2 s ciascuno, scritti sulla seriale)   (Admin)
//   led1                             secondo LED della scheda (se c'e): senza argomento mostra il trigger (Operatore)
//   led1 on|off|blink|auto           comando manuale al posto del trigger, non salva; auto = torna il trigger salvato   (Operatore)
//   led1 trigger [nome]              come led_rgb1 trigger, per led1 (un solo colore); cambiare: Admin
//   led1 threshold [10-95]           soglia di carico CPU del trigger cpu di led1; cambiare: Admin
//   led1 brightness [0-255]          luminosita di led1 (salvata, vale in tutti i modi); cambiare: Operatore, come il LED1
// Chi puo fare cosa lo decide la tabella dei ruoli (vos_shell_roles.cpp): "led_rgb1" e dell'Operatore perche le regole lo usano; qui si
// controlla il ruolo per le forme nuove che CAMBIANO qualcosa.
#include "vos_shell_int.h"
#include "../devices/vos_led_trigger.h"
#include "../devices/vos_led2.h"
#include "../devices/vos_led_nomi.h"

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
  if (!ledTrigFromName(name.c_str(), mode)) { o.println(tr("Uso: led_rgb1 trigger none|state|heartbeat|flash|wifi|cpu|default-on")); return; }
  setMode(mode);
  o.println(trf("Trigger: %s", ledTrigName(mode)));
}

// led_rgb1 color RRGGBB (oldName = led-color). Con un nome di sorgente davanti serve l'Admin.
void color(const Word& first, const Word& second, OutSink& o, int role, bool oldName) {
  uint8_t src;
  uint32_t rgb;
  if (!oldName && sourceFromName(first, src)) {
    if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
    if (!parseHex(second, rgb)) { o.println(tr("Uso: led_rgb1 color <flash-read|flash-write|wifi-rx|wifi-tx|cpu> RRGGBB")); return; }
    cfg.ledSrcColor[src] = rgb;
    cfgSave();
    o.println(trf("Colore %s: %06lX", first.c_str(), (unsigned long)rgb));
    return;
  }
  if (!parseHex(first, rgb)) { o.println(oldName ? tr("Uso: led-color RRGGBB") : tr("Uso: led_rgb1 color RRGGBB")); return; }
  cfg.ledColor = rgb;
  cfgSave();
  o.println(trf("Colore: %06lX", (unsigned long)rgb));
}

void brightness(const Word& val, OutSink& o, bool oldName) {
  int v = val.toInt();
  if (val.empty() || v < 0 || v > 255) { o.println(oldName ? tr("Uso: led-bright 0-255") : tr("Uso: led_rgb1 brightness 0-255")); return; }
  cfg.ledBrightness = v;
  cfgSave();
  o.println(trf("Luminosita: %d", v));
}

void threshold(const Word& val, OutSink& o, int role) {
  if (val.empty()) { o.println(trf("Soglia CPU: %d%%", (int)cfg.ledCpuThr)); return; }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  long v = val.toInt();
  if (v < 10 || v > 95) { o.println(tr("Uso: led_rgb1 threshold 10-95")); return; }
  cfg.ledCpuThr = (uint8_t)v;
  cfgSave();
  o.println(trf("Soglia CPU: %d%%", (int)v));
}

void test(OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  ledTest();
  o.println(tr("Prova dei colori: rosso, verde, blu, bianco (2 secondi ciascuno). I nomi dei colori sono sulla seriale."));
}

void led2Status(OutSink& o) {
  o.println(trf("Trigger: %s", ledTrigName(cfg.led2Mode)));
  if (cfg.led2Mode == LED_CPU) o.println(trf("Soglia CPU: %d%%", (int)cfg.led2CpuThr));
  o.println(trf("Luminosita: %d", (int)cfg.led2Brightness));
  if (led2Mode() != LED2_AUTO) o.println(trf("led1: %s", led2ModeName(led2Mode())));
}

void led2Trigger(const Word& name, OutSink& o, int role) {
  if (name.empty()) {
    o.println(trf("Trigger: %s", ledTrigName(cfg.led2Mode)));
    o.println(tr("Disponibili: none state heartbeat flash wifi cpu default-on"));
    return;
  }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  uint8_t mode;
  if (!ledTrigFromName(name.c_str(), mode)) { o.println(tr("Uso: led1 trigger none|state|heartbeat|flash|wifi|cpu|default-on")); return; }
  cfg.led2Mode = mode;
  led2Set(LED2_AUTO);
  cfgSave();
  o.println(trf("Trigger: %s", ledTrigName(mode)));
}

void led2Threshold(const Word& val, OutSink& o, int role) {
  if (val.empty()) { o.println(trf("Soglia CPU: %d%%", (int)cfg.led2CpuThr)); return; }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  long v = val.toInt();
  if (v < 10 || v > 95) { o.println(tr("Uso: led1 threshold 10-95")); return; }
  cfg.led2CpuThr = (uint8_t)v;
  cfgSave();
  o.println(trf("Soglia CPU: %d%%", (int)v));
}

void led2Brightness(const Word& val, OutSink& o) {
  if (val.empty()) { o.println(trf("Luminosita: %d", (int)cfg.led2Brightness)); return; }
  int v = val.toInt();
  if (v < 0 || v > 255 || !led2SetBrightness(v)) { o.println(tr("Uso: led1 brightness 0-255")); return; }
  o.println(trf("Luminosita: %d", v));
}

void led2Cmd(const Word& a1, const char* line, OutSink& o, int role) {
  if (!led2Present()) { o.println(tr("Questa scheda non ha led1")); return; }
  if (a1.empty()) { led2Status(o); return; }
  if (a1 == "trigger") { led2Trigger(argAt(line, 2), o, role); return; }
  if (a1 == "threshold") { led2Threshold(argAt(line, 2), o, role); return; }
  if (a1 == "brightness") { led2Brightness(argAt(line, 2), o); return; }
  if (!led2SetName(a1.c_str())) { o.println(tr("Uso: led1 [on|off|blink|auto|trigger [nome]|threshold 10-95|brightness 0-255]")); return; }
  o.println(trf("led1: %s", led2ModeName(led2Mode())));
}

}  // namespace

bool shCmdLed(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  if (c == "led-color") { color(a1, Word(), o, role, true); return true; }       // vecchi nomi: le regole li usano ancora
  if (c == "led-bright") { brightness(a1, o, true); return true; }
  if (c == "led1") { led2Cmd(a1, line, o, role); return true; }
  if (c == "led" || c == "led2") {                                              // nomi vecchi: non valgono piu, l'errore dice quello nuovo
    const char* msg = ledNomeVecchioMsg(c.c_str());
    if (!msg) return false;
    o.println(msg);
    return true;
  }
  if (c != "led_rgb1") return false;
  if (!VOS_HAS_LED_RGB) { o.println(tr("Questa scheda non ha led_rgb1")); return true; }

  uint8_t mode;
  if (a1.empty()) showStatus(o);
  else if (a1 == "trigger") trigger(argAt(line, 2), o, role);
  else if (a1 == "color") color(argAt(line, 2), argAt(line, 3), o, role, false);
  else if (a1 == "brightness") brightness(argAt(line, 2), o, false);
  else if (a1 == "test") test(o, role);
  else if (a1 == "threshold") threshold(argAt(line, 2), o, role);
  else if ((a1 == "state" || a1 == "heartbeat" || a1 == "fixed" || a1 == "off") && ledTrigFromName(a1.c_str(), mode)) {
    setMode(mode);                                                               // vecchie forme: restano all'Operatore
    o.println(trf("LED: %s", a1.c_str()));
  }
  else o.println(tr("Uso: led_rgb1 [trigger [nome]|color [sorgente] RRGGBB|brightness 0-255|threshold 10-95|state|heartbeat|fixed|off]"));
  return true;
}
