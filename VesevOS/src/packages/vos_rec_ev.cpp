// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec_ev.cpp
// Azioni nel registratore. Il gestore sul bus e BREVE: copia l'evento in una coda di 16 voci e basta (niente file, niente attese).
// Il task del registratore (vos_rec.cpp) chiama recEvTick una volta al secondo: svuota la coda e scrive eventi.csv a blocchi.
// Le funzioni di scrittura (Tick, Flush, Reset, Json, Text) girano tutte con il blocco del registratore preso: non servono altri blocchi,
// tranne quello della coda, che e l'unico toccato anche dal gestore (cioe da qualunque task che compie un'azione).
#include "vos_rec_ev.h"
#include "../core/vos_common.h"
#if VOS_WITH_REC
#include "vos_rec_file.h"
#include "../core/vos_azioni.h"
#include "../core/vos_config.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_i18n.h"
#include "../core/vos_lock.h"
#include "../core/vos_log.h"
#include "../core/vos_sys.h"
#include "../core/vos_time.h"
#include "../drivers/vos_drv_boot.h"
#include "../drivers/vos_drv_time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define EV_Q          16           // voci nella coda tra il gestore e il task
#define EV_RECENT     20           // ultime azioni tenute in RAM per la pagina
#define EV_PEND       1024         // byte di righe in attesa di essere scritte
#define EV_FLUSH_ROWS 10           // si scrive almeno ogni 10 righe...
#define EV_FLUSH_MS   60000UL      // ...e ogni 60 secondi
#define EV_RST_DEEP   8            // esp_reset_reason: uscita dal sonno profondo

namespace {

struct QEv {
  uint32_t t;                      // epoca o secondi dall'avvio
  uint8_t  epoch, cat, orig;
  int32_t  val;
  char     nome[20];
};

VosMutex g_qmx;                    // coda e ultime azioni (gestore e task)
bool     g_ready = false;
QEv      g_q[EV_Q];
int      g_qn = 0;
QEv      g_recent[EV_RECENT];      // anello: la piu nuova a g_rhead - 1
int      g_rn = 0, g_rhead = 0;
uint32_t g_tot = 0, g_lostQ = 0, g_lostMax = 0;

char          g_pend[EV_PEND];     // da qui in giu: solo con il blocco del registratore
size_t        g_pendLen = 0;
int           g_pendRows = 0;
bool          g_pendEpoch = false;
unsigned long g_pendSince = 0;
bool          g_needHeader = true;
int           g_dayKey = -1;       // AAAAMMGG del giorno contato (0 = ora non valida, -1 = nessuno)
unsigned      g_dayRows = 0;
bool          g_bootDone = false;
char          g_err[96] = "";

void setErr(const char* t) {
  if (strcmp(g_err, t) == 0) return;
  snprintf(g_err, sizeof g_err, "%s", t);
  if (t[0]) vlog("REGISTRATORE: azioni: %s", t);
}

void nowStamp(uint8_t& epoch, uint32_t& t) {
  epoch = timeValid() ? 1 : 0;
  t = epoch ? (uint32_t)time(nullptr) : (uint32_t)(drvMillis() / 1000);
}

void copyName(char* out, size_t cap, const char* in) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < cap; i++) out[i] = (in[i] == ',' || in[i] == '\n' || in[i] == '\r') ? '_' : in[i];
  out[i] = 0;
}

// Mette la voce in coda e fra le ultime. Breve e senza attese lunghe: lo chiama anche il gestore del bus.
void push(const QEv& q) {
  g_qmx.lock();
  g_tot++;
  g_recent[g_rhead] = q;
  g_rhead = (g_rhead + 1) % EV_RECENT;
  if (g_rn < EV_RECENT) g_rn++;
  if (g_qn < EV_Q) g_q[g_qn++] = q;
  else g_lostQ++;                                            // coda piena: l'azione resta nelle ultime ma non va nel file
  g_qmx.unlock();
}

