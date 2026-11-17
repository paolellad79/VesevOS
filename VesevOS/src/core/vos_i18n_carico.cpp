// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_carico.cpp
// Regole del caricamento dei file di lingua (vedi vos_i18n_carico.h).
#include "vos_i18n_carico.h"
#include "vos_i18n.h"
#include "vos_i18n_int.h"
#include "vos_i18n_salute_rule.h"
#include "vos_config.h"
#include "../drivers/vos_drv_fs.h"
#include <string.h>

int langFileKind(const char* percorso, char* code, size_t cap) {
  const size_t dirLen = sizeof(LANG_DIR) - 1;
  if (strncmp(percorso, LANG_DIR, dirLen) != 0) return LANGF_FUORI;
  if (percorso[dirLen] == 0) return LANGF_FUORI;                // "/lang" stessa: e la cartella, non un file
  if (percorso[dirLen] != '/') return LANGF_FUORI;              // "/langs", "/lang2": un'altra cartella
  const char* nome = percorso + dirLen + 1;
  const char* punto = strrchr(nome, '.');
  if (!punto || strchr(nome, '/')) return LANGF_NO;             // sottocartelle o senza estensione
  size_t n = (size_t)(punto - nome);
  if (n >= cap || !langCodeValid(std::string_view(nome, n))) return LANGF_NO;
  if (n == 2 && memcmp(nome, "it", 2) == 0) return LANGF_NO;    // l'italiano e nel firmware
  int kind = LANGF_NO;
  if (strcmp(punto, ".vld") == 0) kind = LANGF_VLD;
  else if (strcmp(punto, ".vlp") == 0) kind = LANGF_VLP;
  if (kind == LANGF_NO) return LANGF_NO;
  memcpy(code, nome, n);
  code[n] = 0;
  return kind;
}

size_t langFileMax(int kind) {
  if (kind == LANGF_VLD) return LANG_VLD_MAX;
  if (kind == LANGF_VLP) return LANG_VLP_MAX;
  return 0;
}

bool langFileControlla(const char* percorso, int kind, ErrBuf& err) {
  VosFile f = drvFsOpen(percorso, "r");
  if (!f || f.isDirectory()) { err = tr("Caricamento non riuscito"); return false; }
  size_t max = langFileMax(kind);
  if (!max || f.size() > max) { err = tr("File di lingua troppo grande"); return false; }
  if (kind == LANGF_VLD) {
    LangVld d;
    if (!langLeggiIntest(f, d)) { err = tr("File di lingua non valido"); return false; }
    return true;
  }
  uint8_t h[3];
  bool gzip = f.size() >= 18 && f.seek(0) && f.read(h, sizeof h) == sizeof h && h[0] == 0x1f && h[1] == 0x8b && h[2] == 0x08;
  if (!gzip) { err = tr("File della pagina non valido"); return false; }
  return true;
}

bool langFileCaricato(const char* caricato, const char* finale, int kind, const char* code, ErrBuf& err) {
  if (!langFileControlla(caricato, kind, err)) {
    drvFsRemove(caricato);
    return false;
  }
  // Se e il dizionario in uso, tra lo scambio del file e langSet() il file e gia nuovo e la copia in memoria e ancora quella vecchia:
  // in quel tratto un guasto rilevato da tr() non e un guasto vero e non si segnala.
  bool inUso = kind == LANGF_VLD && cfg.lang == code;
  if (inUso) langSostituzione(true);
  if (!drvFsRename(caricato, finale)) {     // LittleFS sostituisce in modo atomico: resta il vecchio o il nuovo
    if (inUso) langSostituzione(false);
    drvFsRemove(caricato);
    err = tr("Impossibile scrivere");
    return false;
  }
  if (inUso) {
    ErrBuf e;
    if (!langSet(code, e)) {       // il file nuovo era stato controllato: se non si carica, quello in uso non vale piu
      langSostituzione(false);
      langErroreImposta(code, LG_INTESTAZIONE);
      err = e.c_str();
      return false;
    }
    langSostituzione(false);                // rilegge il dizionario in uso (la memoria dei testi si svuota, lo stato di errore si azzera)
  }
  return true;
}
