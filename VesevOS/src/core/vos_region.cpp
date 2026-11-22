// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region.cpp
// ATTENZIONE - NON MODIFICARE senza un controllo legale: i canali e la potenza dipendono dal paese e dal guadagno
// dell'antenna (limite EIRP di legge). Un valore sbagliato puo violare le norme radio. Non e consulenza legale.
#include "vos_region.h"
#include "vos_region_calc.h"
#include "vos_config.h"
#include "vos_audit.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_strbuf.h"
#include "vos_util.h"
#include "vos_region_tab_rule.h"
#include "vos_risorse_file.h"
#include <atomic>
#include "../boards/vos_board.h"
#include "../drivers/vos_drv_wifi.h"

#define CHIP_MAX_DBM 20

// Ripiego prudente quando la tabella dei paesi non c'e o non e valida (o il paese non e nella tabella): canali 1-11 e limite 20 dBm EIRP.
// Con l'antenna della scheda (vedi regionMaxDbm) fa 15 dBm al chip. Scelto dall'utente il 16/11/2026 (nessuna firma sul file radio).
#define RIPIEGO_CANALI 11
#define RIPIEGO_DBM 20
#define SOGLIA_AVVISO_DBM 20       // oltre questi valori, nella riga del paese in uso in radio.txt, si avvisa in audit (nessun blocco)
#define SOGLIA_AVVISO_CANALI 13

namespace {

struct Voce {
  bool    noto;
  uint8_t ch;
  uint8_t dbm;
};

// L'ultimo paese cercato: cc nei due byte alti, canali e dBm nei due bassi; 0 = vuoto (nessun paese noto ha 0 canali). Senza blocchi.
std::atomic<uint32_t> g_ultimo{0};
std::atomic<int>      g_esito{-1};          // ultimo esito del file radio (RisEsito), -1 = mai letto

// Cerca il paese nel file radio.txt (SHA256SUMS lo controlla la prima volta). Ripiego se manca, non e valido o il paese non c'e.
Voce cercaNelFile(const char* cc) {
  Voce v = {false, RIPIEGO_CANALI, RIPIEGO_DBM};
  if (!cc || strlen(cc) != 2) return v;
  uint32_t k = ((uint32_t)(uint8_t)cc[0] << 24) | ((uint32_t)(uint8_t)cc[1] << 16);
  uint32_t u = g_ultimo.load();
  if ((u & 0xFFFF0000u) == k && (u & 0xFFFFu) != 0) return Voce{true, (uint8_t)((u >> 8) & 0xFF), (uint8_t)(u & 0xFF)};
  VosFile f;
  size_t len = 0;
  RisEsito e = risFileApri(RF_RADIO, f, len);
  g_esito.store((int)e);
  if (e != RIS_OK) return v;
  RegionTabCerca c(cc);
  uint8_t buf[128];
  for (size_t n; (n = f.read(buf, sizeof buf)) > 0;) c.dai((const char*)buf, n);
  c.fine();
  if (!c.valida()) g_esito.store((int)RIS_FORMATO);
  if (!c.trovato()) return v;
  g_ultimo.store(k | ((uint32_t)c.canali() << 8) | (uint32_t)c.dbm());
  return Voce{true, (uint8_t)c.canali(), (uint8_t)c.dbm()};
}

}  // namespace

void regionTabellaAvviso() {
  VosFile f;
  size_t len = 0;
  if (risFileApri(RF_RADIO, f, len) != RIS_OK) return;          // mancante o diverso dall'elenco: lo segnala gia vos_risorse_avviso
  RegionTabCerca c(cfg.country.c_str());                         // solo il paese in uso: la tabella intera ha paesi a 30 dBm del tutto regolari
  uint8_t buf[128];
  for (size_t n; (n = f.read(buf, sizeof buf)) > 0;) c.dai((const char*)buf, n);
  c.fine();
  if (!c.valida() || !c.trovato()) return;
  if (c.dbm() > SOGLIA_AVVISO_DBM || c.canali() > SOGLIA_AVVISO_CANALI) {
    auditEvent(AUD_YELLOW, "radiotab", trf("La tabella radio ha valori oltre i limiti usuali (%d dBm, %d canali al massimo; usuali: %d dBm, %d canali). I valori restano applicati: controlla le regole del tuo paese",
                                           c.dbm(), c.canali(), SOGLIA_AVVISO_DBM, SOGLIA_AVVISO_CANALI).c_str());
  }
}

bool regionValid(const char* cc) { return cercaNelFile(cc).noto; }
int  regionChannels() { return cercaNelFile(cfg.country.c_str()).ch; }
int  regionLimitDbm() { return cercaNelFile(cfg.country.c_str()).dbm; }
bool regionChannelOk(int ch) { return ch >= 1 && ch <= regionChannels(); }

// Guadagno dell'antenna in uso, in decimi di dBi: quella esterna scelta dall'utente oppure quella della scheda (non si sommano)
int regionGuadagnoDeciDbi() {
  if (cfg.antExt) return (int)cfg.antGain * 10;
  return VOS_ANTENNA_DECIMI_DBI;
}

