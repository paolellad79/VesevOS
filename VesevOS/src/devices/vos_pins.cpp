// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pins.cpp
#include "../core/vos_i18n.h"
#include "vos_pins.h"
#include "../core/vos_config.h"
#include "../core/vos_log.h"
#include "../core/vos_common.h"
#include <driver/gpio.h>
#include "../core/vos_parse.h"
#include <string_view>
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"

#define MAX_PINS 24
static PinInfo g_pins[MAX_PINS];
static int g_n = 0;

void pinsInit() {
  g_n = 0;
  pinClaim(VOS_PIN_USB_DM, "USB", "USB D-  (nativo)", true);
  pinClaim(VOS_PIN_USB_DP, "USB", "USB D+  (nativo)", true);
}

bool pinClaim(uint8_t gpio, const char* owner, const char* note, bool fixed) {
  if (gpio < VOS_GPIO_COUNT && cfg.pinNote[gpio].length()) vlog("PIN: GPIO%u e segnato come collegato a '%s' ma %s lo vuole usare", (unsigned)gpio, cfg.pinNote[gpio].c_str(), owner);
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

bool pinsJson(HeapBuf& j) {
  j.add('[');
  for (int i = 0; i < g_n; i++) {
    if (i) j.add(',');
    j.addf("{\"gpio\":%u,\"owner\":\"", (unsigned)g_pins[i].gpio);
    j.addJson(tr(g_pins[i].owner));
    j.add("\",\"note\":\"");
    j.addJson(tr(g_pins[i].note));
    j.addf("\",\"fixed\":%s}", g_pins[i].fixed ? "true" : "false");
  }
  return j.add(']');
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

// quali GPIO esistono lo dice il file di scheda (boards/)
static bool pinExists(int g) { return VOS_GPIO_EXISTS(g); }

const char* pinTestBlock(int g) {
  if (!pinExists(g)) return tr("Pin inesistente su questa scheda");
  if (g == VOS_PIN_USB_DM || g == VOS_PIN_USB_DP) return tr("USB: serve al collegamento con il PC");
  if (g == VOS_PIN_LED_RGB) return tr("LED RGB della scheda");
  if (g == VOS_PIN_BOOT) return tr("Tasto BOOT");
  if (VOS_GPIO_STRAP(g)) return tr("Pin di avvio: meglio non toccarlo");
  if (pinIsUsed(g) && g != g_tPin) return tr("Gia usato dal sistema");
  // (se l'utente lo ha segnato come collegato a qualcosa, la prova e permessa ma la pagina avvisa)
  return nullptr;
}

bool pinTestRequest(int gpio, const char* action, const char* pull, ErrBuf& err) {
  int a = TA_NONE;
  if (!strcmp(action, "high")) a = TA_HIGH;
  else if (!strcmp(action, "low")) a = TA_LOW;
  else if (!strcmp(action, "blink")) a = TA_BLINK;
  else if (!strcmp(action, "read")) a = TA_READ;
  else if (!strcmp(action, "off")) a = TA_OFF;
  else { err = tr("Azione non valida"); return false; }
  if (a != TA_OFF) {
    const char* why = pinTestBlock(gpio);
    if (why) { err = why; return false; }
  } else if (gpio != g_tPin) { err = tr("Questo pin non e in prova"); return false; }
  g_rqPin = gpio; g_rqAct = a;
  g_rqPull = !strcmp(pull, "up") ? 1 : !strcmp(pull, "down") ? 2 : 0;
  g_rqHas = true;
  return true;
}

static void testStop() {
  if (g_tPin >= 0) { drvGpioMode(g_tPin, DG_INPUT); pinRelease(g_tPin); }
  g_tPin = -1; g_tAct = TA_NONE;
}

void pinTestTick() {
  uint32_t now = drvMillis();
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
        if (a == TA_READ) drvGpioMode(p, pl == 1 ? DG_INPUT_PULLUP : pl == 2 ? DG_INPUT_PULLDOWN : DG_INPUT);
        else { drvGpioMode(p, DG_OUTPUT); drvGpioWrite(p, a != TA_LOW); }
      }
    }
  }
  if (g_tPin < 0) return;
  if ((int32_t)(now - g_tEnd) >= 0) { testStop(); return; }
  if (g_tAct == TA_BLINK && now - g_tLast >= 500) {
    g_tLast = now; g_tBlinkOn = !g_tBlinkOn;
    drvGpioWrite(g_tPin, g_tBlinkOn);
  }
}

bool pinTestJson(HeapBuf& j) {
  if (g_tPin < 0) return j.add("{\"gpio\":-1}");
  const char* an = g_tAct == TA_HIGH ? "high" : g_tAct == TA_LOW ? "low" : g_tAct == TA_BLINK ? "blink" : "read";
  int32_t left = (int32_t)(g_tEnd - drvMillis()) / 1000;
  if (left < 0) left = 0;
  return j.addf("{\"gpio\":%d,\"action\":\"%s\",\"left\":%d,\"level\":%d}", g_tPin, an, (int)left, drvGpioRead(g_tPin) ? 1 : 0);
}

