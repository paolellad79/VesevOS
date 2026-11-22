// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_timefmt.cpp
// Data e ora nei formati scelti in Localizzazione. La logica dei formati e in vos_datefmt.cpp (pura); qui ci sono le scelte
// (cfg), i nomi dei giorni e dei mesi nella lingua in uso e le funzioni che usano gli altri moduli.
//   CORTA (tabelle, registro, diario, elenchi, registratore, orari): data pronta o scritta a mano, ora con ore e minuti.
//   LUNGA (riquadri grandi: testata, accesso, pagina Ora): data abbreviata, completa o scritta a mano; ora con o senza secondi.
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "vos_time.h"
#include "vos_datefmt.h"
#include "vos_config.h"
#include "vos_i18n.h"

// ---- nomi nella lingua in uso, in RAM (circa 600 byte): si riempiono una volta e di nuovo solo se cambia la lingua ----
// Cercarli con tr() a ogni riga del registro costerebbe 2-3 ricerche nel file di lingua per riga.
static DfNomi g_nomi;
static char g_lingua[16];
static bool g_langErrore = false;
static bool g_pronto = false;
static bool g_riempie = false;                 // tr() puo scrivere nel registro e tornare qui: in quel caso si usa la tabella com'e
static std::recursive_mutex g_mx;

static void copia(char* dst, size_t n, const char* src) { snprintf(dst, n, "%s", src); }

// I puntatori di tr() si copiano subito (vedi vos_i18n.h). Un tr() per ogni nome: lo scanner delle traduzioni vuole le stringhe letterali.
static void riempiGiorni() {
  copia(g_nomi.giornoBreve[0], DF_NOME_MAX, tr("Dom."));
  copia(g_nomi.giornoBreve[1], DF_NOME_MAX, tr("Lun."));
  copia(g_nomi.giornoBreve[2], DF_NOME_MAX, tr("Mar."));
  copia(g_nomi.giornoBreve[3], DF_NOME_MAX, tr("Mer."));
  copia(g_nomi.giornoBreve[4], DF_NOME_MAX, tr("Giov."));
  copia(g_nomi.giornoBreve[5], DF_NOME_MAX, tr("Ven."));
  copia(g_nomi.giornoBreve[6], DF_NOME_MAX, tr("Sab."));
  copia(g_nomi.giornoLungo[0], DF_NOME_MAX, tr("Domenica"));
  copia(g_nomi.giornoLungo[1], DF_NOME_MAX, tr("Lunedi"));
  copia(g_nomi.giornoLungo[2], DF_NOME_MAX, tr("Martedi"));
  copia(g_nomi.giornoLungo[3], DF_NOME_MAX, tr("Mercoledi"));
  copia(g_nomi.giornoLungo[4], DF_NOME_MAX, tr("Giovedi"));
  copia(g_nomi.giornoLungo[5], DF_NOME_MAX, tr("Venerdi"));
  copia(g_nomi.giornoLungo[6], DF_NOME_MAX, tr("Sabato"));
}

// Una frase intera per mese: cosi "Mar." (marzo) non si confonde con "Mar." (martedi) e ogni lingua mette giorno e punteggiatura come le serve
static void riempiMesiBrevi() {
  copia(g_nomi.fraseMese[0], DF_FRASE_MAX, tr("%d Gen. %d"));
  copia(g_nomi.fraseMese[1], DF_FRASE_MAX, tr("%d Feb. %d"));
  copia(g_nomi.fraseMese[2], DF_FRASE_MAX, tr("%d Mar. %d"));
  copia(g_nomi.fraseMese[3], DF_FRASE_MAX, tr("%d Apr. %d"));
  copia(g_nomi.fraseMese[4], DF_FRASE_MAX, tr("%d Mag. %d"));
  copia(g_nomi.fraseMese[5], DF_FRASE_MAX, tr("%d Giu. %d"));
  copia(g_nomi.fraseMese[6], DF_FRASE_MAX, tr("%d Lug. %d"));
  copia(g_nomi.fraseMese[7], DF_FRASE_MAX, tr("%d Ago. %d"));
  copia(g_nomi.fraseMese[8], DF_FRASE_MAX, tr("%d Set. %d"));
  copia(g_nomi.fraseMese[9], DF_FRASE_MAX, tr("%d Ott. %d"));
  copia(g_nomi.fraseMese[10], DF_FRASE_MAX, tr("%d Nov. %d"));
  copia(g_nomi.fraseMese[11], DF_FRASE_MAX, tr("%d Dic. %d"));
}

static void riempiMesiLunghi() {
  copia(g_nomi.meseLungo[0], DF_NOME_MAX, tr("gennaio"));
  copia(g_nomi.meseLungo[1], DF_NOME_MAX, tr("febbraio"));
  copia(g_nomi.meseLungo[2], DF_NOME_MAX, tr("marzo"));
  copia(g_nomi.meseLungo[3], DF_NOME_MAX, tr("aprile"));
  copia(g_nomi.meseLungo[4], DF_NOME_MAX, tr("maggio"));
  copia(g_nomi.meseLungo[5], DF_NOME_MAX, tr("giugno"));
  copia(g_nomi.meseLungo[6], DF_NOME_MAX, tr("luglio"));
  copia(g_nomi.meseLungo[7], DF_NOME_MAX, tr("agosto"));
  copia(g_nomi.meseLungo[8], DF_NOME_MAX, tr("settembre"));
  copia(g_nomi.meseLungo[9], DF_NOME_MAX, tr("ottobre"));
  copia(g_nomi.meseLungo[10], DF_NOME_MAX, tr("novembre"));
  copia(g_nomi.meseLungo[11], DF_NOME_MAX, tr("dicembre"));
}

