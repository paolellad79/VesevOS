// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_power.cpp
#include "vos_power.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../net/vos_net.h"
#include "../devices/vos_led.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_strbuf.h"
#include "../core/vos_hooks.h"
#include "../core/vos_azioni.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_console.h"
#include "../drivers/vos_drv_sonno.h"
#include "../core/vos_time.h"
#include <time.h>
#include "../net/vos_traffic.h"
#include "vos_ble.h"

#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#define PW_MIN_AWAKE 10                              // mai dormire nei primi 10 minuti dall'accensione
RTC_DATA_ATTR static uint32_t r_magic = 0, r_wakes = 0, r_sleptSec = 0, r_plan = 0;
static int g_psApplied = -1;
static uint32_t g_lastCheck = 0;

void powerBegin() {
  if (r_magic == 0xA55A1234 && drvSonnoRisvegliatoDaTimer()) {
    r_wakes++; r_sleptSec += r_plan;
    vlog("ENERGIA: risveglio dal sonno (timer), dormito circa %lu s, risvegli %lu", (unsigned long)r_plan, (unsigned long)r_wakes);
  }
  r_magic = 0;
}

void powerSet(int mode, uint32_t awakeMin, uint32_t sleepMin, ErrBuf& err) {
  if (mode < 0 || mode > 2) { err = tr("Modo non valido"); return; }
  if (mode == 2) {
    if (awakeMin < PW_MIN_AWAKE || awakeMin > 1440) { err = tr("Tempo da sveglia: da 10 minuti a 24 ore"); return; }
    if (sleepMin < 1 || sleepMin > 10080) { err = tr("Sonno: da 1 minuto a 7 giorni"); return; }
    cfg.pwAwake = awakeMin; cfg.pwSleep = sleepMin;
  }
  cfg.pwMode = mode;
}

void powerSleepNow(uint32_t seconds) {
  vlog(seconds ? "ENERGIA: sonno profondo per %lu s" : "ENERGIA: sonno profondo fino a RESET (%lu)", (unsigned long)seconds);
  azionePubblica(AZ_SONNO, "", (int32_t)seconds);
  hookRecFlush(1500);                                        // dati del registratore in attesa: scritti prima di dormire
  drvConFlush();
  ledShutdown();
  netSospendi();                                             // come nel sonno leggero: prima si ferma tutto cio che usa la radio
  drvDelayMs(100);
  if (seconds) { r_magic = 0xA55A1234; r_plan = seconds; }       // il tasto fisico NON risveglia: tenuto premuto porterebbe in modo programmazione
  drvSonnoProfondo(seconds, false);
}

// Secondi da adesso alla prossima volta che l'orologio locale segna l'orario dato (minuti dalla mezzanotte). 0 = ora non valida.
// Il sonno poi si misura con il timer della scheda: su molte ore puo scostarsi di qualche minuto dall'orologio.
uint32_t powerSecondiAllOra(unsigned minuti) {
  if (!timeValid() || minuti > 1439) return 0;
  time_t now = time(NULL);
  struct tm t;
  localtime_r(&now, &t);
  long da = (long)t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  long a = (long)minuti * 60;
  long d = a - da;
  if (d <= 0) d += 86400;                                    // l'orario di oggi e gia passato: domani
  return (uint32_t)d;
}

bool powerRipresaPresente() { return drvSonnoRipresaPresente(); }

// Controlli del standby senza dormire: tempo massimo e almeno un risveglio (tempo, orario o tasto di ripresa installato) e Bluetooth spento.
bool powerStandbyControlla(bool profondo, uint32_t sec, bool conRipresa, ErrBuf& err) {
  if (sec > 7UL * 86400UL) { err = tr("Tempo: fino a 7 giorni"); return false; }
  if (!sec && !conRipresa) { err = tr("Serve almeno un risveglio: un tempo, un orario o una ripresa installata"); return false; }
  if (!profondo && bleRunning()) { err = tr("Spegni prima il Bluetooth: nel sonno leggero non resta acceso"); return false; }
  if (conRipresa && drvSonnoRipresaPremuta()) { err = tr("Il tasto di ripresa e premuto (o il suo piedino e al livello di risveglio): lo standby si sveglierebbe subito"); return false; }
  return true;
}