void onEvent(const Event& e) {
  int cat = azioneCatDaTopic(e.topic);
  if (cat < 0 || !cfg.recOn || !(cfg.recAzioni & (1u << cat))) return;
  QEv q;
  memset(&q, 0, sizeof q);
  nowStamp(q.epoch, q.t);
  int o;
  int32_t v;
  azioneDecodifica(e.val, o, v);
  q.cat = (uint8_t)cat;
  q.orig = (uint8_t)o;
  q.val = v;
  copyName(q.nome, sizeof q.nome, e.arg);
  push(q);
}

int dayKeyOf(const QEv& q) {
  if (!q.epoch) return 0;
  time_t t = (time_t)q.t;
  struct tm tmv;
  localtime_r(&t, &tmv);
  return (tmv.tm_year + 1900) * 10000 + (tmv.tm_mon + 1) * 100 + tmv.tm_mday;
}

void flushLocked(const RecSettings& s) {
  if (!g_pendRows) return;
  char hd[64];
  snprintf(hd, sizeof hd, "%s,categoria,nome,valore,origine\n", g_pendEpoch ? "epoca" : "avvio_s");
  ErrBuf e;
  if (recFileAppend((int)s.where, s.nome, (int)s.maxKb, (int)s.limite, REC_TIPO_EVENTI, g_pendEpoch, hd, &g_needHeader, g_pend, g_pendLen, e)) setErr("");
  else setErr(e.c_str());
  g_pendLen = 0;
  g_pendRows = 0;
}

void addRow(const RecSettings& s, const QEv& q) {
  int key = dayKeyOf(q);
  if (key != g_dayKey) { g_dayKey = key; g_dayRows = 0; }
  if (s.evMax && g_dayRows >= s.evMax) { g_lostMax++; return; }      // tetto di righe al giorno: per non riempire lo spazio
  char row[80];
  int n = snprintf(row, sizeof row, "%lu,%s,%s,%ld,%s\n", (unsigned long)q.t, azioneNomeCat(q.cat), q.nome, (long)q.val, azioneNomeOrigine(q.orig));
  if (n <= 0 || n >= (int)sizeof row) return;
  if (g_pendRows && (g_pendEpoch != (q.epoch != 0) || g_pendLen + (size_t)n > sizeof g_pend)) flushLocked(s);
  if (!g_pendRows) { g_pendSince = drvMillis(); g_pendEpoch = q.epoch != 0; }
  memcpy(g_pend + g_pendLen, row, (size_t)n);
  g_pendLen += (size_t)n;
  g_pendRows++;
  g_dayRows++;
  if (g_pendRows >= EV_FLUSH_ROWS) flushLocked(s);
}

void drainLocked(const RecSettings& s) {
  QEv loc[EV_Q];
  g_qmx.lock();
  int n = g_qn;
  memcpy(loc, g_q, sizeof(QEv) * (size_t)n);
  g_qn = 0;
  g_qmx.unlock();
  for (int i = 0; i < n; i++) addRow(s, loc[i]);
}

// L'avvio o il risveglio: una volta per accensione. Porta l'ora dell'avvio vero, non quella di adesso.
void logBoot(const RecSettings& s) {
  g_bootDone = true;
  if (!(s.azioni & (1u << AZ_AVVIO))) return;
  QEv q;
  memset(&q, 0, sizeof q);
  uint32_t up = (uint32_t)(drvMillis() / 1000);
  q.epoch = timeValid() ? 1 : 0;
  q.t = q.epoch ? (uint32_t)time(nullptr) - up : 0;
  int why = drvResetReason();
  q.cat = AZ_AVVIO;
  q.orig = AO_SISTEMA;
  q.val = why;
  copyName(q.nome, sizeof q.nome, why == EV_RST_DEEP ? "risveglio" : "avvio");
  push(q);
}

}  // namespace

void recEvBegin() {
  if (g_ready) return;
  g_qmx.init();
  g_ready = EventBus::subscribe("act.", onEvent);
  if (!g_ready) vlog("REGISTRATORE: ERRORE, non riesco ad ascoltare le azioni (ascoltatori pieni)");
}

