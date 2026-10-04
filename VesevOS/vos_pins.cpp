// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pins.cpp
#include "vos_i18n.h"
#include "vos_pins.h"
#include "vos_config.h"
#include "vos_log.h"
#include "vos_common.h"
#include <driver/gpio.h>
#include "vos_util.h"

#define MAX_PINS 24
static PinInfo g_pins[MAX_PINS];
static int g_n = 0;

void pinsInit() {
  g_n = 0;
  pinClaim(19, "USB", "USB D-  (nativo)", true);
  pinClaim(20, "USB", "USB D+  (nativo)", true);
}

bool pinClaim(uint8_t gpio, const char* owner, const char* note, bool fixed) {
  if (gpio < 49 && cfg.pinNote[gpio].length()) vlog("PIN: GPIO%u e segnato come collegato a '%s' ma %s lo vuole usare", (unsigned)gpio, cfg.pinNote[gpio].c_str(), owner);
  for (int i = 0; i < g_n; i++) {
    if (g_pins[i].gpio == gpio) {
      if (strcmp(g_pins[i].owner, owner) == 0) { g_pins[i].note = note; return true; }
      return false;   // gia usato da altri
    }
  }
  if (g_n >= MAX_PINS) return false;
  g_pins[g_n++] = { gpio, owner, note, fixed };
  return true;
}

void pinRelease(uint8_t gpio) {
  for (int i = 0; i < g_n; i++) {
    if (g_pins[i].gpio == gpio && !g_pins[i].fixed) {
      for (int k = i; k < g_n - 1; k++) g_pins[k] = g_pins[k + 1];
      g_n--; return;
    }
  }
}

int pinCount() { return g_n; }
const PinInfo* pinAt(int i) { return (i >= 0 && i < g_n) ? &g_pins[i] : nullptr; }

bool pinIsUsed(uint8_t gpio) {
  for (int i = 0; i < g_n; i++) if (g_pins[i].gpio == gpio) return true;
  return false;
}

String pinsJson() {
  String j = "[";
  for (int i = 0; i < g_n; i++) {
    if (i) j += ",";
    j += "{\"gpio\":" + String(g_pins[i].gpio) +
         ",\"owner\":\"" + jsonEscape(tr(g_pins[i].owner)) +
         "\",\"note\":\"" + jsonEscape(tr(g_pins[i].note)) +
         "\",\"fixed\":" + (g_pins[i].fixed ? "true" : "false") + "}";
  }
  j += "]";
  return j;
}

// ===================== Prova dei pin =====================
#define TEST_OUT_MS   20000UL   // uscite: si spengono dopo 20 s
#define TEST_READ_MS  60000UL   // lettura: dopo 60 s

enum { TA_NONE = 0, TA_HIGH, TA_LOW, TA_BLINK, TA_READ, TA_OFF };
static int g_tPin = -1;                 // pin in prova adesso (-1 = nessuno)
static int g_tAct = TA_NONE;
static uint32_t g_tEnd = 0, g_tLast = 0;
static bool g_tBlinkOn = false;
// richiesta in attesa: la applica sempre loop() (cosi non ci sono accessi in parallelo)
static volatile bool g_rqHas = false;
static volatile int g_rqPin = -1, g_rqAct = TA_NONE, g_rqPull = 0;

// ESP32-S3: i GPIO 22-25 non esistono, 26-32 sono flash/PSRAM; 33-37 sono liberi con PSRAM quad (SuperMini 2 MB)
static bool pinExists(int g) { return (g >= 0 && g <= 21) || (g >= 33 && g <= 48); }

const char* pinTestBlock(int g) {
  if (!pinExists(g)) return tr("Pin inesistente su questa scheda");
  if (g == 19 || g == 20) return tr("USB: serve al collegamento con il PC");
  if (g == 48) return tr("LED RGB della scheda");
  if (g == 0) return tr("Tasto BOOT");
  if (g == 3 || g == 45 || g == 46) return tr("Pin di avvio: meglio non toccarlo");
  if (pinIsUsed(g) && g != g_tPin) return tr("Gia usato dal sistema");
  // (se l'utente lo ha segnato come collegato a qualcosa, la prova e permessa ma la pagina avvisa)
  return nullptr;
}

bool pinTestRequest(int gpio, const String& action, const String& pull, String& err) {
  int a = TA_NONE;
  if (action == "high") a = TA_HIGH;
  else if (action == "low") a = TA_LOW;
  else if (action == "blink") a = TA_BLINK;
  else if (action == "read") a = TA_READ;
  else if (action == "off") a = TA_OFF;
  else { err = tr("Azione non valida"); return false; }
  if (a != TA_OFF) {
    const char* why = pinTestBlock(gpio);
    if (why) { err = why; return false; }
  } else if (gpio != g_tPin) { err = tr("Questo pin non e in prova"); return false; }
  g_rqPin = gpio; g_rqAct = a;
  g_rqPull = (pull == "up") ? 1 : (pull == "down") ? 2 : 0;
  g_rqHas = true;
  return true;
}