// Da chiamare con il blocco preso
static const DfNomi& nomiAttuali() {
  bool errore = langInErrore();
  bool cambiata = !g_pronto || g_langErrore != errore || strcmp(g_lingua, cfg.lang.c_str()) != 0;
  if (cambiata && !g_riempie) {
    g_riempie = true;
    riempiGiorni();
    riempiMesiBrevi();
    riempiMesiLunghi();
    copia(g_lingua, sizeof g_lingua, cfg.lang.c_str());
    g_langErrore = errore;
    g_pronto = true;
    g_riempie = false;
  }
  return g_nomi;
}

static DfImpost impostazioni() {
  DfImpost im;
  im.dataCorta = cfg.dateFmt;
  im.dataCortaTesto = cfg.dataCortaTesto.c_str();
  im.dataLunga = cfg.dataLungaFmt;
  im.dataLungaTesto = cfg.dataLungaTesto.c_str();
  im.ampm = cfg.timeFmt != 0;
  im.oraLungaSec = cfg.oraLungaSec;
  im.oraLunga = cfg.oraLungaFmt;
  im.oraLungaTesto = cfg.oraLungaTesto.c_str();
  return im;
}

static void partiDaEpoca(uint32_t ep, DfData& d) {
  time_t t = (time_t)ep;
  struct tm tmv;
  localtime_r(&t, &tmv);
  d.anno = tmv.tm_year + 1900;
  d.mese = tmv.tm_mon + 1;
  d.giorno = tmv.tm_mday;
  d.giornoSett = tmv.tm_wday;
  d.ore = tmv.tm_hour;
  d.minuti = tmv.tm_min;
  d.secondi = tmv.tm_sec;
}

void timeFmtEpoch(uint32_t ep, bool conData, char* out, size_t n) {
  if (!out || !n) return;
  if (!ep) {
    snprintf(out, n, "-");
    return;
  }
  DfData d;
  partiDaEpoca(ep, d);
  DfImpost im = impostazioni();
  char ora[TIME_FMT_MAX];
  dfOraCorta(im, d, ora, sizeof ora);
  if (!conData) {
    snprintf(out, n, "%s", ora);
    return;
  }
  char data[TIME_FMT_MAX];
  {
    std::lock_guard<std::recursive_mutex> g(g_mx);
    dfDataCorta(im, d, nomiAttuali(), data, sizeof data);
  }
  snprintf(out, n, "%s %s", data, ora);
}

void timeFmtEpochLungo(uint32_t ep, bool conData, char* out, size_t n) {
  if (!out || !n) return;
  if (!ep) {
    snprintf(out, n, "-");
    return;
  }
  DfData d;
  partiDaEpoca(ep, d);
  DfImpost im = impostazioni();
  char ora[TIME_FMT_MAX];
  char data[TIME_FMT_MAX];
  {
    std::lock_guard<std::recursive_mutex> g(g_mx);
    const DfNomi& nomi = nomiAttuali();
    dfOraLunga(im, d, nomi, ora, sizeof ora);
    dfDataLunga(im, d, nomi, data, sizeof data);
  }
  if (conData) snprintf(out, n, "%s %s", data, ora);
  else snprintf(out, n, "%s", ora);
}

void timeFmtMinutes(unsigned m, char* out, size_t n) {
  if (!out || !n) return;
  m %= 1440;
  DfData d = {};
  d.ore = (int)(m / 60);
  d.minuti = (int)(m % 60);
  dfOraCorta(impostazioni(), d, out, n);
}

void timeNowFmt(char* out, size_t n) {
  if (!out || !n) return;
  if (!timeValid()) {
    snprintf(out, n, "%s", tr("non impostata"));
    return;
  }
  timeFmtEpoch((uint32_t)time(nullptr), true, out, n);
}

// ---- controllo dei formati scritti a mano ----
const char* timeFmtMessaggio(int errore) {
  switch ((DfErr)errore) {
    case DF_VUOTO: return tr("Formato vuoto: scrivi il formato");
    case DF_LUNGO: return tr("Formato troppo lungo (massimo 32 caratteri)");
    case DF_CARATTERE: return tr("Caratteri non ammessi nel formato (solo ASCII, senza virgolette e senza barra rovesciata)");
    case DF_SEGNO: return tr("Segno non riconosciuto dopo il segno di percentuale");
    case DF_PERCENTO_FINALE: return tr("Segno di percentuale isolato in fondo al formato");
    default: return "";
  }
}

// Un testo scritto a mano deve essere valido se e scelto; se non e scelto, quando c'e, va controllato lo stesso
static bool testoOk(bool scelto, const char* testo, ErrBuf& err) {
  if (!scelto && (!testo || !testo[0])) return true;
  DfErr e = dfControlla(testo);
  if (e == DF_OK) return true;
  err = timeFmtMessaggio((int)e);
  return false;
}

bool timeFmtControlla(int dataCorta, const char* testoCorta, int dataLunga, const char* testoLunga, int oraLunga, const char* testoOra, ErrBuf& err) {
  return testoOk(dataCorta == 3, testoCorta, err) && testoOk(dataLunga == 2, testoLunga, err) && testoOk(oraLunga == 1, testoOra, err);
}
