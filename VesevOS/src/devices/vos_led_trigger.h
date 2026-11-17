// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led_trigger.h
// I "trigger" del LED: che cosa fa accendere il LED RGB oltre al normale stato di rete.
//   state, heartbeat, default-on, none : i modi di sempre (LED_STATE, LED_HEARTBEAT, LED_FIXED, LED_OFF)
//   flash : un lampo a ogni accesso alla flash o alla microSD (azzurro lettura, arancio scrittura)
//   wifi  : un lampo a ogni pacchetto Wi-Fi (bianco ricevuto, magenta inviato)
//   cpu   : il colore di fondo vira verso il colore CPU quando il carico supera la soglia (nessun lampo)
// Sul LED condiviso vince, dall'alto: tasto BOOT, "trova questa scheda", errore, WPS, script, prima configurazione, allarme,
// poi i lampi di attivita, poi il colore di fondo (stato di rete). Qui c'e solo la parte "lampi" e "carico".
// Limite di sicurezza per chi e sensibile alla luce: al massimo 3 lampi al secondo (il lampo dura 70 ms, il ciclo non scende sotto 340 ms).
#pragma once
#include <stdint.h>

// Chiamata dal task del LED ogni ciclo (circa 20 ms). true = adesso c'e un lampo da mostrare con il colore r,g,b.
bool ledTrigOverlay(uint32_t nowMs, uint8_t& r, uint8_t& g, uint8_t& b);

// Lampi per un LED qualsiasi: slot 0 = LED1, slot 1 = LED2 (ognuno ricorda i contatori che ha gia visto). mode = LED_FLASH o LED_WIFI.
// true = adesso c'e un lampo; color (se non nullo) riceve il colore 0xRRGGBB della sorgente (il LED2 non lo usa).
#define LEDTRIG_SLOTS 2
bool ledTrigPulse(int slot, uint8_t mode, uint32_t nowMs, uint32_t* color);

// true se il carico della CPU supera la soglia (limitata a 10-95). Per i LED a un solo colore.
bool ledTrigCpuOver(uint8_t thr);

// Forma del battito (due impulsi ravvicinati), valore 0..1 nel periodo dato. La usano il LED1 e il LED2.
float ledTrigBeat(uint32_t t, uint32_t period);

// Modo "cpu": porta r,g,b (colore di fondo) verso il colore CPU, piu forte quanto piu il carico supera la soglia. false = sotto la soglia.
bool ledTrigCpuMix(uint8_t& r, uint8_t& g, uint8_t& b);

const char* ledTrigName(uint8_t mode);                    // nome ufficiale di un modo ("state", "flash", ...)
bool ledTrigFromName(const char* name, uint8_t& mode);    // nome ufficiale o vecchio (fixed, off) -> modo; false se non esiste
