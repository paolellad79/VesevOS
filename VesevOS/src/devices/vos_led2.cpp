// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led2.cpp
// Secondo LED a un solo colore con gli stessi modi e la stessa luminosita (0-255, PWM) del LED1 (vedi vos_led2.h).
// Il pin viene dal file di scheda (VOS_PIN_LED2, livello attivo VOS_LED2_ACTIVE_LOW) e si prenota nel registro dei pin come pin fisso.
// Lampi, carico CPU e battito riusano le funzioni del LED1 (vos_led_trigger): qui c'e solo la resa a un solo colore.
#include "vos_led2.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_sys.h"
#include "../core/vos_apwin.h"
#include "../core/vos_diag.h"
#include "../core/vos_azioni.h"
#include <string.h>
#include <stdlib.h>
#ifdef VOS_PIN_LED2
#include "vos_pins.h"
#include "vos_led_trigger.h"
#include "../drivers/vos_drv_pwm.h"
#include "../drivers/vos_drv_time.h"
#include "../core/vos_clamp.h"

#ifndef VOS_LED2_ACTIVE_LOW
#define VOS_LED2_ACTIVE_LOW 0
#endif
#define LED2_BLINK_MS 500        // comando manuale "blink" e lampeggio lento dello stato
#define LED2_FAST_MS 150         // lampeggio veloce (errore)

static volatile uint8_t g_manual = LED2_AUTO;
static volatile NetState g_net = NET_BOOT;
static volatile bool g_fault = false;
static volatile bool g_setup = false;            // prima configurazione non finita (solo sulle schede senza LED RGB: l'RGB ha la sua resa)
static bool g_ready = false;
static int g_pwm = -1;                           // canale PWM del LED2 (drivers/vos_drv_pwm)
static uint8_t g_level = 0;                      // luminosita scritta adesso sul pin (0 = spento)

static void level(uint8_t v) {
  g_level = v;
  drvPwmSet(g_pwm, v, 255);
}

// Lampeggio a meta periodo acceso, meta spento
static bool blinkPhase(uint32_t now, uint32_t halfMs) {
  return ((now / halfMs) & 1) == 0;
}

// Battito come nel LED1: la luminosita sale e scende, il periodo e piu corto se la CPU e piu carica.
// Decisione dell'utente del 16/11/2026: sul LED a un solo colore il battito scende fino allo SPENTO (0%), cosi si riconosce come battito
// (con il minimo dell'8% ereditato dal LED1 il LED sembrava sempre acceso). Il LED a colori (vos_led.cpp) e il respiro restano come prima.
static uint8_t heartbeatLevel(uint32_t now, uint8_t br) {
  float v = ledTrigBeat(now, 1600 - 11 * vosClamp(sysCpuPercent(), 0, 100));
  return (uint8_t)(br * v);
}

// Resa del modo "stato" con un solo colore. Rete collegata: battito (come il LED a colori, che batte nel colore dello stato).
// Gli altri stati si distinguono dal lampeggio, perche senza colore non c'e altro modo.
static uint8_t stateLevel(uint32_t now, uint8_t br) {
  switch (g_net) {
    case NET_CLIENT_OK: return heartbeatLevel(now, br);
    case NET_AIR:       return (now % 2000) < 100 ? br : 0;                  // lampo breve ogni 2 s
    default:            return blinkPhase(now, LED2_BLINK_MS) ? br : 0;      // avvio, connessione, punto di accesso
  }
}

// Respiro lento (periodo 3 s); con la finestra del tasto BOOT aperta piu rapido (1,2 s). Sale e scende come il battito, senza scendere a buio.
static uint8_t breathLevel(uint32_t now, uint8_t br) {
  uint32_t per = apWinIsOpen() ? 1200 : 3000;                      // in attesa del tasto (prima accensione) lento, con l'hotspot aperto veloce
  uint32_t ph = now % per, half = per / 2;
  uint32_t lv = ph < half ? ph : per - ph;                        // 0..half
  return (uint8_t)(br * (0.08f + 0.92f * ((float)lv / (float)half)));
}

