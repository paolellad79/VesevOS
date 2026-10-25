// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_serial.cpp
#include "vos_serial.h"
#include "vos_config.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_strbuf.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_console.h"

static const bool kUsb = true;          // la console e la USB nativa: la velocita non conta

// Uscita con a-capo scelto: "\r\n", "\n" e "\r" nel testo diventano tutti l'a-capo configurato
// Se il PC non legge (monitor aperto ma fermo) una scrittura finisce in ritardo: da quel momento per 2 secondi
// l'uscita si butta via senza aspettare, cosi il ciclo principale non resta bloccato (e il watchdog non scatta).
static uint32_t g_stallUntil = 0, g_stalls = 0;
uint32_t serStallCount() { return g_stalls; }
static bool stalled() { return g_stallUntil && (int32_t)(drvMillis() - g_stallUntil) < 0; }
static void markStall() { g_stalls++; g_stallUntil = drvMillis() + 2000UL; if (!g_stallUntil) g_stallUntil = 1; }

class SerOut : public OutSink {
  uint8_t last = 0;
  // una scrittura sola per pezzo (non una per carattere): con il PC che non legge, ogni scrittura puo aspettare fino al tempo massimo
  size_t put(const uint8_t* b, size_t n) {
    if (stalled()) return n;                                            // il PC non sta leggendo: non si aspetta
    uint8_t buf[96]; size_t k = 0;
    for (size_t i = 0; i < n; i++) {
      uint8_t c = b[i];
      if (c == '\n' && last == '\r') { last = c; continue; }            // seconda meta di CR+LF: gia scritto
      last = c;
      if (k + 2 > sizeof(buf)) { if (drvConWrite(buf, k) < k) { markStall(); return n; } k = 0; }
      if (c == '\r' || c == '\n') {
        if (cfg.serEol == 0 || cfg.serEol == 2) buf[k++] = '\r';        // CR+LF o solo CR
        if (cfg.serEol == 0 || cfg.serEol == 1) buf[k++] = '\n';        // CR+LF o solo LF
      } else buf[k++] = c;
    }
    if (k && drvConWrite(buf, k) < k) { markStall(); return n; }
    return n;
  }
public:
  size_t write(uint8_t c) override { return put(&c, 1); }
  size_t write(const uint8_t* b, size_t n) override { return put(b, n); }
};
static SerOut g_out;
OutSink& serOut() { return g_out; }
bool serIsOut(OutSink& o) { return &o == &g_out; }

void serLogLine(const char* line) {
  if (!cfg.serLogOut) return;
  g_out.print(line); g_out.print("\n");
}

static uint32_t g_trialBaud = 0, g_trialUntil = 0;      // velocita nuova in prova (non finisce nella configurazione finche non si conferma)

void serialApply() {
  drvConSetTxTimeoutMs(cfg.serTx);
}

static bool baudOk(uint32_t b) {
  static const uint32_t L[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
  for (size_t i = 0; i < sizeof(L) / sizeof(L[0]); i++) if (L[i] == b) return true;
  return false;
}

static bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

bool serialSet(const char* key, const char* val, ErrBuf& err) {
  if (!key) key = "";
  if (!val) val = "";
  bool isBool = (same(key, "echo") || same(key, "input") || same(key, "log") || same(key, "banner"));
  bool on = (same(val, "on") || same(val, "1"));
  if (isBool && !on && !same(val, "off") && !same(val, "0")) { err = tr("Valore non valido (on oppure off)"); return false; }
  if (same(key, "baud")) {
    uint32_t b = (uint32_t)atol(val);
    if (!baudOk(b)) { err = tr("Velocita non valida (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)"); return false; }
    if (kUsb) { cfg.serBaud = b; }
    else {
      if (b == cfg.serBaud && !g_trialUntil) return true;
      g_trialBaud = b; g_trialUntil = drvMillis() + 20000UL; if (!g_trialUntil) g_trialUntil = 1;
      serialApply();
      vlog("SERIALE: velocita %lu in prova per 20 secondi", (unsigned long)b);
      return true;                                     // salvata solo con "serial keep"
    }
  }
  else if (same(key, "eol")) {
    if (same(val, "crlf")) cfg.serEol = 0; else if (same(val, "lf")) cfg.serEol = 1; else if (same(val, "cr")) cfg.serEol = 2;
    else { err = tr("A capo non valido (crlf, lf, cr)"); return false; }
  }
  else if (same(key, "echo")) cfg.serEcho = on;
  else if (same(key, "input")) cfg.serIn = on;
  else if (same(key, "log")) cfg.serLogOut = on;
  else if (same(key, "banner")) cfg.serBanner = on;
  else if (same(key, "tx")) {
    int t = (int)atol(val);
    if (t < 0 || t > 200 || (t == 0 && !same(val, "0"))) { err = tr("Attesa non valida (0-200 ms)"); return false; }
    cfg.serTx = (uint8_t)t;
  }
  else { err = tr("Impostazione sconosciuta (baud, eol, echo, input, log, banner, tx)"); return false; }
  serialApply();
  cfgSave();
  vlog("SERIALE: impostazione %s = %s", key, val);
  return true;
}

bool serialKeep() {
  if (!g_trialUntil) return false;
  g_trialUntil = 0; cfg.serBaud = g_trialBaud; cfgSave();
  vlog("SERIALE: velocita %lu confermata", (unsigned long)cfg.serBaud);
  return true;
}

void serialTick() {
  if (g_trialUntil && (int32_t)(drvMillis() - g_trialUntil) >= 0) {
    g_trialUntil = 0; serialApply();
    vlog("SERIALE: velocita non confermata, torna %lu", (unsigned long)cfg.serBaud);
  }
}

static const char* eolName() { return cfg.serEol == 1 ? "lf" : (cfg.serEol == 2 ? "cr" : "crlf"); }

// 8 righe tradotte (al massimo ~700 byte): 1024 bastano
std::string serialText() {
  StrText t(1024);
  StrBuf& o = t.buf();
  char usb[64] = "";
  if (kUsb) StrBuf(usb, sizeof(usb)).add(" (").add(tr("USB nativa: non conta")).add(")");
  o.addf(tr("Velocita: %lu%s"), (unsigned long)cfg.serBaud, usb).add('\n');
  o.addf(tr("A capo: %s"), eolName()).add('\n');
  o.addf(tr("Eco dei tasti: %s"), cfg.serEcho ? "on" : "off").add('\n');
  o.addf(tr("Accetta comandi: %s"), cfg.serIn ? "on" : "off").add('\n');
  o.addf(tr("Scrive il registro: %s"), cfg.serLogOut ? "on" : "off").add('\n');
  o.addf(tr("Benvenuto all'apertura: %s"), cfg.serBanner ? "on" : "off").add('\n');
  o.addf(tr("Attesa di scrittura: %d ms"), (int)cfg.serTx).add('\n');
  if (g_trialUntil) o.add(tr("Velocita in prova: scrivi 'serial keep' per confermarla")).add('\n');
  return t.take();
}

bool serialJson(HeapBuf& j) {
  return j.addf("{\"baud\":%lu,\"usb\":%s,\"eol\":\"%s\",\"echo\":%s,\"input\":%s,\"log\":%s,\"banner\":%s,\"tx\":%d,\"trial\":%s}",
                (unsigned long)cfg.serBaud, kUsb ? "true" : "false", eolName(), cfg.serEcho ? "true" : "false", cfg.serIn ? "true" : "false",
                cfg.serLogOut ? "true" : "false", cfg.serBanner ? "true" : "false", (int)cfg.serTx, g_trialUntil ? "true" : "false");
}
