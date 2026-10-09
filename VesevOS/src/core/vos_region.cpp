// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region.cpp
#include "vos_region.h"
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_util.h"
#include "vos_common_data.h"
#include "../drivers/vos_drv_wifi.h"

#define CHIP_MAX_DBM 20

static const VosCountryRadio* findCc(const String& cc) {
  if (cc.length() != 2) return nullptr;
  for (int i = 0; i < VOS_COUNTRY_N; i++) if (cc == VOS_COUNTRIES[i].cc) return &VOS_COUNTRIES[i];
  return nullptr;
}

bool regionValid(const String& cc) { return findCc(cc) != nullptr; }
int  regionChannels() { const VosCountryRadio* c = findCc(cfg.country); return c ? c->ch : 11; }
int  regionLimitDbm() { const VosCountryRadio* c = findCc(cfg.country); return c ? c->dbm : 20; }
bool regionChannelOk(int ch) { return ch >= 1 && ch <= regionChannels(); }

int regionMaxDbm() {
  int m = regionLimitDbm() - (cfg.antExt ? cfg.antGain : 0);   // EIRP = potenza + guadagno dell'antenna
  if (m > CHIP_MAX_DBM) m = CHIP_MAX_DBM;
  if (m < 2) m = 2;
  return m;
}

int regionTxDbm() {
  int m = regionMaxDbm();
  if (cfg.txDbm <= 0 || cfg.txDbm > m) return m;
  return cfg.txDbm;
}

void regionApplyRadio() {
  if (drvWifiMode() == DW_OFF) return;
  String cc = cfg.country.length() == 2 && regionValid(cfg.country) ? cfg.country : String("01");
  int dbm = regionTxDbm();
  cc = drvWifiApplyRegion(cc.c_str(), regionChannels(), dbm);
  vlog("RADIO: paese %s, canali 1-%d, potenza %d dBm (antenna %s)", cfg.country.length() ? cfg.country.c_str() : "-",
       regionChannels(), dbm, cfg.antExt ? "esterna" : "interna");
}

String regionJson() {
  String j = "{\"country\":\"" + jsonEscape(cfg.country) + "\",\"ch\":" + String(regionChannels()) +
             ",\"limit\":" + String(regionLimitDbm()) + ",\"max\":" + String(regionMaxDbm()) + ",\"tx\":" + String(regionTxDbm()) +
             ",\"txSet\":" + String(cfg.txDbm) + ",\"antExt\":" + String(cfg.antExt) + ",\"gain\":" + String(cfg.antGain) +
             ",\"weekStart\":" + String(cfg.weekStart) + ",\"decSep\":" + String(cfg.decSep) +
             ",\"dateFmt\":" + String(cfg.dateFmt) + ",\"timeFmt\":" + String(cfg.timeFmt) + ",\"tempUnit\":" + String(cfg.tempUnit) +
             ",\"tz\":\"" + jsonEscape(cfg.tz) + "\",\"tzName\":\"" + jsonEscape(cfg.tzName) + "\",\"ntp\":\"" + jsonEscape(cfg.ntpServer) +
             "\",\"lang\":\"" + jsonEscape(cfg.lang) + "\"}";
  return j;
}

String regionText() {
  String t;
  t += trf("Paese:     %s", cfg.country.length() ? cfg.country.c_str() : tr("non scelto (regole prudenti)")) + "\n";
  t += trf("Canali:    1-%d", regionChannels()) + "\n";
  t += trf("Potenza:   %d dBm (massimo %d, limite del paese %d dBm EIRP)", regionTxDbm(), regionMaxDbm(), regionLimitDbm()) + "\n";
  t += trf("Antenna:   %s", cfg.antExt ? trf("esterna, %d dBi", cfg.antGain).c_str() : tr("interna")) + "\n";
  t += trf("Fuso:      %s", cfg.tzName.c_str()) + "\n";
  t += String(tr("Non e consulenza legale: verifica le regole del tuo paese.")) + "\n";
  return t;
}

const uint8_t* regionCommonGz(size_t& len) { len = COMMON_GZ_LEN; return COMMON_GZ; }