bool pinMapJson(HeapBuf& j) {
  j.add('[');
  bool first = true;
  for (int g = 0; g <= VOS_GPIO_MAX; g++) {
    if (!pinExists(g)) continue;
    if (!first) j.add(',');
    first = false;
    const char* why = pinTestBlock(g);
    const char* own = "";
    for (int i = 0; i < g_n; i++) if (g_pins[i].gpio == g) own = tr(g_pins[i].owner);
    j.addf("{\"g\":%d,\"note\":\"", g);
    j.addJson(cfg.pinNote[g].c_str());
    j.addf("\",\"ok\":%s,\"why\":\"", why ? "false" : "true");
    j.addJson(why ? why : "");
    j.add("\",\"owner\":\"");
    j.addJson(own);
    j.add("\"}");
  }
  return j.add(']');
}

// ---- rilevamento: il chip si interroga, la scheda no (i piedini saldati li conosce solo la tabella) ----
#ifndef SOC_GPIO_PIN_COUNT
#define SOC_GPIO_PIN_COUNT VOS_GPIO_COUNT
#endif
bool pinNotesJson(HeapBuf& j) {
  j.add('{');
  bool first = true;
  for (int g = 0; g < VOS_GPIO_COUNT; g++) {
    if (!cfg.pinNote[g].length()) continue;
    if (!first) j.add(',');
    first = false;
    j.addf("\"%d\":\"", g);
    j.addJson(cfg.pinNote[g].c_str());
    j.add('"');
  }
  return j.add('}');
}

bool pinNoteSet(int g, std::string_view name, ErrBuf& err) {
  if (!pinExists(g)) { err = tr("Pin inesistente su questa scheda"); return false; }
  std::string_view n = trimView(name);
  if (n.size() > 24) { err = tr("Nome troppo lungo (massimo 24 caratteri)"); return false; }
  for (char c : n) {
    unsigned char ch = (unsigned char)c;
    if (ch < 32 || ch > 126 || ch == '"' || ch == '\'' || ch == '\\') { err = tr("Nome: solo lettere, numeri, spazio e simboli semplici (niente apici)"); return false; }
  }
  char buf[25];
  memcpy(buf, n.data(), n.size());
  buf[n.size()] = 0;
  cfg.pinNote[g] = buf;
  cfgSave();
  return true;
}

bool pinBoardJson(HeapBuf& j) {
  int valid = 0, out = 0;
  for (int g = 0; g < SOC_GPIO_PIN_COUNT; g++) {
    if (GPIO_IS_VALID_GPIO(g)) valid++;
    if (GPIO_IS_VALID_OUTPUT_GPIO(g)) out++;
  }
  j.addf("{\"chip\":\"%s\",\"rev\":%d,\"cores\":%d,\"gpioCount\":%d,\"gpioValid\":%d,\"gpioOut\":%d,\"board\":\"" VOS_BOARD "\",\"flash\":%lu,\"psram\":%lu",
         drvChipModel(), (int)drvChipRevision(), (int)drvChipCores(), SOC_GPIO_PIN_COUNT, valid, out,
         (unsigned long)drvFlashSize(), (unsigned long)drvPsramTotal());
  // dal file di scheda: la pagina non scrive piu a mano numeri di pin
  j.addf(",\"gmax\":%d,\"led\":%d,\"ledType\":\"" VOS_LED_TYPE "\",\"boot\":%d,\"usb\":[%d,%d],\"adc\":[%d,%d],\"adc2\":[%d,%d],\"jtag\":[%d,%d],\"uart\":[%d,%d],\"psq\":[%d,%d],\"strap\":[",
         VOS_GPIO_MAX, VOS_PIN_LED_RGB, VOS_PIN_BOOT, VOS_PIN_USB_DM, VOS_PIN_USB_DP, VOS_ADC_FIRST, VOS_ADC_LAST, VOS_ADC2_FIRST, VOS_ADC2_LAST,
         VOS_JTAG_FIRST, VOS_JTAG_LAST, VOS_PIN_UART_TX, VOS_PIN_UART_RX, VOS_PSRAMQ_FIRST, VOS_PSRAMQ_LAST);
  bool first = true;
  for (int g = 0; g <= VOS_GPIO_MAX; g++) {
    if (!VOS_GPIO_EXISTS(g) || !(VOS_GPIO_STRAP(g) || g == VOS_PIN_BOOT)) continue;
    j.addf(first ? "%d" : ",%d", g);
    first = false;
  }
  return j.add("],\"lay\":" VOS_BOARD_LAYOUT "}");
}
