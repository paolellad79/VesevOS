// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_serial.cpp
#include "vos_serial.h"
#include "vos_config.h"
#include "vos_log.h"
#include "vos_i18n.h"

#if ARDUINO_USB_CDC_ON_BOOT
static const bool kUsb = true;          // USB nativa: la velocita non conta
#else
static const bool kUsb = false;
#endif

// Uscita con a-capo scelto: "\r\n", "\n" e "\r" nel testo diventano tutti l'a-capo configurato
class SerOut : public Print {
  uint8_t last = 0;
public:
  size_t write(uint8_t c) override {
    if (c == '\n' && last == '\r') { last = c; return 1; }          // seconda meta di CR+LF: gia scritto
    last = c;
    if (c == '\r' || c == '\n') {
      switch (cfg.serEol) {
        case 1:  Serial.write((uint8_t)'\n'); break;
        case 2:  Serial.write((uint8_t)'\r'); break;
        default: Serial.write((uint8_t)'\r'); Serial.write((uint8_t)'\n'); break;
      }
      return 1;
    }
    return Serial.write(c);
  }
  size_t write(const uint8_t* b, size_t n) override { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
};
static SerOut g_out;
Print& serOut() { return g_out; }
bool serIsOut(Print& o) { return &o == &g_out || &o == &Serial; }

void serLogLine(const char* line) {
  if (!cfg.serLogOut) return;
  g_out.print(line); g_out.print("\n");
}

static uint32_t g_prevBaud = 0, g_trialUntil = 0;       // prova della velocita nuova

void serialApply() {
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(cfg.serTx);
#else
  static uint32_t cur = 115200;
  if (cur != cfg.serBaud) { Serial.updateBaudRate(cfg.serBaud); cur = cfg.serBaud; }
#endif
}

static bool baudOk(uint32_t b) {
  static const uint32_t L[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
  for (size_t i = 0; i < sizeof(L) / sizeof(L[0]); i++) if (L[i] == b) return true;
  return false;
}

bool serialSet(const String& key, const String& val, String& err) {
  bool on = (val == "on" || val == "1");
  if (key == "baud") {
    uint32_t b = (uint32_t)val.toInt();
    if (!baudOk(b)) { err = tr("Velocita non valida (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)"); return false; }
    if (kUsb) { cfg.serBaud = b; }
    else {
      if (b == cfg.serBaud) return true;
      g_prevBaud = cfg.serBaud; g_trialUntil = millis() + 20000UL; if (!g_trialUntil) g_trialUntil = 1;
      cfg.serBaud = b; serialApply();
      vlog("SERIALE: velocita %lu in prova per 20 secondi", (unsigned long)b);
      return true;                                     // salvato solo con "serial keep"
    }
  }
  else if (key == "eol") {
    if (val == "crlf") cfg.serEol = 0; else if (val == "lf") cfg.serEol = 1; else if (val == "cr") cfg.serEol = 2;
    else { err = tr("A capo non valido (crlf, lf, cr)"); return false; }
  }
  else if (key == "echo") cfg.serEcho = on;
  else if (key == "input") cfg.serIn = on;
  else if (key == "log") cfg.serLogOut = on;
  else if (key == "banner") cfg.serBanner = on;
  else if (key == "tx") {
    int t = val.toInt();
    if (t < 0 || t > 200 || (t == 0 && val != "0")) { err = tr("Attesa non valida (0-200 ms)"); return false; }
    cfg.serTx = (uint8_t)t;
  }
  else { err = tr("Impostazione sconosciuta (baud, eol, echo, input, log, banner, tx)"); return false; }
  serialApply();
  cfgSave();
  vlog("SERIALE: impostazione %s = %s", key.c_str(), val.c_str());
  return true;
}

bool serialKeep() {
  if (!g_trialUntil) return false;
  g_trialUntil = 0; cfgSave();
  vlog("SERIALE: velocita %lu confermata", (unsigned long)cfg.serBaud);
  return true;
}

void serialTick() {
  if (g_trialUntil && (int32_t)(millis() - g_trialUntil) >= 0) {
    g_trialUntil = 0; cfg.serBaud = g_prevBaud; serialApply();
    vlog("SERIALE: velocita non confermata, torna %lu", (unsigned long)cfg.serBaud);
  }
}

static const char* eolName() { return cfg.serEol == 1 ? "lf" : (cfg.serEol == 2 ? "cr" : "crlf"); }

String serialText() {
  String o;
  o += trf("Velocita: %lu%s", (unsigned long)cfg.serBaud, kUsb ? (String(" (") + tr("USB nativa: non conta") + ")").c_str() : "") + "\n";
  o += trf("A capo: %s", eolName()) + "\n";
  o += trf("Eco dei tasti: %s", cfg.serEcho ? "on" : "off") + "\n";
  o += trf("Accetta comandi: %s", cfg.serIn ? "on" : "off") + "\n";
  o += trf("Scrive il registro: %s", cfg.serLogOut ? "on" : "off") + "\n";
  o += trf("Benvenuto all'apertura: %s", cfg.serBanner ? "on" : "off") + "\n";
  o += trf("Attesa di scrittura: %d ms", (int)cfg.serTx) + "\n";
  if (g_trialUntil) o += String(tr("Velocita in prova: scrivi 'serial keep' per confermarla")) + "\n";
  return o;
}

String serialJson() {
  return String("{\"baud\":") + (unsigned long)cfg.serBaud + ",\"usb\":" + (kUsb ? "true" : "false") + ",\"eol\":\"" + eolName() +
         "\",\"echo\":" + (cfg.serEcho ? "true" : "false") + ",\"input\":" + (cfg.serIn ? "true" : "false") +
         ",\"log\":" + (cfg.serLogOut ? "true" : "false") + ",\"banner\":" + (cfg.serBanner ? "true" : "false") +
         ",\"tx\":" + (int)cfg.serTx + ",\"trial\":" + (g_trialUntil ? "true" : "false") + "}";
}
