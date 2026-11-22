// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec.cpp
// Il task campiona, il grafico legge l'anello in RAM, le righe CSV si scrivono a blocchi (vos_rec_file.cpp).
#include "vos_rec.h"
#include "../core/vos_common.h"
#if VOS_WITH_REC
#include "vos_rec_file.h"
#include "vos_rec_ev.h"
#include "../core/vos_common.h"
#include "../core/vos_azioni.h"
#include "../core/vos_config.h"
#include "../core/vos_sys.h"
#include "../core/vos_i18n.h"
#include "../core/vos_lock.h"
#include "../core/vos_log.h"
#include "../core/vos_strbuf.h"
#include "../core/vos_time.h"
#include "../devices/vos_io.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REC_BOOT_WAIT_MS 20000UL     // niente campioni nei primi 20 secondi
#define REC_FLUSH_ROWS   10          // si scrive almeno ogni 10 righe...
#define REC_FLUSH_MS     60000UL     // ...e ogni 60 secondi
#define REC_PEND_MAX     1536        // righe in attesa di essere scritte
#define REC_STACK        6144        // 4096 bastava prima; l'elenco dei file ora ha nomi piu lunghi (24 voci da 56 byte)

namespace {

struct Point {
  uint32_t t;                // epoca (secondi dal 1970) o secondi dall'avvio
  bool     epoch;
  int32_t  v[REC_SLOTS];
};

VosMutex   g_mx;
Point      g_ring[REC_RING];
int        g_n = 0, g_head = 0;                // g_n punti validi, g_head = posto del prossimo
char       g_pend[REC_PEND_MAX];
size_t     g_pendLen = 0;
int        g_pendRows = 0;
bool       g_pendEpoch = false;
unsigned long g_pendSince = 0;
bool       g_needHeader = true;
unsigned long g_nextSlot[REC_SLOTS];                 // quando scade ogni fonte (0 = subito)
char       g_err[96] = "";
bool       g_started = false;

bool slotOn(const RecSettings& s, int i) {
  if (i < REC_DEV_MAX) return s.dev[i][0] != 0;
  return i == REC_SLOT_T ? s.t : s.r;
}

void slotName(const RecSettings& s, int i, char* out, size_t cap) {
  if (i < REC_DEV_MAX) snprintf(out, cap, "%s", s.dev[i]);
  else snprintf(out, cap, "%s", i == REC_SLOT_T ? "temp_c" : "ram_kb");
}

int slotKind(int i) { return i == REC_SLOT_T ? 1 : (i == REC_SLOT_R ? 2 : 0); }   // 0 valore della periferica, 1 centesimi di grado, 2 KB

void cell(char* out, size_t cap, int i, int32_t v) {
  if (v == REC_ND) { out[0] = 0; return; }
  if (slotKind(i) == 1) snprintf(out, cap, "%ld.%02ld", (long)(v / 100), (long)(labs(v) % 100));
  else snprintf(out, cap, "%ld", (long)v);
}

int minSec(int where) { return where == 1 ? 1 : 10; }

// Intervallo di una fonte in secondi: il suo, o quello predefinito se vale 0; mai sotto il minimo del posto.
unsigned slotSec(const RecSettings& s, int i) {
  unsigned v = i < REC_DEV_MAX ? s.secDev[i] : (i == REC_SLOT_T ? s.secT : s.secR);
  if (!v) v = s.sec;
  unsigned m = (unsigned)minSec(s.where);
  return v < m ? m : v;
}

void setErr(const char* t) {
  if (strcmp(g_err, t) == 0) return;
  snprintf(g_err, sizeof g_err, "%s", t);
  if (t[0]) vlog("REGISTRATORE: %s", t);
}

void header(const RecSettings& s, bool epoch, char* out, size_t cap) {
  size_t n = (size_t)snprintf(out, cap, "%s", epoch ? "epoca" : "avvio_s");
  for (int i = 0; i < REC_SLOTS && n < cap; i++) {
    if (!slotOn(s, i)) continue;
    char nm[20];
    slotName(s, i, nm, sizeof nm);
    n += (size_t)snprintf(out + n, cap - n, ",%s", nm);
    if (n >= cap) { n = cap - 1; break; }
  }
  if (n + 2 < cap) { out[n++] = '\n'; out[n] = 0; }
}

void flushLocked(const RecSettings& s) {
  if (!g_pendRows) return;
  char hd[160];
  header(s, g_pendEpoch, hd, sizeof hd);
  ErrBuf e;
  if (recFileAppend((int)s.where, s.nome, (int)s.maxKb, (int)s.limite, REC_TIPO_CAMPIONI, g_pendEpoch, hd, &g_needHeader, g_pend, g_pendLen, e)) setErr("");
  else setErr(e.c_str());
  g_pendLen = 0;
  g_pendRows = 0;
}

int32_t readSlot(const RecSettings& s, int i) {
  if (i < REC_DEV_MAX) {
    int v = 0;
    ErrBuf e;
    return ioGet(s.dev[i], v, e) ? (int32_t)v : REC_ND;
  }
  if (i == REC_SLOT_T) {
    float c = sysCpuTempLetta();
    return isnan(c) ? REC_ND : (int32_t)lroundf(c * 100.0f);
  }
  return (int32_t)(drvHeapFree() / 1024);
}

// due = elenco delle fonti scadute (un bit per fonte): le altre restano vuote in questa riga.
void sampleLocked(const RecSettings& s, unsigned due) {
  Point p;
  p.epoch = timeValid();
  p.t = p.epoch ? (uint32_t)time(nullptr) : (uint32_t)(drvMillis() / 1000);
  for (int i = 0; i < REC_SLOTS; i++) p.v[i] = (slotOn(s, i) && (due & (1u << i))) ? readSlot(s, i) : REC_ND;
  g_ring[g_head] = p;
  g_head = (g_head + 1) % REC_RING;
  if (g_n < REC_RING) g_n++;
  char row[128];
  size_t n = (size_t)snprintf(row, sizeof row, "%lu", (unsigned long)p.t);
  for (int i = 0; i < REC_SLOTS && n < sizeof row - 16; i++) {
    if (!slotOn(s, i)) continue;
    char c[16];
    cell(c, sizeof c, i, p.v[i]);
    n += (size_t)snprintf(row + n, sizeof row - n, ",%s", c);
    if (n >= sizeof row) break;
  }
  if (n + 2 > sizeof row) n = sizeof row - 2;
  row[n++] = '\n';
  row[n] = 0;
  if (g_pendRows && (g_pendEpoch != p.epoch || g_pendLen + n > sizeof g_pend)) flushLocked(s);
  if (g_pendLen + n > sizeof g_pend) return;                  // riga piu grande dell'intero blocco: non succede (max 128)
  if (!g_pendRows) { g_pendSince = drvMillis(); g_pendEpoch = p.epoch; }
  memcpy(g_pend + g_pendLen, row, n);
  g_pendLen += n;
  g_pendRows++;
  if (g_pendRows >= REC_FLUSH_ROWS) flushLocked(s);
}

void task(void*) {
  for (;;) {
    drvDelayMs(1000);
    azioniImposta(cfg.recOn ? cfg.recAzioni : 0);             // anche nei primi 20 secondi: le azioni dell'avvio restano in coda
    if (drvMillis() < REC_BOOT_WAIT_MS) continue;
    RecSettings s;
    recSettingsGet(s);
    VosLock L(g_mx);
    recEvTick(s);                                             // azioni: svuota la coda e scrive a blocchi (anche da spento: scrive cio che e rimasto)
    if (!cfg.recOn) {                                         // spento: le righe rimaste in attesa vanno scritte, poi si aspetta
      if (g_pendRows) flushLocked(s);
      for (int i = 0; i < REC_SLOTS; i++) g_nextSlot[i] = 0;
      continue;
    }
    unsigned long now = drvMillis();
    unsigned due = 0;
    for (int i = 0; i < REC_SLOTS; i++) {
      if (!slotOn(s, i)) continue;
      if (g_nextSlot[i] == 0 || (long)(now - g_nextSlot[i]) >= 0) {
        g_nextSlot[i] = now + slotSec(s, i) * 1000UL;
        if (!g_nextSlot[i]) g_nextSlot[i] = 1;
        due |= 1u << i;
      }
    }
    if (due) sampleLocked(s, due);
    if (g_pendRows && now - g_pendSince >= REC_FLUSH_MS) flushLocked(s);
  }
}

}  // namespace