void recEvTick(const RecSettings& s) {
  if (!g_ready) return;
  if (!g_bootDone && s.on) logBoot(s);
  drainLocked(s);
  if (g_pendRows && drvMillis() - g_pendSince >= EV_FLUSH_MS) flushLocked(s);
}

void recEvFlush(const RecSettings& s, uint32_t maxMs) {
  (void)maxMs;                                               // il blocco del registratore lo ha preso chi chiama (con il suo tetto di attesa)
  if (!g_ready) return;
  drainLocked(s);
  flushLocked(s);
}

void recEvReset() {
  g_qmx.lock();
  g_qn = 0;
  g_rn = 0;
  g_rhead = 0;
  g_tot = g_lostQ = g_lostMax = 0;
  g_qmx.unlock();
  g_pendLen = 0;
  g_pendRows = 0;
  g_needHeader = true;
  g_dayKey = -1;
  g_dayRows = 0;
}

// Ultime azioni, la piu nuova per prima. Ritorna quante ne ha copiate.
static int copyRecent(QEv* out) {
  g_qmx.lock();
  int n = g_rn;
  for (int k = 0; k < n; k++) out[k] = g_recent[(g_rhead - 1 - k + 2 * EV_RECENT) % EV_RECENT];
  g_qmx.unlock();
  return n;
}

bool recEvJson(HeapBuf& j) {
  QEv r[EV_RECENT];
  int n = copyRecent(r);
  bool ok = j.addf(",\"ev\":{\"tot\":%lu,\"persi\":%lu,\"scartati\":%lu,\"err\":\"", (unsigned long)g_tot, (unsigned long)g_lostQ, (unsigned long)g_lostMax);
  ok = ok && j.addJson(g_err, strlen(g_err)) && j.add("\",\"ultime\":[");
  for (int k = 0; k < n && ok; k++) {
    char txt[48] = "";
    if (r[k].cat == AZ_AVVIO) sysResetNameTo((int)r[k].val, txt, sizeof txt);
    ok = ok && (k ? j.add(',') : true) && j.addf("[%lu,%d,\"%s\",\"", (unsigned long)r[k].t, (int)r[k].epoch, azioneNomeCat(r[k].cat));
    ok = ok && j.addJson(r[k].nome, strlen(r[k].nome)) && j.addf("\",%ld,\"%s\",\"", (long)r[k].val, azioneNomeOrigine(r[k].orig));
    ok = ok && j.addJson(txt, strlen(txt)) && j.add("\"]");
  }
  return ok && j.add("]}");
}

void recEvText(StrBuf& b) {
  b.add(tr("Azioni tracciate:"));
  for (int c = 0; c < AZ_N; c++) b.addf(" %s%s", cfg.recAzioni & (1u << c) ? "+" : "-", azioneNomeCat(c));
  b.add('\n');
  b.addf(tr("Azioni registrate: %lu, perse per coda piena: %lu, oltre il limite del giorno: %lu"), (unsigned long)g_tot, (unsigned long)g_lostQ, (unsigned long)g_lostMax).add('\n');
  if (g_err[0]) b.addf(tr("Ultimo errore: %s"), g_err).add('\n');
  QEv r[EV_RECENT];
  int n = copyRecent(r);
  if (!n) { b.add(tr("Nessuna azione ancora")).add('\n'); return; }
  for (int k = 0; k < n && k < 10; k++) {
    char when[TIME_FMT_MAX];
    if (r[k].epoch) timeFmtEpoch(r[k].t, true, when, sizeof when);   // epoca: nel formato di Localizzazione
    else snprintf(when, sizeof when, "[%lus]", (unsigned long)r[k].t);     // ora non valida: secondi dall'avvio
    b.addf("%s %s %s %ld %s\n", when, azioneNomeCat(r[k].cat), r[k].nome, (long)r[k].val, azioneNomeOrigine(r[k].orig));
  }
}
#endif