// Luminosita voluta adesso (0 = spento). Il comando manuale vale piu del modo salvato, l'errore grave piu di tutto il resto
static uint8_t wanted(uint32_t now) {
  uint8_t br = cfg.led2Brightness;
  bool dAcceso;     // codice della diagnostica (classe, pausa, dettaglio): vale piu di tutto; un guasto fatale o critico si vede anche con il LED spento
  if (diagLedComanda(now, cfg.led2Mode == LED_OFF, &dAcceso, nullptr, nullptr, nullptr)) return dAcceso ? (br < 80 ? 80 : br) : 0;
  if (g_manual == LED2_ON) return br;
  if (g_manual == LED2_OFF) return 0;
  if (g_manual == LED2_BLINK) return blinkPhase(now, LED2_BLINK_MS) ? br : 0;
  if (g_fault) return blinkPhase(now, LED2_FAST_MS) ? br : 0;
  if (!VOS_HAS_LED_RGB && cfg.led2Mode != LED_OFF) {              // scheda con il solo LED semplice (C3): dice da sola prima configurazione e salvataggio fallito
    if (!cfgLastSaveOk()) return blinkPhase(now, LED2_FAST_MS) ? br : 0;
    if (g_setup) return breathLevel(now, br);
  }
  switch (cfg.led2Mode) {
    case LED_OFF:       return 0;
    case LED_FIXED:     return br;
    case LED_HEARTBEAT: return heartbeatLevel(now, br);
    case LED_FLASH:
    case LED_WIFI:      return ledTrigPulse(1, cfg.led2Mode, now, nullptr) ? br : 0;
    case LED_CPU:       return ledTrigCpuOver(cfg.led2CpuThr) ? br : 0;
    default:            return stateLevel(now, br);
  }
}

bool led2Present() { return true; }

void led2Init() {
  if (!pinClaim(VOS_PIN_LED2, "LED2", "LED2 della scheda", true)) return;   // gia di altri: non si tocca
  g_pwm = drvPwmOpen(VOS_PIN_LED2, 4000, 8, VOS_LED2_ACTIVE_LOW != 0);    // 4 kHz, 8 bit
  if (g_pwm < 0) return;                                                  // PWM non disponibile: il LED resta com'e (spento)
  level(0);
  g_manual = LED2_AUTO;
  g_ready = true;
}

void led2Shutdown() {
  if (g_ready) level(0);
}

bool led2Set(uint8_t mode) {
  if (!g_ready || mode > LED2_AUTO) return false;
  g_manual = mode;
  azionePubblica(AZ_LED, "led2", mode);
  return true;
}

uint8_t led2Mode() { return g_manual; }
bool led2On() { return g_ready && g_level > 0; }
void led2SetNetState(NetState s) { g_net = s; }
void led2SetFault(bool on) { g_fault = on; }
void led2SetSetup(bool on) { g_setup = on; }

void led2Tick() {
  if (!g_ready) return;
  uint8_t v = wanted(drvMillis());
  if (v != g_level) level(v);                     // si scrive sul pin solo quando cambia
}
#else
bool led2Present() { return false; }
void led2Init() {}
void led2Shutdown() {}
bool led2Set(uint8_t) { return false; }
uint8_t led2Mode() { return LED2_AUTO; }
bool led2On() { return false; }
void led2SetNetState(NetState) {}
void led2SetFault(bool) {}
void led2SetSetup(bool) {}
void led2Tick() {}
#endif

// Luminosita salvata: vale in tutti i modi. Senza LED2 non c'e nulla da regolare.
bool led2SetBrightness(int v) {
  if (!led2Present() || v < 0 || v > 255) return false;
  cfg.led2Brightness = (uint8_t)v;
  cfgSave();
  azionePubblica(AZ_LED, "led2_lum", v);
  return true;
}

// Azione delle regole: "brightness <valore>" oppure uno dei nomi di led2SetName
bool led2Apply(const char* w, const char* arg) {
  if (w && !strcmp(w, "brightness")) return led2SetBrightness(arg ? atoi(arg) : -1);
  return led2SetName(w);
}

bool led2SetName(const char* w) {
  if (!w) return false;
  if (!strcmp(w, "on") || !strcmp(w, "1")) return led2Set(LED2_ON);
  if (!strcmp(w, "off") || !strcmp(w, "0")) return led2Set(LED2_OFF);
  if (!strcmp(w, "blink")) return led2Set(LED2_BLINK);
  if (!strcmp(w, "auto")) return led2Set(LED2_AUTO);
  return false;
}

const char* led2ModeName(uint8_t mode) {
  return mode == LED2_ON ? "on" : mode == LED2_BLINK ? "blink" : mode == LED2_AUTO ? "auto" : "off";
}