void recBegin() {
  if (g_started) return;
  g_mx.init();
  recEvBegin();
  azioniImposta(cfg.recOn ? cfg.recAzioni : 0);
  g_started = drvTaskCreate(task, "rec", REC_STACK, nullptr, 1, nullptr, DRV_CORE_ANY);
  if (!g_started) vlog("REGISTRATORE: ERRORE, il task non parte (memoria)");
}

void recSettingsGet(RecSettings& s) {
  memset(&s, 0, sizeof s);
  s.on = cfg.recOn;
  s.sec = cfg.recSec;
  s.where = cfg.recWhere;
  s.maxKb = cfg.recMaxKb;
  s.limite = cfg.recLimite;
  snprintf(s.nome, sizeof s.nome, "%s", cfg.recNome.c_str());
  s.t = cfg.recT;
  s.r = cfg.recR;
  for (int i = 0; i < REC_DEV_MAX; i++) {
    snprintf(s.dev[i], sizeof s.dev[i], "%s", cfg.recDev[i].c_str());
    s.secDev[i] = cfg.recSecDev[i];
  }
  s.secT = cfg.recSecT;
  s.secR = cfg.recSecR;
  s.azioni = cfg.recAzioni;
  s.evMax = cfg.recEvMax;
}

void recSet(const RecSettings& s, ErrBuf& err) {
  if (s.where > 1) { err = tr("Posto non valido"); return; }
  if (s.sec < minSec(s.where) || s.sec > 86400) {
    err = s.where == 1 ? tr("Intervallo: da 1 secondo a 24 ore") : tr("Sulla memoria interna l'intervallo parte da 10 secondi");
    return;
  }
  if (!recNomeValido(s.nome)) { err = tr("Nome del file non valido: da 1 a 24 caratteri, solo lettere, numeri, _ e -"); return; }
  if (s.limite > 1) { err = tr("Tipo di limite non valido"); return; }
  if (s.azioni > 0x7FF) { err = tr("Categorie di azioni non valide"); return; }
  unsigned lo = (unsigned)minSec(s.where);
  unsigned propri[REC_SLOTS] = { s.secDev[0], s.secDev[1], s.secDev[2], s.secDev[3], s.secT, s.secR };
  for (int i = 0; i < REC_SLOTS; i++) {
    if (propri[i] && (propri[i] < lo || propri[i] > 86400)) {
      err = s.where == 1 ? tr("Intervallo di una fonte: da 1 secondo a 24 ore, o 0 per quello predefinito") : tr("Intervallo di una fonte: da 10 secondi a 24 ore, o 0 per quello predefinito");
      return;
    }
  }
  if (s.maxKb < 4 || (s.where == 0 && s.maxKb > 1024)) { err = tr("Spazio massimo non valido"); return; }
  for (int i = 0; i < REC_DEV_MAX; i++) {
    if (!s.dev[i][0]) continue;
    if (!ioHas(s.dev[i])) { err = tr("Periferica sconosciuta"); return; }
    for (int j = 0; j < i; j++) if (!strcmp(s.dev[i], s.dev[j])) { err = tr("Periferica ripetuta"); return; }
  }
  if (s.on && !s.t && !s.r && !s.dev[0][0] && !s.dev[1][0] && !s.dev[2][0] && !s.dev[3][0]) { err = tr("Scegli almeno una fonte"); return; }
  VosLock L(g_mx);
  RecSettings old;
  recSettingsGet(old);
  if (g_pendRows) flushLocked(old);                           // le righe in attesa vanno dove e come erano
  recEvFlush(old, 1000);                                      // e anche le azioni in attesa
  cfg.recOn = s.on;
  cfg.recSec = s.sec;
  cfg.recWhere = s.where;
  cfg.recMaxKb = s.maxKb;
  cfg.recNome = s.nome;
  cfg.recLimite = s.limite;
  cfg.recAzioni = s.azioni;
  cfg.recEvMax = s.evMax;
  cfg.recSecT = s.secT;
  cfg.recSecR = s.secR;
  azioniImposta(s.on ? s.azioni : 0);
  for (int i = 0; i < REC_DEV_MAX; i++) cfg.recSecDev[i] = s.secDev[i];
  cfg.recT = s.t;
  cfg.recR = s.r;
  for (int i = 0; i < REC_DEV_MAX; i++) cfg.recDev[i] = s.dev[i];
  g_n = 0;
  g_head = 0;
  g_needHeader = true;
  for (int i = 0; i < REC_SLOTS; i++) g_nextSlot[i] = 0;
  setErr("");
}

