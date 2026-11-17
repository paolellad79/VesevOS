// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led.cpp
#include "vos_led.h"
#include "../drivers/vos_drv_task.h"
#include "../core/vos_config.h"
#include "../core/vos_sys.h"
#include "vos_pins.h"
#include "vos_led_trigger.h"
#include "vos_led2.h"
#include "../core/vos_apwin.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_wd.h"
#include "../core/vos_diag.h"
#include <math.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_led.h"
#include "../core/vos_clamp.h"

static volatile NetState g_net = NET_BOOT;
static volatile bool g_fault = false;
static volatile bool g_reconf = true;
static uint8_t g_pin = VOS_PIN_LED_RGB;

void ledSetNetState(NetState s) { g_net = s; led2SetNetState(s); }
void ledSetFault(bool on) { g_fault = on; led2SetFault(on); }
static volatile uint32_t g_identUntil = 0;   // 0 = spento, altrimenti millis() di fine
void ledIdentify(uint32_t ms) { g_identUntil = drvMillis() + ms; if (!g_identUntil) g_identUntil = 1; }
void ledApplyConfig() { g_reconf = true; }
static volatile bool g_setup = false;
static volatile int g_alarm = 0, g_hold = 0;
void ledSetSetup(bool on) { g_setup = on; led2SetSetup(on); }
void ledSetAlarm(int level) { g_alarm = level; }
void ledSetHold(int stage) { g_hold = stage; }
static volatile int g_wps = 0;
void ledSetWps(int stage) { g_wps = stage; }
static volatile int32_t g_script = -1;         // colore imposto da uno script (-1 = nessuno)
// Prova dei colori (shell "led test"): rosso, verde, blu e bianco puri, 2 s ciascuno, e sulla seriale quale colore si invia.
static volatile uint32_t g_testAt = 0;           // millis() di inizio; 0 = nessuna prova
void ledTest() { g_testAt = drvMillis(); if (!g_testAt) g_testAt = 1; }
void ledScript(int32_t rgb) { g_script = rgb < 0 ? -1 : (rgb & 0xFFFFFF); }

// tinta 0..359 -> colore pieno
static void hue(uint32_t h, uint8_t& r, uint8_t& g, uint8_t& b) {
  uint32_t f = h % 60, q = 255 * f / 60;
  switch ((h / 60) % 6) {
    case 0: r = 255; g = q; b = 0; break;
    case 1: r = 255 - q; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = q; break;
    case 3: r = 0; g = 255 - q; b = 255; break;
    case 4: r = q; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - q; break;
  }
}

static void put(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  uint16_t R = (uint16_t)r * bright / 255;
  uint16_t G = (uint16_t)g * bright / 255;
  uint16_t B = (uint16_t)b * bright / 255;
  drvLedWrite(g_pin, R, G, B);
  static int s_loggedPin = -1;                       // un solo messaggio per pin, quando il driver si arrende
  if (drvLedAbsent() && s_loggedPin != g_pin) {
    s_loggedPin = g_pin;
    vlog(tr("ATTENZIONE: LED RGB non risponde sul pin %d, smetto di riprovare"), (int)g_pin);
  }
}

static void stateColor(NetState s, uint8_t& r, uint8_t& g, uint8_t& b) {
  switch (s) {
    case NET_BOOT:       r = 255; g = 120; b = 0;   break;  // arancio
    case NET_AP:         r = 0;   g = 0;   b = 255; break;  // blu
    case NET_CLIENT_TRY: r = 255; g = 255; b = 0;   break;  // giallo
    case NET_CLIENT_OK:  r = 0;   g = 255; b = 0;   break;  // verde
    case NET_AIR:        r = 170; g = 0;   b = 255; break;  // viola (modalita aereo)
    case NET_ATTESA:     r = 255; g = 120; b = 0;   break;  // arancio (prima accensione: in attesa del tasto)
  }
}