// Standby: sonno leggero o profondo, con risveglio a tempo (sec = 0 senza tempo) e, se e installato, dal tasto di ripresa. Lo standby non vede
// ne cerca il piedino: sa solo se la ripresa c'e (drvSonnoRipresaPresente). Qui si fanno solo i controlli e si PRENOTA lo standby: parte un secondo
// dopo, dal ciclo principale (powerTick), cosi la risposta alla pagina o alla shell esce PRIMA che il Wi-Fi si spenga e gli errori si vedono.
static struct { bool attivo; bool profondo; uint32_t sec; uint32_t quando; } g_standby = {false, false, 0, 0};

bool powerStandby(bool profondo, uint32_t sec, ErrBuf& err) {
  if (!profondo && (sec == 0 || sec > POWER_LEGGERO_MAX_S)) {      // sonno leggero sospeso (prove del 22/11/2026): lo standby passa al profondo
    profondo = true;
    vlog("ENERGIA: il sonno leggero e sospeso: lo standby usa il sonno profondo");
  }
  bool conRipresa = drvSonnoRipresaPresente();
  if (!powerStandbyControlla(profondo, sec, conRipresa, err)) return false;
  g_standby.profondo = profondo;
  g_standby.sec = sec;
  g_standby.quando = drvMillis() + 1000;
  g_standby.attivo = true;
  vlog("ENERGIA: standby %s prenotato (tempo %lu s, ripresa %s)", profondo ? "profondo" : "leggero", (unsigned long)sec, conRipresa ? "si" : "no");
  return true;
}

// ---- Dopo il sonno leggero: checklist di riaccensione ----
// Un passo ogni 3 s almeno e ognuno preceduto dal controllo della memoria: 1 rete, 2 server web (a rete pronta), poi un task per volta.
static const uint32_t RIPRESA_PASSO_MS = 3000;
static const uint32_t RIPRESA_ATTESA_RETE_MS = 40000;
static struct { bool attiva; int passo; uint32_t quando; uint32_t dal; } g_ripresa = {false, 0, 0, 0};

static void ripresaAvvia() {
  g_ripresa.attiva = true;
  g_ripresa.passo = 0;
  g_ripresa.dal = drvMillis();
  g_ripresa.quando = g_ripresa.dal + RIPRESA_PASSO_MS;
  vlog("SONNO: checklist di riaccensione, un passo ogni 3 s");
}

static void ripresaPasso() {
  uint32_t now = drvMillis();
  g_ripresa.quando = now + RIPRESA_PASSO_MS;
  if (g_hooks.heapIntegra && !g_hooks.heapIntegra()) {
    vlog("SONNO: memoria rovinata al passo %d: riavvio pulito", g_ripresa.passo + 1);
    g_ripresa.attiva = false;
    if (g_hooks.riavviaPulito) g_hooks.riavviaPulito("memoria rovinata dopo il sonno");
    return;
  }
  int p = g_ripresa.passo;
  if (p == 0) {
    netRiprendi();
    vlog("SONNO: passo 1: rete riavviata (memoria controllata)");
  } else if (p == 1) {
    NetState ns = netState();
    bool pronta = (ns == NET_CLIENT_OK || ns == NET_AP);
    if (!pronta && (uint32_t)(now - g_ripresa.dal) < RIPRESA_ATTESA_RETE_MS) return;   // aspetta la rete: stesso passo al prossimo giro
    hookCall(g_hooks.webRiprendi);
    vlog("SONNO: passo 2: server web riacceso%s", pronta ? "" : " (rete non pronta, tempo scaduto)");
  } else if (!(g_hooks.taskRiprendiUno && g_hooks.taskRiprendiUno())) {
    g_ripresa.attiva = false;
    vlog("SONNO: checklist finita");
    return;
  }
  g_ripresa.passo = p + 1;
}