void recClear(ErrBuf& err) {
  VosLock L(g_mx);
  g_n = 0;
  g_head = 0;
  g_pendLen = 0;
  g_pendRows = 0;
  g_needHeader = true;
  recEvReset();
  recFileClear(cfg.recNome.c_str(), err);
}

bool recJson(HeapBuf& j, bool conPunti) {
  RecSettings s;
  recSettingsGet(s);
  VosLock L(g_mx);
  bool ok = j.addf("{\"on\":%s,\"sec\":%d,\"where\":%d,\"maxkb\":%d,\"t\":%s,\"r\":%s,\"minsec\":[10,1],\"dir\":\"%s\",\"motore\":\"task\",",
                   s.on ? "true" : "false", (int)s.sec, (int)s.where, (int)s.maxKb, s.t ? "true" : "false", s.r ? "true" : "false", recDirOf(s.where));
  ok = ok && j.addf("\"nome\":\"%s\",\"limite\":%d,\"secdev\":[%d,%d,%d,%d],\"sect\":%d,\"secr\":%d,\"azioni\":%d,\"evmax\":%d,",
                    s.nome, (int)s.limite, (int)s.secDev[0], (int)s.secDev[1], (int)s.secDev[2], (int)s.secDev[3], (int)s.secT, (int)s.secR, (int)s.azioni, (int)s.evMax);
  ok = ok && j.add("\"dev\":[");
  for (int i = 0; i < REC_DEV_MAX; i++) {
    ok = ok && (i ? j.add(',') : true) && j.add('"') && j.addJson(s.dev[i], strlen(s.dev[i])) && j.add('"');
  }
  ok = ok && j.addf("],\"caps\":{\"ulp\":%d,\"lp\":%d,\"pcnt\":%d,\"rtc\":%d},\"err\":\"", (int)VOS_HAS_ULP, (int)VOS_HAS_LP_CPU, (int)VOS_HAS_PCNT, (int)VOS_RTC_MEM_BYTES);
  ok = ok && j.addJson(g_err, strlen(g_err)) && j.add("\",\"cols\":[");
  bool first = true;
  for (int i = 0; i < REC_SLOTS; i++) {
    if (!slotOn(s, i)) continue;
    char nm[20];
    slotName(s, i, nm, sizeof nm);
    ok = ok && (first ? true : j.add(',')) && j.addf("{\"i\":%d,\"k\":%d,\"n\":\"", i, slotKind(i)) && j.addJson(nm, strlen(nm)) && j.add("\"}");
    first = false;
  }
  ok = ok && j.addf("],\"count\":%d", g_n);
  if (conPunti) {
    ok = ok && j.add(",\"fonti\":[");
    bool f1 = true;
    for (int i = 0; i < ioCount(); i++) {
      const char* nm = ioNameAt(i);
      if (!nm[0] || !strcmp(ioClassName(nm), "sd")) continue;
      ok = ok && (f1 ? true : j.add(',')) && j.add('"') && j.addJson(nm, strlen(nm)) && j.add('"');
      f1 = false;
    }
    ok = ok && j.add("],\"files\":[");
    RecFileInfo L2[REC_FILES_MAX];
    uint32_t tot = 0;
    int nf = recFileList((int)s.where, s.nome, L2, REC_FILES_MAX, &tot);
    for (int i = 0; i < nf; i++) ok = ok && (i ? j.add(',') : true) && j.add("{\"n\":\"") && j.addJson(L2[i].name, strlen(L2[i].name)) &&
                              j.addf("\",\"s\":%lu,\"q\":%lu,\"o\":%d,\"e\":%d}", (unsigned long)L2[i].size, (unsigned long)L2[i].day, L2[i].own ? 0 : 1, L2[i].ev ? 1 : 0);
    ok = ok && j.addf("],\"bytes\":%lu,\"pts\":[", (unsigned long)tot);
    for (int k = 0; k < g_n && ok; k++) {
      const Point& p = g_ring[(g_head - g_n + k + REC_RING) % REC_RING];
      ok = ok && (k ? j.add(',') : true) && j.addf("[%lu,%d", (unsigned long)p.t, p.epoch ? 1 : 0);
      for (int i = 0; i < REC_SLOTS && ok; i++) {
        if (!slotOn(s, i)) continue;
        ok = ok && (p.v[i] == REC_ND ? j.add(",null") : j.addf(",%ld", (long)p.v[i]));
      }
      ok = ok && j.add(']');
    }
    ok = ok && j.add(']') && recEvJson(j);
  }
  return ok && j.add('}') && !j.oom() && !j.over();
}