int regionMaxDbm() {
  return regionCalcolaMaxDbm(regionLimitDbm(), regionGuadagnoDeciDbi(), CHIP_MAX_DBM);   // EIRP = potenza + guadagno dell'antenna
}

int regionTxDbm() {
  int m = regionMaxDbm();
  if (cfg.txDbm <= 0 || cfg.txDbm > m) return m;
  return cfg.txDbm;
}

void regionApplyRadio() {
  if (drvWifiMode() == DW_OFF) return;
  const char* cc = regionValid(cfg.country.c_str()) ? cfg.country.c_str() : "01";   // "01" = mondo (regole prudenti)
  int dbm = regionTxDbm();                            // limite del paese (e scelta dell'utente)
#ifdef VOS_AP_TXPOWER_DBM
  // Limite della scheda mentre l'hotspot e acceso (la potenza di IDF vale per tutta la radio): vale il MINIMO tra paese e scheda
  DrvWifiMode wm = drvWifiMode();
  dbm = regionCalcolaConHotspot(dbm, wm == DW_AP || wm == DW_AP_STA, VOS_AP_TXPOWER_DBM);
#endif
  drvWifiApplyRegion(cc, regionChannels(), dbm);
  int esito = g_esito.load();
  if (esito > 0) vlog("RADIO: tabella dei paesi non disponibile (%s): ripiego prudente, canali 1-%d, limite %d dBm EIRP", risCodice((RisEsito)esito), RIPIEGO_CANALI, RIPIEGO_DBM);
  vlog("RADIO: paese %s, canali 1-%d, potenza %d dBm (antenna %s)", cfg.country.length() ? cfg.country.c_str() : "-",
       regionChannels(), dbm, cfg.antExt ? "esterna" : "interna");
}

bool regionJson(HeapBuf& j) {
  return j.add("{\"country\":\"") && j.addJson(cfg.country.c_str()) &&
         j.addf("\",\"ch\":%d,\"limit\":%d,\"max\":%d,\"tx\":%d,\"txSet\":%d,\"antExt\":%d,\"gain\":%d,\"weekStart\":%d,\"decSep\":%d,\"dateFmt\":%d,\"timeFmt\":%d,\"tempUnit\":%d,\"tarSoc\":%d,\"tarExt\":%d,\"tz\":\"",
                regionChannels(), regionLimitDbm(), regionMaxDbm(), regionTxDbm(), (int)cfg.txDbm, (int)cfg.antExt, (int)cfg.antGain,
                (int)cfg.weekStart, (int)cfg.decSep, (int)cfg.dateFmt, (int)cfg.timeFmt, (int)cfg.tempUnit, (int)cfg.tempTarSoc, (int)cfg.tempTarExt) &&
         j.addJson(cfg.tz.c_str()) && j.add("\",\"tzName\":\"") && j.addJson(cfg.tzName.c_str()) &&
         j.addf("\",\"dataLungaFmt\":%d,\"oraLungaFmt\":%d,\"oraLungaSec\":%d,\"dataCortaTesto\":\"", (int)cfg.dataLungaFmt, (int)cfg.oraLungaFmt, cfg.oraLungaSec ? 1 : 0) &&
         j.addJson(cfg.dataCortaTesto.c_str()) && j.add("\",\"dataLungaTesto\":\"") && j.addJson(cfg.dataLungaTesto.c_str()) &&
         j.add("\",\"oraLungaTesto\":\"") && j.addJson(cfg.oraLungaTesto.c_str()) &&
         j.add("\",\"ntp\":\"") && j.addJson(cfg.ntpServer.c_str()) && j.add("\",\"lang\":\"") && j.addJson(cfg.lang.c_str()) && j.add("\"}");
}

std::string regionText() {
  StrText t(512);
  StrBuf& r = t.buf();
  r.addf(tr("Paese:     %s"), cfg.country.length() ? cfg.country.c_str() : tr("non scelto (regole prudenti)")).add('\n');
  r.addf(tr("Canali:    1-%d"), regionChannels()).add('\n');
  r.addf(tr("Potenza:   %d dBm (massimo %d, limite del paese %d dBm EIRP)"), regionTxDbm(), regionMaxDbm(), regionLimitDbm()).add('\n');
  if (cfg.antExt) r.addf(tr("Antenna:   %s"), trf("esterna, %d dBi", cfg.antGain).c_str());
  else r.addf(tr("Antenna:   %s"), trf("interna, %d,%d dBi", VOS_ANTENNA_DECIMI_DBI / 10, VOS_ANTENNA_DECIMI_DBI % 10).c_str());
  r.add('\n');
  r.addf(tr("Fuso:      %s"), cfg.tzName.c_str()).add('\n');
  r.add(tr("Non e consulenza legale: verifica le regole del tuo paese.")).add('\n');
  return t.take();
}
