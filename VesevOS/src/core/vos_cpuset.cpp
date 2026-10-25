// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpuset.cpp
// Scelta della velocita della CPU e parametri dei governor: controllo del testo, salvataggio in config, traccia nel registro.
// Chi chiama (shell, API) deve gia aver controllato che l'utente sia Admin. Il cambio vero lo fa il monitor (vos_cpupm.cpp).
#include <string.h>
#include "vos_cpupm.h"
#include "vos_cpugov.h"
#include "vos_config.h"
#include "vos_audit.h"
#include "vos_i18n.h"

// Scelta da testo in (velocita fissa, governor). mhz = 0 vuol dire "decide il governor".
static bool choiceParse(const char* text, int& mhz, CpuGov& gov) {
  if (!text) return false;
  if (!strcmp(text, "80"))  { mhz = 80;  return true; }
  if (!strcmp(text, "160")) { mhz = 160; return true; }
  if (!strcmp(text, "240")) { mhz = 240; return true; }
  if (!strcmp(text, "auto")) { mhz = 0; gov = GOV_ONDEMAND; return true; }      // nome dei vecchi client
  if (cpuGovParse(text, gov)) { mhz = 0; return true; }
  return false;
}

bool cpuChoiceValid(const char* text) {
  int m; CpuGov g = GOV_ONDEMAND;
  return choiceParse(text, m, g);
}

bool cpuChoiceSet(const char* text) {
  int mhz = 0; CpuGov gov = (CpuGov)cfg.cpuGov;
  if (!choiceParse(text, mhz, gov)) return false;
  cfg.cpuMhz = (uint16_t)mhz;
  cfg.cpuGov = (uint8_t)gov;
  cfgSave();
  cpuPmApplyMode();
  auditEvent(AUD_INFO, "", trf("Velocita CPU scelta: %s", cpuChoiceName()).c_str());
  return true;
}

const char* cpuChoiceName() {
  static char fixed[8];
  if (cfg.cpuMhz == 0) return cpuGovName((CpuGov)cfg.cpuGov);
  snprintf(fixed, sizeof fixed, "%u", (unsigned)cfg.cpuMhz);
  return fixed;
}

bool cpuModeValid(int mode) { return mode == 0 || mode == 80 || mode == 160 || mode == 240; }

bool cpuModeSet(int mode) {
  if (!cpuModeValid(mode)) return false;
  if (mode == 0) return cpuChoiceSet("ondemand");
  return cpuChoiceSet(mode == 80 ? "80" : mode == 160 ? "160" : "240");
}

bool cpuParamsSet(int up, int down, int hold, ErrBuf& err) {
  if (!cpuGovParamsValid(up, down, hold)) {
    err = tr("Valore fuori campo: salita 50-95, discesa 5-50 e almeno 10 sotto la salita, attesa 2-120 secondi");
    return false;
  }
  cfg.cpuUp = (uint8_t)up; cfg.cpuDown = (uint8_t)down; cfg.cpuHold = (uint16_t)hold;
  cfgSave();
  auditEvent(AUD_INFO, "", trf("Parametri governor CPU: salita %d%%, discesa %d%%, attesa %d s", up, down, hold).c_str());
  return true;
}

bool cpuParamSet(const char* name, long value, ErrBuf& err) {
  int up = cfg.cpuUp, down = cfg.cpuDown, hold = cfg.cpuHold;
  if (value < 0 || value > 1000) { err = tr("Valore fuori campo: salita 50-95, discesa 5-50 e almeno 10 sotto la salita, attesa 2-120 secondi"); return false; }
  if (name && !strcmp(name, "up")) up = (int)value;
  else if (name && !strcmp(name, "down")) down = (int)value;
  else if (name && !strcmp(name, "hold")) hold = (int)value;
  else { err = tr("Parametro sconosciuto"); return false; }
  return cpuParamsSet(up, down, hold, err);
}

// Motivo del tetto, in lingua.
const char* cpuPmWhyText(CpuWhy w) {
  switch (w) {
    case CW_START:  return tr("avvio: nessun cambio nei primi 15 secondi");
    case CW_USER:   return tr("velocita fissa scelta da te");
    case CW_LOAD:   return tr("lo decide il governor in base al carico");
    case CW_BOOST:  return tr("prenotazione di velocita");
    case CW_HOT:    return tr("abbassata per il caldo");
    case CW_HTTPS:  return tr("caldo, ma HTTPS acceso: il tetto resta a 160 MHz");
    case CW_SENSOR: return tr("sensore di temperatura in errore: tetto prudente");
  }
  return "";
}
