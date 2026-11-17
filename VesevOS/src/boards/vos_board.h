// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board.h
// Sceglie il file di scheda, in UN solo punto. Di fabbrica: in base al chip della compilazione (CONFIG_IDF_TARGET_*),
// ESP32-S3 SuperMini se non si sa. Altra scheda: -DVOS_BOARD_FILE="vos_board_altra.h"  (il file sta in questa cartella).
// Solo caratteri ASCII.
#pragma once
// Ordine dei byte del LED RGB (VOS_LED_ORDER nel file di scheda). Se la scheda non lo dice: GRB, come il WS2812 standard.
#define VOS_LED_ORD_GRB 0
#define VOS_LED_ORD_RGB 1
#define VOS_LED_ORD_BRG 2
#define VOS_LED_ORD_GBR 3
#define VOS_LED_ORD_RBG 4
#define VOS_LED_ORD_BGR 5
// ---- Tabella dei piedini (VOS_BOARD_PINS nel file di scheda): una riga per piedino o piazzola fisica ----
// X(lato, posizione, gpio, tipo, bit). Posizione = indice lungo il lato (0 = vicino alla USB). gpio = -1 se non e un GPIO.
// Lato visto con la USB in alto. Fronte: SIN/DES/BASSO. Retro (visto da sotto): RETRO_SIN (colonna esterna sinistra),
// RETRO_INT (colonna interna sinistra), RETRO_DES, RETRO_ALTO (fila in alto, da sinistra a destra).
#define VOS_PL_SIN        0
#define VOS_PL_DES        1
#define VOS_PL_BASSO      2   // bordo corto in basso, da sinistra a destra
#define VOS_PL_RETRO_SIN  3
#define VOS_PL_RETRO_DES  4
#define VOS_PL_RETRO_ALTO 5
#define VOS_PL_RETRO_INT  6
// tipo: GPIO oppure quale alimentazione/piazzola (il nome mostrato viene da qui)
#define VOS_PT_GPIO       0
#define VOS_PT_5V         1
#define VOS_PT_3V3        2
#define VOS_PT_GND        3
#define VOS_PT_BATT_PIU   4
#define VOS_PT_BATT_MENO  5
#define VOS_PT_BOOST      6   // ponticello della corrente di carica
// bit: funzioni fisse del piedino (dal datasheet) e ruoli
#define VOS_PF_ADC        1
#define VOS_PF_TOCCO      2
#define VOS_PF_UART       4   // UART0 (console)
#define VOS_PF_SPI        8   // SPI principale sul multiplatore IO
#define VOS_PF_JTAG       16
#define VOS_PF_USB        32
#define VOS_PF_RTC        64  // dominio RTC / LP: puo svegliare dal sonno
#define VOS_PF_AVVIO      128 // pin di avvio (strapping)
#define VOS_PF_LED        256
#define VOS_PF_TASTO      512
#define VOS_PF_RISERVATO  1024 // console, USB, flash: non toccare
#if defined(__has_include)
#if __has_include(<sdkconfig.h>)
#include <sdkconfig.h>
#endif
#endif
#ifdef VOS_BOARD_FILE
#include VOS_BOARD_FILE
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#include "vos_board_c6_supermini.h"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#include "vos_board_c3_supermini.h"
#else
#include "vos_board_s3_supermini.h"
#endif
#ifndef VOS_BOARD_PERIF
#error "il file di scheda deve definire VOS_BOARD_PERIF (periferiche di bordo)"
#endif
#ifndef VOS_BOARD_PINS
#error "il file di scheda deve definire VOS_BOARD_PINS (tabella dei piedini)"
#endif
#ifndef VOS_ANTENNA_DECIMI_DBI
#error "il file di scheda deve definire VOS_ANTENNA_DECIMI_DBI (guadagno dell'antenna in decimi di dBi, serve al calcolo dell'EIRP)"
#endif
#ifndef VOS_LED_ORDER
#define VOS_LED_ORDER VOS_LED_ORD_GRB
#endif
#ifndef VOS_HAS_LED_RGB
#define VOS_HAS_LED_RGB 1     // la scheda ha un LED RGB (WS2812) sul pin VOS_PIN_LED_RGB. A 0 (C3): niente WS2812 ne prenotazione di quel pin; il LED semplice e il LED2
#endif
// Diagnostica con i lampi (core/vos_diag.cpp). Il LED dei codici e il LED semplice (VOS_PIN_LED2) se la scheda lo ha (C3, C6, poi H2: basta
// definire VOS_PIN_LED2 nel file di scheda); altrimenti il LED a colori (S3). VOS_LED_DIAG_PANICO = 1 (solo nel file di scheda, solo se il LED semplice e
// un on/off su GPIO: C3 e C6): il codice lampeggia anche DENTRO il panico, a mano sul GPIO (drivers/vos_drv_panic.cpp). Sull'S3 resta 0 (WS2812 non provato).
#ifndef VOS_LED_DIAG_PANICO
#define VOS_LED_DIAG_PANICO 0
#endif
#if VOS_LED_DIAG_PANICO && !defined(VOS_PIN_LED2)
#error "VOS_LED_DIAG_PANICO richiede VOS_PIN_LED2 (un LED semplice acceso/spento su GPIO)"
#endif
#ifndef VOS_LED2_ACTIVE_LOW
#define VOS_LED2_ACTIVE_LOW 0
#endif
#ifndef VOS_BOARD_PWM
#define VOS_BOARD_PWM 0       // la scheda non dice nulla: PWM dell'utente spento
#endif
#ifndef VOS_BOARD_CONTATORE
#define VOS_BOARD_CONTATORE 0 // la scheda non dice nulla: contatore di impulsi dell'utente spento
#endif
#ifndef VOS_BOARD_LED_RGB_MANO
#define VOS_BOARD_LED_RGB_MANO 0   // la scheda non dice nulla: LED a colori WS2812 a mano dell'utente spento
#endif
#ifndef VOS_RAM_MINIMA
#define VOS_RAM_MINIMA 0      // 1 = scheda con poca RAM interna e senza PSRAM (C3): HTTPS a 1 connessione, niente riavvio per RAM, registro delle modifiche senza copia in RAM, avviso in Funzionalita
#endif
#ifndef VOS_LOG_RIGHE
#define VOS_LOG_RIGHE 150     // righe del registro di sistema in RAM (120 byte l'una). Poca RAM (C3): 40
#endif
// Stack dei task di VesevOS (byte). Default = valori storici. Schede con poca RAM: ridotti nel file di scheda (misure di `ram audit`, prova 1.8.23f).
#ifndef VOS_PILA_PRINCIPALE
#define VOS_PILA_PRINCIPALE 8192   // loopTask (shell, tasto BOOT)
#endif
#ifndef VOS_PILA_ORA
#define VOS_PILA_ORA 4096          // task "time"
#endif
#ifndef VOS_PILA_MONITOR
#define VOS_PILA_MONITOR 4608      // task "monitor"
#endif
#ifndef VOS_PILA_WD
#define VOS_PILA_WD 6144           // task "wd"
#endif
#ifndef VOS_PILA_LED
#define VOS_PILA_LED 3072          // task "led"
#endif
// VOS_AP_TXPOWER_DBM (facoltativa, nel file di scheda): potenza massima in dBm mentre l'hotspot e acceso. Senza la macro nessun limite di scheda.