// I modi che mostrano lo stato di rete come colore di fondo (e quindi anche gli allarmi): stato, flash, wifi, cpu
static bool stateBased(uint8_t mode) {
  return mode == LED_STATE || mode == LED_FLASH || mode == LED_WIFI || mode == LED_CPU;
}

static void ledTask(void*) {
  wdWatch("led", wdLimit("led"));                     // base 30 s
  for (;;) {
    wdBeat("led");
    led2Tick();                                       // LED2 (se la scheda lo ha): resa del suo modo
    if (!VOS_HAS_LED_RGB) { drvDelayMs(20); continue; }   // scheda senza LED RGB: qui non c'e altro da pilotare
    if (g_reconf) {
      g_reconf = false;
      if (g_pin != cfg.ledPin) { pinRelease(g_pin); g_pin = cfg.ledPin; }
      pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
    }
    uint32_t now = drvMillis();
    // tasto BOOT tenuto premuto: il colore dice cosa succede se lo lasci ora (ha la precedenza su tutto)
    if (g_hold) {
      static const uint8_t HC[4][3] = {{0, 0, 0}, {0, 160, 255}, {255, 200, 0}, {255, 0, 0}};
      put(HC[g_hold][0], HC[g_hold][1], HC[g_hold][2], cfg.ledBrightness < 80 ? 80 : cfg.ledBrightness);
      vTaskDelay(pdMS_TO_TICKS(30)); continue;
    }
    if (g_testAt) {
      static int s_step = -1;
      int step = (int)((now - g_testAt) / 2000);
      if (step < 4) {
        if (step != s_step) {
          s_step = step;
          if (step == 0) vlog(tr("LED prova: rosso ff0000"));
          else if (step == 1) vlog(tr("LED prova: verde 00ff00"));
          else if (step == 2) vlog(tr("LED prova: blu 0000ff"));
          else vlog(tr("LED prova: bianco ffffff"));
        }
        static const uint8_t TC[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
        put(TC[step][0], TC[step][1], TC[step][2], 255);
        drvDelayMs(30); continue;
      }
      g_testAt = 0; s_step = -1;
      vlog(tr("LED prova finita: torna il modo normale"));
    }
    if (g_identUntil) {
      if ((int32_t)(g_identUntil - now) > 0) {
        // arcobaleno veloce ("trova questa scheda")
        uint8_t r, g, b; hue((now / 4) % 360, r, g, b);
        put(r, g, b, cfg.ledBrightness < 80 ? 80 : cfg.ledBrightness);
        vTaskDelay(pdMS_TO_TICKS(20)); continue;
      }
      g_identUntil = 0;
    }
#ifndef VOS_PIN_LED2     // codice della diagnostica sul LED a colori: solo se la scheda non ha un LED semplice (S3); sulla C6 lo mostra il LED2 (vos_led2.cpp)
    { bool dAcceso; uint8_t dr, dg, db;
      if (diagLedComanda(now, cfg.ledMode == LED_OFF, &dAcceso, &dr, &dg, &db)) {
        put(dr, dg, db, dAcceso ? (cfg.ledBrightness < 80 ? 80 : cfg.ledBrightness) : 0);
        drvDelayMs(20); continue;
      } }
#endif
    if (g_fault) {
      put(255, 0, 0, ((now / 150) & 1) ? cfg.ledBrightness : 0);
      vTaskDelay(pdMS_TO_TICKS(30)); continue;
    }
    // WPS: blu che sale e scende piano (3 s, senza lampeggi), poi verde o rosso fisso per qualche secondo
    { int w = g_wps;
      if (w) {
        uint8_t br = cfg.ledBrightness < 60 ? 60 : cfg.ledBrightness;
        if (w == 1) { float v = 0.15f + 0.85f * (0.5f + 0.5f * sinf((float)(now % 3000) * 6.2831853f / 3000.0f)); put(0, 90, 255, (uint8_t)(br * v)); }
        else if (w == 2) put(0, 255, 0, br); else put(255, 0, 0, br);
        vTaskDelay(pdMS_TO_TICKS(30)); continue;
      } }
    // colore imposto da uno script (funzioni VesevOS): vale al posto del modo scelto, ma non copre errore grave, tasto BOOT e "trova questa scheda"
    { int32_t sc = g_script;
      if (sc >= 0) { put((sc >> 16) & 255, (sc >> 8) & 255, sc & 255, cfg.ledBrightness); vTaskDelay(pdMS_TO_TICKS(20)); continue; } }
    // prima configurazione: in attesa del tasto BOOT (radio spenta) respiro lento arancio (2 s); con la finestra aperta (hotspot acceso) arancio fisso
    if (g_setup) {
      uint32_t ph = now % 2000, lv = ph < 1000 ? ph : 2000 - ph;   // 0..1000
      int lum = apWinIsOpen() ? cfg.ledBrightness : (int)(lv * cfg.ledBrightness / 1000);
      if (lum < 20) lum = 20;
      put(255, 160, 0, (uint8_t)lum);
      vTaskDelay(pdMS_TO_TICKS(40)); continue;
    }
    // allarmi della configurazione: lampo breve ogni 4 s (solo nei modi che mostrano lo stato)
    if (g_alarm && stateBased(cfg.ledMode) && (now % 4000) < 200) {
      if (g_alarm >= 2) put(255, 0, 0, cfg.ledBrightness); else put(255, 110, 0, cfg.ledBrightness);
      vTaskDelay(pdMS_TO_TICKS(20)); continue;
    }
    // lampi di attivita (modi flash e wifi): coprono il colore di fondo ma non gli allarmi; al massimo 3 al secondo
    { uint8_t ar, ag, ab;
      if (ledTrigOverlay(now, ar, ag, ab)) { put(ar, ag, ab, cfg.ledBrightness); vTaskDelay(pdMS_TO_TICKS(20)); continue; } }
    uint8_t r = 0, g = 0, b = 0;
    switch (cfg.ledMode) {
      case LED_OFF: put(0, 0, 0, 0); vTaskDelay(pdMS_TO_TICKS(20)); continue;
      case LED_FIXED:
        put((cfg.ledColor >> 16) & 255, (cfg.ledColor >> 8) & 255, cfg.ledColor & 255, cfg.ledBrightness);
        vTaskDelay(pdMS_TO_TICKS(20)); continue;
      case LED_HEARTBEAT:
        r = (cfg.ledColor >> 16) & 255; g = (cfg.ledColor >> 8) & 255; b = cfg.ledColor & 255;
        break;
      default:                                   // stato, flash, wifi, cpu: il fondo e il colore dello stato di rete
        stateColor(g_net, r, g, b);
        break;
    }
    if (cfg.ledMode == LED_CPU) ledTrigCpuMix(r, g, b);       // carico sopra la soglia: il colore vira verso quello della CPU
    // battito: periodo piu corto se la CPU e piu carica
    int cpu = sysCpuPercent();
    uint32_t period = 1600 - 11 * vosClamp(cpu, 0, 100);
    float v = ledTrigBeat(now, period);
    uint8_t br = (uint8_t)(cfg.ledBrightness * (0.08f + 0.92f * v));
    put(r, g, b, br);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

static void ledStart() {
  g_reconf = true;
  drvTaskCreate(ledTask, "led", VOS_PILA_LED, NULL, 1, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 1 fisso)
}

void ledInit() {
  g_pin = cfg.ledPin;
  if (VOS_HAS_LED_RGB) pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
  led2Init();
  sysTaskRegister("led", ledStart);
  ledStart();
}

void ledShutdown() {
  TaskHandle_t h = xTaskGetHandle("led");
  if (h) vTaskDelete(h);
  led2Shutdown();
  if (VOS_HAS_LED_RGB) drvLedWrite(g_pin, 0, 0, 0);
}