// Dal ciclo principale: fa partire lo standby prenotato. Il leggero ritorna e scrive nel registro com'e andato (codice, causa, durata); il profondo non ritorna.
static void eseguiStandby() {
  bool profondo = g_standby.profondo;
  uint32_t sec = g_standby.sec;
  g_standby.attivo = false;
  azionePubblica(AZ_SONNO, "", (int32_t)sec);
  hookRecFlush(1500);                                        // dati del registratore in attesa: scritti prima di dormire
  drvConFlush();
  if (!profondo) hookCall(g_hooks.taskSospendiTutti);          // sonno leggero: prima si fermano tutti i task, poi la rete; al risveglio tornano uno per volta
  trafficPausa(true);                                        // il contatore del traffico non deve toccare le interfacce mentre si distruggono
  for (int i = 0; i < 30 && trafficOccupato(); i++) drvDelayMs(10);
  netSospendi();                                             // il livello net ferma WPS, mesh, Bluetooth, portale, mDNS e spegne la radio (guida ESP-IDF, sleep_modes)
  drvDelayMs(100);
  if (profondo) {
    ledShutdown();
    if (sec) { r_magic = 0xA55A1234; r_plan = sec; }
    drvSonnoProfondo(sec);                                   // non ritorna
    return;
  }
  DrvSonnoEsito es;
  drvSonnoLeggero(sec, es);
  trafficPausa(false);                                       // il contatore riprende dopo qualche secondo, a interfacce ricreate
  static const char* const CAUSE[] = {"nessuna", "timer", "piedino", "altra"};
  static const char* const CODICI[] = {"ok", "rifiutato: un risveglio era gia attivo", "troppo breve", "errore"};
  int c = (es.causa >= 0 && es.causa <= 3) ? es.causa : 3;
  int k = (es.codice >= 0 && es.codice <= 3) ? es.codice : 3;
  vlog("ENERGIA: standby leggero finito: %s, causa %s, dopo %lu ms, tasto prima %d", CODICI[k], CAUSE[c], (unsigned long)es.durataMs, es.livelloPrima);
  ripresaAvvia();                                            // rete, web e task tornano uno per volta, non tutti insieme
}

static void applyPs() {
  int want = cfg.pwMode == 1 ? 2 : 1;                 // 2 = massimo risparmio, 1 = normale
  int st = (int)netState() * 10 + want;
  if (st == g_psApplied) return;
  g_psApplied = st;
  drvWifiPowerSave(want == 2);
}

static uint32_t sleepInSec() {
  if (cfg.pwMode != 2 || !cfg.setupDone) return 0;
  uint32_t up = drvMillis() / 1000, need = cfg.pwAwake * 60UL;
  return up >= need ? 0 : need - up;
}

void powerTick() {
  uint32_t now = drvMillis();
  if (g_ripresa.attiva && (int32_t)(now - g_ripresa.quando) >= 0) ripresaPasso();
  if (g_standby.attivo && (int32_t)(now - g_standby.quando) >= 0) { eseguiStandby(); return; }
  if (now - g_lastCheck < 2000) return;
  g_lastCheck = now;
  applyPs();
  if (cfg.pwMode != 2 || !cfg.setupDone) return;
  if (sleepInSec() > 0) return;
  if (hookWebIdleSec() < 60) return;                       // qualcuno sta usando la pagina: si aspetta
  powerSleepNow(cfg.pwSleep * 60UL);
}

bool powerJson(HeapBuf& j) {
  unsigned long inSec = (cfg.pwMode == 2 && cfg.setupDone) ? (unsigned long)sleepInSec() : 0;
  return j.addf("{\"mode\":%d,\"awake\":%d,\"sleep\":%d,\"in\":%lu,\"wakes\":%lu,\"slept\":%lu,\"guide\":%s}",
                (int)cfg.pwMode, (int)cfg.pwAwake, (int)cfg.pwSleep, inSec, (unsigned long)r_wakes, (unsigned long)r_sleptSec,
                cfg.setupDone ? "true" : "false");
}

// due righe tradotte (al massimo ~250 byte): 512 bastano
std::string powerText() {
  StrText t(512);
  StrBuf& s = t.buf();
  if (cfg.pwMode == 0) s.add(tr("Risparmio energia: spento"));
  else if (cfg.pwMode == 1) s.add(tr("Risparmio energia: Wi-Fi a risparmio massimo (raggiungibile, piu lenta)"));
  else s.addf(tr("Risparmio energia: sonno a cicli, sveglia %u min, dorme %u min"), (unsigned)cfg.pwAwake, (unsigned)cfg.pwSleep);
  s.add('\n').addf(tr("Risvegli dal sonno: %lu, tempo dormito circa %lu s"), (unsigned long)r_wakes, (unsigned long)r_sleptSec).add('\n');
  return t.take();
}