std::string recText() {
  RecSettings s;
  recSettingsGet(s);
  VosLock L(g_mx);
  StrText t(1024);
  StrBuf& b = t.buf();
  b.addf(tr("Registratore: %s, un campione ogni %u s, salva su %s (%s)"), s.on ? tr("acceso") : tr("spento"), (unsigned)s.sec,
         s.where ? tr("microSD") : tr("memoria interna"), recDirOf(s.where)).add('\n');
  b.add(tr("Fonti (ogni quanti secondi):"));
  bool any = false;
  for (int i = 0; i < REC_SLOTS; i++) {
    if (!slotOn(s, i)) continue;
    char nm[20];
    slotName(s, i, nm, sizeof nm);
    b.addf(" %s(%u)", nm, slotSec(s, i));
    any = true;
  }
  if (!any) b.add(tr(" nessuna"));
  b.add('\n').addf(s.limite ? tr("Spazio massimo %u KB per ogni file, punti in memoria %d") : tr("Spazio massimo %u KB per tutti i file, punti in memoria %d"), (unsigned)s.maxKb, g_n).add('\n');
  b.addf(tr("Nome dei file: %s"), s.nome).add('\n');
  b.add(tr("Motore: task del firmware")).add('\n');
  if (g_err[0]) b.addf(tr("Ultimo errore: %s"), g_err).add('\n');
  return t.take();
}

std::string recAzioniText() {
  VosLock L(g_mx);
  StrText t(1536);
  recEvText(t.buf());
  return t.take();
}

// Prima di un riavvio o del sonno profondo (gancio recFlush di core/vos_hooks.h): scrive su file campioni e azioni in attesa.
// Il tempo e limitato: si aspetta il blocco al massimo maxMs; la scrittura vera dura pochi decimi di secondo.
void recFlushNow(uint32_t maxMs) {
  if (!g_started) return;
  if (!g_mx.lockFor(maxMs)) return;
  RecSettings s;
  recSettingsGet(s);
  if (g_pendRows) flushLocked(s);
  recEvFlush(s, maxMs);
  g_mx.unlock();
}
#endif
