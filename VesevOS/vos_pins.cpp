// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_pins.cpp
#include "vos_i18n.h"
#include "vos_pins.h"
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

static bool pinExists(int g) { return (g >= 0 && g <= 21) || (g >= 38 && g <= 48); }

const char* pinTestBlock(int g) {
  if (!pinExists(g)) return tr("Pin inesistente su questa scheda");
  if (g == 19 || g == 20) return tr("USB: serve al collegamento con il PC");
  if (g == 48) return tr("LED RGB della scheda");
  if (g == 0) return tr("Tasto BOOT");
  if (g == 3 || g == 45 || g == 46) return tr("Pin di avvio: meglio non toccarlo");
  if (pinIsUsed(g) && g != g_tPin) return tr("Gia usato dal sistema");
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
    j += "{\"g\":" + String(g) + ",\"ok\":" + (why ? "false" : "true") +
         ",\"why\":\"" + jsonEscape(why ? why : "") + "\",\"owner\":\"" + jsonEscape(own) + "\"}";
  }
  j += "]";
  return j;
}