static void testStop() {
  if (g_tPin >= 0) { pinMode(g_tPin, INPUT); pinRelease(g_tPin); }
  g_tPin = -1; g_tAct = TA_NONE;
}

void pinTestTick() {
  uint32_t now = millis();
  if (g_rqHas) {
    int p = g_rqPin, a = g_rqAct, pl = g_rqPull;
    g_rqHas = false;
    if (a == TA_OFF) testStop();
    else {
      if (g_tPin != p) testStop();
      if ((!pinIsUsed(p) || p == g_tPin) && !pinTestBlock(p)) {
        pinClaim(p, "TEST", "Prova temporanea", false);
        g_tPin = p; g_tAct = a; g_tLast = now; g_tBlinkOn = true;
        g_tEnd = now + (a == TA_READ ? TEST_READ_MS : TEST_OUT_MS);
        if (a == TA_READ) pinMode(p, pl == 1 ? INPUT_PULLUP : pl == 2 ? INPUT_PULLDOWN : INPUT);
        else { pinMode(p, OUTPUT); digitalWrite(p, a == TA_LOW ? LOW : HIGH); }
      }
    }
  }
  if (g_tPin < 0) return;
  if ((int32_t)(now - g_tEnd) >= 0) { testStop(); return; }
  if (g_tAct == TA_BLINK && now - g_tLast >= 500) {
    g_tLast = now; g_tBlinkOn = !g_tBlinkOn;
    digitalWrite(g_tPin, g_tBlinkOn ? HIGH : LOW);
  }
}

String pinTestJson() {
  if (g_tPin < 0) return "{\"gpio\":-1}";
  const char* an = g_tAct == TA_HIGH ? "high" : g_tAct == TA_LOW ? "low" : g_tAct == TA_BLINK ? "blink" : "read";
  int32_t left = (int32_t)(g_tEnd - millis()) / 1000;
  if (left < 0) left = 0;
  return "{\"gpio\":" + String(g_tPin) + ",\"action\":\"" + an + "\",\"left\":" + String((int)left) +
         ",\"level\":" + String(digitalRead(g_tPin) ? 1 : 0) + "}";
}

String pinMapJson() {
  String j = "[";
  bool first = true;
  for (int g = 0; g <= 48; g++) {
    if (!pinExists(g)) continue;
    if (!first) j += ",";
    first = false;
    const char* why = pinTestBlock(g);
    String own = "";
    for (int i = 0; i < g_n; i++) if (g_pins[i].gpio == g) own = tr(g_pins[i].owner);
    j += "{\"g\":" + String(g) + ",\"note\":\"" + jsonEscape(cfg.pinNote[g]) + "\",\"ok\":" + (why ? "false" : "true") +
         ",\"why\":\"" + jsonEscape(why ? why : "") + "\",\"owner\":\"" + jsonEscape(own) + "\"}";
  }
  j += "]";
  return j;
}

// ---- rilevamento: il chip si interroga, la scheda no (i piedini saldati li conosce solo la tabella) ----
#ifndef SOC_GPIO_PIN_COUNT
#define SOC_GPIO_PIN_COUNT 49
#endif
String pinNotesJson() {
  String j = "{";
  bool first = true;
  for (int g = 0; g < 49; g++) if (cfg.pinNote[g].length()) { if (!first) j += ","; first = false; j += "\"" + String(g) + "\":\"" + jsonEscape(cfg.pinNote[g]) + "\""; }
  return j + "}";
}

bool pinNoteSet(int g, const String& name, String& err) {
  if (!pinExists(g)) { err = tr("Pin inesistente su questa scheda"); return false; }
  String n = name; n.trim();
  if (n.length() > 24) { err = tr("Nome troppo lungo (massimo 24 caratteri)"); return false; }
  for (size_t i = 0; i < n.length(); i++) { unsigned char ch = n[i]; if (ch < 32 || ch > 126 || ch == '"' || ch == '\'' || ch == '\\') { err = tr("Nome: solo lettere, numeri, spazio e simboli semplici (niente apici)"); return false; } }
  cfg.pinNote[g] = n;
  cfgSave();
  return true;
}

String pinBoardJson() {
  int valid = 0, out = 0;
  for (int g = 0; g < SOC_GPIO_PIN_COUNT; g++) {
    if (GPIO_IS_VALID_GPIO(g)) valid++;
    if (GPIO_IS_VALID_OUTPUT_GPIO(g)) out++;
  }
  String j = "{\"chip\":\"" + String(ESP.getChipModel()) + "\",\"rev\":" + String((int)ESP.getChipRevision()) +
             ",\"cores\":" + String((int)ESP.getChipCores()) + ",\"gpioCount\":" + String(SOC_GPIO_PIN_COUNT) +
             ",\"gpioValid\":" + String(valid) + ",\"gpioOut\":" + String(out) +
             ",\"board\":\"" VOS_BOARD "\",\"flash\":" + String((unsigned long)ESP.getFlashChipSize()) +
             ",\"psram\":" + String((unsigned long)ESP.getPsramSize()) + "}";
  return j;
}
