// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_riserva.cpp
// Copia di riserva del firmware in uso sulla microSD, fatta prima di un'installazione automatica. Solo il programma:
// impostazioni, password e chiavi stanno in NVS e nei file e non si copiano. Una riserva sola: resta fino all'installazione dopo.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_ota.h"
#include "vos_aggiorna_dati.h"
#include "vos_aggiorna_int.h"
#include "vos_aggiorna_sd.h"

#if VOS_WITH_SD

namespace {

const char* kCartella = "/AGG";
const char* kRiserva = "/AGG/RIS.BIN";
const char* kTemporaneo = "/AGG/RIS.TMP";

struct Copia {
  uint32_t off;
  uint32_t dim;
  char errore[SD_ERR_MAX];
};

// Legge un blocco del programma e lo scrive sulla microSD: l'impronta si calcola su quello che esce dalla flash.
int copiaBlocco(void* ctx, uint8_t* buf, size_t max) {
  Copia* c = (Copia*)ctx;
  size_t n = c->dim - c->off < max ? c->dim - c->off : max;
  if (!n) return 0;
  if (!drvApp0Leggi(c->off, buf, n)) { snprintf(c->errore, sizeof c->errore, "%s", tr("Non riesco a leggere il firmware in uso")); return -1; }
  if (!sdUp() || !sdFlussoScrivi(buf, n, c->errore, sizeof c->errore)) return -1;
  c->off += (uint32_t)n;
  return (int)n;
}

struct Rilettura {
  uint32_t off;
};

int rileggiBlocco(void* ctx, uint8_t* buf, size_t max) {
  Rilettura* r = (Rilettura*)ctx;
  char err[SD_ERR_MAX];
  int n = sdReadAt(kTemporaneo, r->off, buf, max > SD_IO_MAX ? SD_IO_MAX : max, err, sizeof err);
  if (n > 0) r->off += (uint32_t)n;
  return n;
}

void togli(const char* file) {
  char err[SD_ERR_MAX];
  sdRemove(file, err, sizeof err);              // se non c'e va bene lo stesso
}

bool spazioPer(uint32_t dim) {
  uint32_t totaleKb = 0, usatoKb = 0;
  return sdSpaceKb(totaleKb, usatoKb) && totaleKb >= usatoKb && aggSpazioRiserva(totaleKb - usatoKb, dim);
}

// Scrive il temporaneo e lo rilegge. La riserva vecchia si toglie solo quando la nuova e buona.
bool copiaControllata(AggRiserva& r, uint8_t* buf, ErrBuf& err) {
  char errore[SD_ERR_MAX];
  if (!sdFlussoApri(kTemporaneo, errore, sizeof errore)) { err = errore; return false; }
  Copia c = {};
  c.dim = r.dim;
  bool letta = sha256HexLetto(copiaBlocco, &c, buf, SD_IO_MAX, r.sha);
  bool chiusa = sdFlussoChiudi(errore, sizeof errore);
  if (!letta) { err = c.errore[0] ? c.errore : tr("La microSD non si legge"); return false; }
  if (!chiusa) { err = errore; return false; }
  Rilettura l = {};
  char riletta[65];
  if (!sha256HexLetto(rileggiBlocco, &l, buf, SD_IO_MAX, riletta) || l.off != r.dim || strcasecmp(riletta, r.sha)) {
    err = tr("La copia di riserva sulla microSD non corrisponde");
    return false;
  }
  togli(kRiserva);
  if (!sdRename(kTemporaneo, kRiserva, errore, sizeof errore)) { err = errore; return false; }
  return true;
}

}  // namespace

const char* aggFileRiserva() { return kRiserva; }

bool aggSdRiserva(AggRiserva& r, ErrBuf& err) {
  memset(&r, 0, sizeof r);
  r.dim = drvSketchSize();
  if (!r.dim || r.dim > drvFreeSketchSpace()) { err = tr("Non riesco a leggere il firmware in uso"); return false; }
  char errore[SD_ERR_MAX];
  sdMakeDir(kCartella, errore, sizeof errore);  // se c'e gia va bene
  togli(kTemporaneo);
  if (!spazioPer(r.dim)) togli(kRiserva);       // la vecchia serve solo se manca il posto per la nuova: conta il firmware in uso
  if (!spazioPer(r.dim)) {
    vlog("AGGIORNAMENTI: microSD con poco spazio: installazione senza copia di riserva");
    return true;
  }
  uint8_t* buf = (uint8_t*)drvMallocBig(SD_IO_MAX);
  if (!buf) { err = tr("Memoria insufficiente"); return false; }
  bool ok = copiaControllata(r, buf, err);
  free(buf);
  if (!ok) { togli(kTemporaneo); return false; }
  r.pronta = true;
  return true;
}

#else

const char* aggFileRiserva() { return ""; }

#endif
