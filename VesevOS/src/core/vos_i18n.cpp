// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.cpp
// tr() cerca la frase nel dizionario /lang/<codice>.vld A SALTI SUL FILE, senza caricarlo in RAM (la logica pura sta in
// vos_i18n_calc.h). Il file si APRE A OGNI RICERCA e si chiude subito: un file aperto su LittleFS tiene in RAM una
// cache e un descrittore (alcune centinaia di byte, non misurati) per tutto il tempo, e sulla C3 la RAM e poca; le ricerche
// vere sono rare perche i testi recenti restano nella memoria circolare (vedi LangCache e la sua garanzia sui puntatori).
//
// RAM FISSA (non misurata con una build): lo stato "Stato" qui sotto, circa 4,8 KB, preso con malloc la prima volta che
// si sceglie una lingua diversa dall'italiano e MAI piu liberato (cosi un cambio di lingua non puo fallire per memoria
// frammentata). Con l'italiano solo: 0 byte. STACK di tr(): circa 250 byte propri (buffer di ricerca 160 byte, percorso
// non sullo stack) piu le chiamate di LittleFS per aprire e leggere, che le altre parti del firmware usano gia.
// CONCORRENZA: tr() puo essere chiamata da piu task; un mutex ricorsivo (VosMutex) protegge ricordi e file.
// Dentro il blocco ci sono letture di LittleFS: se un altro task sta scrivendo la flash la lettura puo aspettare.
#include "vos_i18n.h"
#include "vos_i18n_int.h"
#include "vos_i18n_calc.h"
#include "vos_i18n_fmt_rule.h"
#include "vos_i18n_salute_rule.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_lock.h"
#include "../drivers/vos_drv_fs.h"
#include "vos_lang_it.h"
#include <stdarg.h>
#include <stdio.h>
#include <new>

namespace {

struct Stato {
  LangVld vld;
  char path[sizeof(LANG_DIR) + LANG_CODE_BUF + 6];
  char code[LANG_CODE_BUF];
  LangCache cache;
};

Stato* g_s = nullptr;            // preso la prima volta e mai liberato
bool g_attiva = false;           // false = italiano (nessuna ricerca)

VosMutex& mux() {
  static VosMutex m = [] { VosMutex v; v.init(); return v; }();
  return m;
}

LangLetture g_letture;           // errori di lettura consecutivi (protetto da mux)

// Il file del dizionario per LangLettore (vos_i18n_salute_rule.h): si apre alla PRIMA lettura e si chiude da solo a fine ricerca.
struct FonteVld {
  const char* path;
  VosFile f;
  bool apri() {
    f = drvFsOpen(path, "r");
    return f && !f.isDirectory();
  }
  bool esiste() { return f ? false : drvFsExists(path); }     // una cartella al posto del file non e "il file c'e"
  uint32_t dimensione() { return (uint32_t)f.size(); }
  bool leggi(uint32_t pos, void* buf, size_t n) { return f.seek(pos) && f.read((uint8_t*)buf, n) == n; }
};

}  // namespace

bool langCodeValid(std::string_view c) {
  if (c.size() < 2 || c.size() > 8) return false;
  for (char ch : c) {
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))) return false;
  }
  return true;
}

void langPath(const char* code, const char* ext, char* out, size_t cap) {
  snprintf(out, cap, LANG_DIR "/%s.%s", code, ext);
}

bool langLeggiIntest(VosFile& f, LangVld& d) {
  uint8_t h[LANG_INTEST];
  size_t sz = f.size();
  if (sz < LANG_INTEST || sz > LANG_VLD_MAX) return false;
  if (!f.seek(0) || f.read(h, sizeof h) != sizeof h) return false;
  return langVldIntest(h, sz, d);
}

