// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region.cpp
#include "vos_region.h"
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_strbuf.h"
#include "vos_util.h"
#include "vos_common_data.h"
#include "../drivers/vos_drv_wifi.h"

#define CHIP_MAX_DBM 20

static const VosCountryRadio* findCc(const char* cc) {
  if (!cc || strlen(cc) != 2) return nullptr;
  for (int i = 0; i < VOS_COUNTRY_N; i++) if (strcmp(cc, VOS_COUNTRIES[i].cc) == 0) return &VOS_COUNTRIES[i];
  return nullptr;
}

bool regionValid(const char* cc) { return findCc(cc) != nullptr; }
int  regionChannels() { const VosCountryRadio* c = findCc(cfg.country.c_str()); return c ? c->ch : 11; }
int  regionLimitDbm() { const VosCountryRadio* c = findCc(cfg.country.c_str()); return c ? c->dbm : 20; }
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
  const char* cc = regionValid(cfg.country.c_str()) ? cfg.country.c_str() : "01";   // "01" = mondo (regole prudenti)
  int dbm = regionTxDbm();
  drvWifiApplyRegion(cc, regionChannels(), dbm);
  vlog("RADIO: paese %s, canali 1-%d, potenza %d dBm (antenna %s)", cfg.country.length() ? cfg.country.c_str() : "-",
       regionChannels(), dbm, cfg.antExt ? "esterna" : "interna");
}

bool regionJson(HeapBuf& j) {
  return j.add("{\"country\":\"") && j.addJson(cfg.country.c_str()) &&
         j.addf("\",\"ch\":%d,\"limit\":%d,\"max\":%d,\"tx\":%d,\"txSet\":%d,\"antExt\":%d,\"gain\":%d,\"weekStart\":%d,\"decSep\":%d,\"dateFmt\":%d,\"timeFmt\":%d,\"tempUnit\":%d,\"tz\":\"",
                regionChannels(), regionLimitDbm(), regionMaxDbm(), regionTxDbm(), (int)cfg.txDbm, (int)cfg.antExt, (int)cfg.antGain,
                (int)cfg.weekStart, (int)cfg.decSep, (int)cfg.dateFmt, (int)cfg.timeFmt, (int)cfg.tempUnit) &&
         j.addJson(cfg.tz.c_str()) && j.add("\",\"tzName\":\"") && j.addJson(cfg.tzName.c_str()) &&
         j.add("\",\"ntp\":\"") && j.addJson(cfg.ntpServer.c_str()) && j.add("\",\"lang\":\"") && j.addJson(cfg.lang.c_str()) && j.add("\"}");
}

std::string regionText() {
  StrText t(512);
  StrBuf& r = t.buf();
  r.addf(tr("Paese:     %s"), cfg.country.length() ? cfg.country.c_str() : tr("non scelto (regole prudenti)")).add('\n');
  r.addf(tr("Canali:    1-%d"), regionChannels()).add('\n');
  r.addf(tr("Potenza:   %d dBm (massimo %d, limite del paese %d dBm EIRP)"), regionTxDbm(), regionMaxDbm(), regionLimitDbm()).add('\n');
  if (cfg.antExt) r.addf(tr("Antenna:   %s"), trf("esterna, %d dBi", cfg.antGain).c_str());
  else r.addf(tr("Antenna:   %s"), tr("interna"));
  r.add('\n');
  r.addf(tr("Fuso:      %s"), cfg.tzName.c_str()).add('\n');
  r.add(tr("Non e consulenza legale: verifica le regole del tuo paese.")).add('\n');
  return t.take();
}

const uint8_t* regionCommonGz(size_t& len) { len = COMMON_GZ_LEN; return COMMON_GZ; }
