// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_sd.cpp
#include "vos_aggiorna_sd.h"
#include "vos_aggiorna_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_updorder.h"
#include "../core/vos_util.h"
#include "../devices/vos_io.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_https.h"
#include "../drivers/vos_drv_ota.h"

#if VOS_WITH_SD

namespace {

const char* kCartella = "/AGG";                 // nomi corti: il recovery legge la FAT senza nomi lunghi
const char* kFile = "/AGG/FW.BIN";
const uint32_t kTempoScarico = 600000;          // tetto ai 10 minuti: la scheda intanto lavora
const uint32_t kMargineKb = 64;

struct Scrittura {
  bool primo;                                   // il primo blocco si controlla: se non e un firmware inutile continuare
  char errore[SD_ERR_MAX];
};

bool scriviBlocco(void* ctx, const uint8_t* dati, size_t n) {
  Scrittura* s = (Scrittura*)ctx;
  if (s->primo) {
    if (!aggTestaFirmware(dati, n)) { snprintf(s->errore, sizeof s->errore, "%s", tr("Il file scaricato non e un firmware valido")); return false; }
    s->primo = false;
  }
  if (!sdUp()) { snprintf(s->errore, sizeof s->errore, "%s", tr("La microSD non e piu disponibile")); return false; }
  return sdFlussoScrivi(dati, n, s->errore, sizeof s->errore);
}

struct Rilettura {
  uint32_t off;
  size_t marchio;                               // quanto del marchio e gia stato visto
  bool trovato;
};

int leggiBlocco(void* ctx, uint8_t* buf, size_t max) {
  Rilettura* r = (Rilettura*)ctx;
  char err[SD_ERR_MAX];
  int n = sdReadAt(kFile, r->off, buf, max > SD_IO_MAX ? SD_IO_MAX : max, err, sizeof err);
  if (n <= 0) return n;
  r->off += (uint32_t)n;
  r->trovato = aggMarchio(buf, (size_t)n, r->marchio);
  return n;
}

// Si controlla quello che e davvero sulla microSD, non quello che e passato in rete.
bool fileGiusto(const AggDisponibile& d, uint8_t* buf, size_t cap, ErrBuf& err) {
  Rilettura r = {};
  char impronta[65];
  if (!sha256HexLetto(leggiBlocco, &r, buf, cap, impronta)) { err = tr("La microSD non si legge"); return false; }
  if (r.off != d.dim) { err = tr("Il file scaricato ha una dimensione sbagliata"); return false; }
  if (strcasecmp(impronta, d.sha)) { err = tr("L'impronta SHA-256 del file scaricato non corrisponde"); return false; }
  if (!r.trovato) { err = tr("Il file scaricato non e un firmware valido"); return false; }
  return true;
}

}  // namespace

void aggSdPulisci() {
  if (!sdUp() || sdFormatting()) return;
  char err[SD_ERR_MAX];
  sdRemove(kFile, err, sizeof err);             // se non c'e va bene lo stesso
}

AggSdEsito aggSdPrepara(const AggDisponibile& d, ErrBuf& err) {
  uint32_t totaleKb = 0, usatoKb = 0;
  if (!sdUp() || !sdSpaceKb(totaleKb, usatoKb)) return AGG_SD_ASSENTE;
  int pin[5];
  if (!ioSdPins(pin)) return AGG_SD_ASSENTE;
  if (!updPinSdOk(pin)) {                       // il recovery li rifiuterebbe: sarebbe un riavvio a vuoto a ogni controllo
    vlog("AGGIORNAMENTI: pin della microSD non adatti al recovery: si usa la rete");
    return AGG_SD_ASSENTE;
  }
  if (totaleKb < usatoKb || totaleKb - usatoKb < d.dim / 1024 + kMargineKb) return AGG_SD_PIENA;
  if (!d.dim || d.dim > drvFreeSketchSpace()) { err = tr("Il file scaricato ha una dimensione sbagliata"); return AGG_SD_ERRORE; }

  uint8_t* buf = (uint8_t*)drvMallocBig(SD_IO_MAX);
  if (!buf) { err = tr("Memoria insufficiente"); return AGG_SD_ERRORE; }

  char errore[SD_ERR_MAX];
  sdMakeDir(kCartella, errore, sizeof errore);  // se c'e gia va bene
  aggSdPulisci();
  if (!sdFlussoApri(kFile, errore, sizeof errore)) {
    err = errore;
    free(buf);
    return AGG_SD_ERRORE;
  }

  Scrittura s = {};
  s.primo = true;
  int stato = 0;
  DrvHttpsEsito e = drvHttpsBlocchi(d.url, buf, SD_IO_MAX, scriviBlocco, &s, d.dim, kTempoScarico, stato);
  bool chiuso = sdFlussoChiudi(errore, sizeof errore);
  if (e == HTTPS_E_RISPOSTA && stato >= 300 && stato < 400) {   // i rimandi li segue solo il recovery
    free(buf);
    aggSdPulisci();
    return AGG_SD_ASSENTE;
  }
  bool ok = false;
  if (e == HTTPS_E_SCRITTURA) err = s.errore;
  else if (e == HTTPS_E_TEMPO) err = tr("Scaricamento interrotto o troppo lento: si riprova");
  else if (e != HTTPS_OK) {
    vlog("AGGIORNAMENTI: scaricamento su microSD non riuscito (rete %d, HTTP %d)", (int)e, stato);
    err = tr("Scaricamento non riuscito: si riprova al prossimo controllo");
  } else if (!chiuso) err = errore;
  else ok = fileGiusto(d, buf, SD_IO_MAX, err);
  free(buf);
  if (!ok) aggSdPulisci();
  return ok ? AGG_SD_PRONTO : AGG_SD_ERRORE;
}

bool aggSdOrdine(const AggDisponibile& d, const AggRiserva& r, ErrBuf& err) {
  int p[5];
  if (!ioSdPins(p)) { err = tr("La microSD non e piu disponibile"); return false; }
  if (r.pronta && !updRiservaArma(aggFileRiserva(), r.sha, r.dim, d.ver, p, (uint32_t)p[4], err)) return false;
  bool ok = updOrderSd(kFile, d.sha, d.ver, d.dim, p, (uint32_t)p[4], err);
  if (!ok) updRiservaDisarma();
  return ok;
}

#else

void aggSdPulisci() {}
AggSdEsito aggSdPrepara(const AggDisponibile&, ErrBuf&) { return AGG_SD_ASSENTE; }
bool aggSdRiserva(AggRiserva& r, ErrBuf&) { r.pronta = false; return true; }
bool aggSdOrdine(const AggDisponibile&, const AggRiserva&, ErrBuf&) { return false; }

#endif