// Controlla il file /lang/<code>.vld e, se e buono, lo mette in uso (nessun testo si carica).
static bool loadFile(const char* code, ErrBuf& err, int* guasto = nullptr) {
  char path[sizeof(LANG_DIR) + LANG_CODE_BUF + 6];
  langPath(code, "vld", path, sizeof path);
  LangVld vld;
  {
    VosFile f = drvFsOpen(path, "r");
    if (!f || f.isDirectory()) { err = tr("Lingua non installata"); if (guasto) *guasto = LG_NON_SI_APRE; return false; }
    if (!langLeggiIntest(f, vld)) { err = tr("File di lingua non valido"); if (guasto) *guasto = LG_INTESTAZIONE; return false; }
  }
  VosLock l(mux());
  if (!g_s) {
    g_s = (Stato*)malloc(sizeof(Stato));
    if (!g_s) { err = tr("Memoria esaurita"); return false; }
    new (g_s) Stato();
  }
  g_s->vld = vld;
  snprintf(g_s->path, sizeof g_s->path, "%s", path);
  snprintf(g_s->code, sizeof g_s->code, "%s", code);
  g_s->cache.svuota();
  g_letture.riuscita();
  g_attiva = true;
  langErroreAzzera();                               // file valido in uso: la lingua non e piu in errore
  vlog("LANG: %s (%d voci)", code, (int)vld.n);
  return true;
}

static void usaItaliano() {
  VosLock l(mux());
  g_attiva = false;
  langErroreAzzera();
}

void langInit() {
  if (cfg.lang == "it" || !langCodeValid(cfg.lang.view())) { usaItaliano(); return; }
  ErrBuf err;
  int guasto = LG_NESSUNO;
  if (!loadFile(cfg.lang.c_str(), err, &guasto)) {
    vlog("LANG: %s non caricata, uso l'italiano", cfg.lang.c_str());
    langErroreImposta(cfg.lang.c_str(), guasto);    // la lingua scelta ha un file in errore: la pagina si apre in emergenza
  }
}

bool langSet(const char* code, ErrBuf& err) {
  if (!langCodeValid(code)) { err = tr("Lingua non valida"); return false; }
  if (strcmp(code, "it") == 0) { usaItaliano(); cfg.lang = "it"; cfgSave(); return true; }
  if (!loadFile(code, err)) return false;
  cfg.lang = code; cfgSave();
  return true;
}

const char* tr(const char* it) {
  if (!g_attiva || !it || langInErrore()) return it;   // lingua in errore: italiano, nessun accesso al file
  char code[LANG_CODE_BUF];
  const char* t = nullptr;
  LangGuasto guasto = LG_NESSUNO;
  {
    VosLock l(mux());
    if (!g_attiva) return it;                       // cambiata mentre aspettavo il blocco
    FonteVld fonte{g_s->path};
    LangLettore<FonteVld> leggi(fonte, g_s->vld);
    bool fuori = false;
    t = langTraduci(g_s->cache, leggi, g_s->vld, it, strlen(it), &fuori);
    guasto = langEsitoRicerca(leggi.aperto, leggi.guasto, leggi.errore, fuori, g_letture);
    snprintf(code, sizeof code, "%s", g_s->code);
  }
  if (guasto != LG_NESSUNO) {                       // il file non e piu quello caricato: da ora italiano; l'avviso lo da langTick()
    langErroreImposta(code, guasto);
    return it;
  }
  if (!t) return it;
  // Un testo tradotto puo finire come formato di printf (trf, addf, snprintf...): se i suoi specificatori non sono quelli dell'italiano
  // (un file di lingua sbagliato o manomesso: %n, %s in piu...) si usa l'italiano. Costo: una scansione dei due testi, nessuna memoria.
  if (!i18nFormatiUguali(it, t)) return it;
  return t;
}

TrText trf(const char* it, ...) {
  const char* f = tr(it);
  TrText r;
  va_list ap; va_start(ap, it);
  int n = vsnprintf(r.small_, sizeof(r.small_), f, ap);
  va_end(ap);
  if (n < (int)sizeof(r.small_)) return r;           // entra nel piccolo (o errore di formato: resta il testo troncato)
  r.big_ = (char*)malloc(n + 1);
  if (!r.big_) return r;                             // manca la memoria: resta il testo troncato
  va_start(ap, it);
  vsnprintf(r.big_, n + 1, f, ap);
  va_end(ap);
  return r;
}
