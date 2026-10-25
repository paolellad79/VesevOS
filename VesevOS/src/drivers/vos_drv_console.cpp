// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_console.cpp (strato 1: console sulla USB Serial/JTAG, vedi vos_drv_console.h)
//
// Perche non c'e piu la correzione B (attesa di 10 ms + memoria di 2 s sul "non connesso"): quella serviva perche
// HWCDC di Arduino, con la porta "non connessa", buttava via i byte vecchi per far posto ai nuovi. Il driver
// `usb_serial_jtag` di ESP-IDF non lo fa: scrive in un anello e, se e pieno, aspetta il tempo che gli diciamo noi
// e poi restituisce quanti byte sono entrati. DA PROVARE: `ram` x20 con 0 perdite; se tornano, rimettere l'attesa breve.
#include "vos_drv_console.h"
#include "vos_drv_time.h"
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>

#define CON_TX_BUF 512                  // anello di trasmissione (un pezzo della SerOut e di 96 byte)
#define CON_RX_BUF 256
#define CON_FLUSH_MS 50                 // attesa massima di drvConFlush
#define CON_FAIL_MS 2000                // dopo una scrittura corta non si aspetta piu per questo tempo

static bool g_up = false;               // driver installato
static uint32_t g_txMs = 10;            // attesa massima per scrittura (come cfg.serTx)
static uint32_t g_failUntil = 0;        // fino a quando il PC e considerato "non legge" (0 = nessun problema)
static int g_peek = -1;                 // byte letto in anticipo da drvConAvailable()

static bool failed() { return g_failUntil && (int32_t)(drvMillis() - g_failUntil) < 0; }
static void markFail() { g_failUntil = drvMillis() + CON_FAIL_MS; if (!g_failUntil) g_failUntil = 1; }

void drvConBegin(uint32_t) {
  if (g_up) return;
  if (usb_serial_jtag_is_driver_installed()) { g_up = true; return; }
  usb_serial_jtag_driver_config_t c = { CON_TX_BUF, CON_RX_BUF };
  g_up = (usb_serial_jtag_driver_install(&c) == ESP_OK);      // se fallisce la console resta muta, il resto parte lo stesso
}

void drvConSetTxTimeoutMs(uint32_t ms) { g_txMs = ms; }

size_t drvConWrite(const uint8_t* b, size_t n) {
  if (!g_up || !n) return n;
  if (!usb_serial_jtag_is_connected() || failed() || g_txMs == 0) {
    usb_serial_jtag_write_bytes(b, n, 0);                     // senza attesa: quello che non entra si butta
    return n;
  }
  size_t done = 0;
  uint32_t t0 = drvMillis();
  while (done < n) {
    int w = usb_serial_jtag_write_bytes(b + done, n - done, pdMS_TO_TICKS(g_txMs));
    if (w <= 0) break;
    done += (size_t)w;
    if ((uint32_t)(drvMillis() - t0) >= g_txMs) break;
  }
  if (done < n) markFail();                                   // il PC non legge: SerOut smette di scrivere per 2 s
  return done;
}

int drvConAvailable() {
  if (!g_up) return 0;
  if (g_peek < 0) {
    uint8_t c;
    if (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) g_peek = c;
  }
  return g_peek >= 0 ? 1 : 0;
}

int drvConRead() {
  if (!drvConAvailable()) return -1;
  int c = g_peek;
  g_peek = -1;
  return c;
}

void drvConFlush() {
  if (!g_up || !usb_serial_jtag_is_connected() || failed()) return;
  usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(CON_FLUSH_MS));
}

bool drvConIsOpen() { return g_up && usb_serial_jtag_is_connected() && !failed(); }
