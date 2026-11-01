// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vde_inst.cpp (vedi vos_vde.h): file dei driver .vde in /devs, installazione sui pin scelti dall'utente, rimozione, elenco JSON.
#include "vos_vde.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_elf_run.h"
#include "vos_hw.h"
#include "vos_drvcmd.h"
#include "vos_drvwidget.h"
#include "../devices/vos_drvres.h"
#include "../devices/vos_io.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_sd.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"
#include "../core/vos_ext.h"
#include "../drivers/vos_drv_fs.h"

namespace {

void percorso(char* out, size_t cap, const char* nome) { snprintf(out, cap, "%s/%s%s", VDE_DIR, nome, VDE_EXT); }

// Legge il file per intero in un blocco. nullptr = non c'e, vuoto o troppo grande.
uint8_t* leggiFile(const char* nome, size_t& len) {
  char p[64]; percorso(p, sizeof p, nome);
  VosFile f = drvFsOpen(p);
  if (!f || f.isDirectory()) return nullptr;
  len = f.size();
  if (len == 0 || len > VDE_TEXT_MAX + 64 + 262144) return nullptr;
  uint8_t* b = (uint8_t*)malloc(len);
  if (!b) return nullptr;
  if (f.read(b, len) != len) { free(b); return nullptr; }
  return b;
}

// "21,22" -> numeri; ritorna quanti ne ha letti (0 se un valore non e un numero di pin).
int leggiPin(const char* csv, int* out, int mx) {
  int n = 0;
  const char* p = csv;
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    if (!*p) break;
    char* e = nullptr; long g = strtol(p, &e, 10);
    if (e == p || g < 0 || g > 48 || n >= mx) return 0;
    out[n++] = (int)g;
    p = e;
  }
  return n;
}

// La periferica sd (e una sola): il suo nome, o false se non c'e.
bool nomeSd(char* out, size_t cap) {
  for (int i = 0; i < ioCount(); i++) {
    const char* n = ioNameAt(i);
    const char* c = ioClassName(n);
    if (c && !strcmp(c, "sd")) { snprintf(out, cap, "%s", n); return true; }
  }
  return false;
}

// C'e gia un driver Berry con questo nome? Un nome vale per un solo driver, .vdb o .vde.
bool esisteBerry(const char* nome) {
  char p[64];
  snprintf(p, sizeof p, "%s/%s%s", VDE_DIR, nome, VOS_EXT_DRV_B);
  if (drvFsExists(p)) return true;
  snprintf(p, sizeof p, "%s/%s%s", VDE_DIR, nome, MOD_EXT_OLD);
  return drvFsExists(p);
}

// Dice chi occupa un pin: "GPIO12 e gia usato da sd1" o "GPIO19 e riservato dal sistema". Vuoto se e libero.
bool pinOccupatoDa(int gpio, ErrBuf& err) {
  for (int i = 0; i < pinCount(); i++) {
    const PinInfo* pi = pinAt(i);
    if (pi->gpio != gpio) continue;
    err.setf(pi->fixed ? tr("GPIO%d e riservato dal sistema (%s)") : tr("GPIO%d e gia usato da %s"), gpio, pi->owner);
    return true;
  }
  return false;
}

int segnali(uint8_t bus, int nsig) { return bus == VB_I2C ? 2 : (bus == VB_SPI || bus == VB_SD) ? 4 : nsig; }

}  // namespace

bool vdeEsiste(const char* nome) {
  char p[64]; percorso(p, sizeof p, nome);
  return utilNameOk(nome, strlen(nome)) && drvFsExists(p);
}

bool vdeLeggi(const char* nome, VdeInfo& v, uint8_t** elf, ErrBuf& err) {
  if (!utilNameOk(nome, strlen(nome))) { err = tr("Nome non valido"); return false; }
  size_t len = 0;
  uint8_t* f = leggiFile(nome, len);
  if (!f) { err = tr("File non trovato"); return false; }
  bool ok = vdeParse(f, len, nome, v, elf, err);
  free(f);
  return ok;
}

bool vdeSalva(const char* nome, const uint8_t* file, size_t len, ErrBuf& err) {
  if (!utilNameOk(nome, strlen(nome))) { err = tr("Nome non valido"); return false; }
  if (esisteBerry(nome)) { err = tr("Esiste gia un driver con questo nome"); return false; }
  VdeInfo* v = new VdeInfo();
  bool ok = vdeParse(file, len, nome, *v, nullptr, err);
  delete v;
  if (!ok) return false;
  char p[64], t[72]; percorso(p, sizeof p, nome); snprintf(t, sizeof t, "%s.tmp~", p);
  VosFile f = drvFsOpen(t, "w");
  if (!f) { err = tr("Impossibile scrivere"); return false; }
  size_t w = f.write(file, len);
  f.close();
  if (w != len || (drvFsExists(p) && !drvFsRemove(p)) || !drvFsRename(t, p)) { drvFsRemove(t); err = tr("Impossibile scrivere"); return false; }
  vdeRinfresca();
  return true;
}

int vdeElenco(char (*nomi)[MOD_NAME_BUF], int mx) {
  VosFile dir = drvFsOpen(VDE_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  int n = 0;
  size_t el = strlen(VDE_EXT);
  for (VosFile f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const char* nm = f.name();
    const char* slash = strrchr(nm, '/'); if (slash) nm = slash + 1;
    size_t l = strlen(nm);
    if (f.isDirectory() || l <= el || strcmp(nm + l - el, VDE_EXT) || l - el >= MOD_NAME_BUF || !utilNameOk(nm, l - el)) continue;
    if (n < mx) { memcpy(nomi[n], nm, l - el); nomi[n][l - el] = 0; }
    n++;
  }
  return n;
}

bool vdeInstallato(const char* nome) {
  DrvRes r;
  if (drvresFind(nome, r)) return true;
  VdeInfo* v = new VdeInfo(); ErrBuf e;
  bool sd = vdeLeggi(nome, *v, nullptr, e) && v->bus == VB_SD;
  delete v;
  int pin[5];
  return sd && ioSdPins(pin);                                // microSD: la periferica sd esiste (e indipendente dal driver)
}

int vdePinDati(const char* nome, int* pin, int mx) {
  DrvRes r;
  if (drvresFind(nome, r)) {
    int n = r.npin < mx ? r.npin : mx;
    for (int i = 0; i < n; i++) pin[i] = (int)r.pin[i];
    return n;
  }
  VdeInfo* v = new VdeInfo(); ErrBuf e;
  bool sd = vdeLeggi(nome, *v, nullptr, e) && v->bus == VB_SD;
  delete v;
  int p[5];
  if (!sd || mx < 4 || !ioSdPins(p)) return 0;
  for (int i = 0; i < 4; i++) pin[i] = p[i];                     // CS, MOSI, SCK, MISO
  return 4;
}

bool vdeInstalla(const char* nome, const char* pinsCsv, ErrBuf& err) {
  VdeInfo* v = new VdeInfo();
  bool ok = vdeLeggi(nome, *v, nullptr, err);
  if (!ok) { delete v; return false; }
  if (esisteBerry(nome)) { err = tr("Esiste gia un driver con questo nome"); delete v; return false; }
  char gia[16];
  if (v->bus == VB_SD && nomeSd(gia, sizeof gia)) { err.setf(tr("La microSD e gia configurata come %s: toglila prima (pagina Periferiche) o dai altri pin a quel driver"), gia); delete v; return false; }
  int pin[VDE_SIG_MAX] = {0}, n = segnali(v->bus, v->nsig);
  if (pinsCsv && pinsCsv[0]) {
    if (leggiPin(pinsCsv, pin, VDE_SIG_MAX) != n) { err.setf(tr("Servono %d numeri di pin, separati da virgola"), n); delete v; return false; }
  } else {
    int prop[VDE_SIG_MAX] = {0};                              // i pin proposti dal firmware (liberi e non vietati); li applica questo comando perche l'utente l'ha lanciato
    uint8_t cls = v->bus == VB_I2C ? IC_I2C : v->bus == VB_SD ? IC_SD : IC_PIN;
    if (ioSuggest(cls, n, prop) < n) { err = tr("Non ci sono abbastanza pin liberi"); delete v; return false; }
    memcpy(pin, prop, sizeof pin);
  }
  for (int i = 0; i < n; i++) if (pinOccupatoDa(pin[i], err)) { delete v; return false; }   // dice quale pin e occupato e da chi
  if (v->bus == VB_SD) {
    char nomeIo[16], msg[IO_ERR_MAX] = {0};
    ok = ioAddSd(pin[0], pin[1], pin[2], pin[3], v->khz, "", nomeIo, msg);
    if (!ok) err = msg;
#if VOS_WITH_SD
    else sdAutoStart();                                         // da ora la scheda si monta da sola
#endif
  } else {
    ok = drvresAdd(nome, v->bus == VB_I2C ? DRB_I2C : v->bus == VB_SPI ? DRB_SPI : DRB_GPIO, pin, n, v->addr, v->khz, err);
  }
  delete v;
  if (ok) { vlog("VDE: %s installato", nome); vdeRinfresca(); }
  return ok;
}

bool vdeRimuovi(const char* nome, bool conFile, ErrBuf& err) {
  if (!utilNameOk(nome, strlen(nome))) { err = tr("Nome non valido"); return false; }
  vos_elf_ferma(nome);                                        // se sta girando un comando del driver
  hwRilascia(nome);                                           // chiude il bus e rimette in ingresso le uscite, prima di perdere i pin
  DrvRes r;
  if (drvresFind(nome, r) && !drvresRemove(nome, err)) return false;
  VdeInfo* v = new VdeInfo(); ErrBuf e0;
  bool sd = vdeLeggi(nome, *v, nullptr, e0) && v->bus == VB_SD;
  delete v;
  char io[16];
  if (sd && nomeSd(io, sizeof io) && !ioRemove(io, err)) return false;   // la periferica sd: si smonta la scheda e si liberano i 4 pin
  if (conFile) {
    char p[64]; percorso(p, sizeof p, nome);
    if (drvFsExists(p) && !drvFsRemove(p)) { err = tr("Impossibile scrivere"); return false; }
  }
  vdeRinfresca();
  vlog("VDE: %s tolto", nome);
  return true;
}

// Da' altri pin a un driver installato: lo toglie (senza il file) e lo reinstalla; se i nuovi pin non vanno rimette i vecchi.
bool vdeCambiaPin(const char* nome, const char* pinsCsv, ErrBuf& err) {
  if (!vdeEsiste(nome)) { err = tr("File non trovato"); return false; }
  int vecchi[VDE_SIG_MAX] = {0};
  int nv = vdePinDati(nome, vecchi, VDE_SIG_MAX);
  char csv[24] = "";
  for (int i = 0; i < nv; i++) { char t[8]; snprintf(t, sizeof t, "%s%d", i ? "," : "", vecchi[i]); strncat(csv, t, sizeof csv - strlen(csv) - 1); }
  if (!vdeRimuovi(nome, false, err)) return false;
  if (vdeInstalla(nome, pinsCsv, err)) return true;
  ErrBuf e2;
  if (nv > 0) vdeInstalla(nome, csv, e2);                     // rimette i pin di prima
  return false;
}

void vdeRinfresca() { drvCmdRefresh(); drvWidgetRefresh(); }

bool vdeElencoJson(HeapBuf& out) {
  char (*nomi)[MOD_NAME_BUF] = new char[VDE_LIST_MAX][MOD_NAME_BUF];
  int tot = vdeElenco(nomi, VDE_LIST_MAX), n = tot < VDE_LIST_MAX ? tot : VDE_LIST_MAX;
  VdeInfo* v = new VdeInfo();
  bool ok = out.add("{\"ok\":true,\"list\":[");
  for (int i = 0; ok && i < n; i++) {
    ErrBuf e;
    bool buono = vdeLeggi(nomi[i], *v, nullptr, e);
    if (buono && esisteBerry(nomi[i])) { buono = false; e = tr("Esiste gia un driver con questo nome"); }   // un nome vale per un solo driver
    if (i) ok = out.add(',');
    ok = ok && heapAddf(out, "{\"n\":\"%s\",\"t\":\"", nomi[i]);
    ok = ok && out.addJson(buono ? v->title : nomi[i]);
    ok = ok && heapAddf(out, "\",\"ok\":%d,\"e\":\"", buono ? 1 : 0);
    ok = ok && out.addJson(e.c_str());
    ok = ok && heapAddf(out, "\",\"b\":\"%s\",\"i\":%d,\"s\":[", buono ? (v->bus == VB_I2C ? "i2c" : v->bus == VB_SPI ? "spi" : v->bus == VB_SD ? "sd" : "gpio") : "", vdeInstallato(nomi[i]) ? 1 : 0);
    for (int k = 0; ok && buono && k < v->nsig; k++) ok = heapAddf(out, "%s\"%s\"", k ? "," : "", v->sig[k]);
    ok = ok && out.add("],\"sug\":[");                         // i pin liberi che propone il firmware (solo se il driver non e ancora installato)
    if (buono && !vdeInstallato(nomi[i])) {
      int prop[VDE_SIG_MAX] = {0};
      uint8_t cls = v->bus == VB_I2C ? IC_I2C : v->bus == VB_SD ? IC_SD : IC_PIN;
      int got = ioSuggest(cls, segnali(v->bus, v->nsig), prop);
      for (int k = 0; ok && k < got && k < VDE_SIG_MAX; k++) ok = heapAddf(out, "%s%d", k ? "," : "", prop[k]);
    }
    ok = ok && out.add("],\"p\":[");
    int dati[VDE_SIG_MAX], nd = vdePinDati(nomi[i], dati, VDE_SIG_MAX);
    for (int k = 0; ok && k < nd; k++) ok = heapAddf(out, "%s%d", k ? "," : "", dati[k]);
    ok = ok && heapAddf(out, "],\"by\":%u,\"pw\":\"%s\",\"w\":\"", buono ? (unsigned)v->elfLen : 0u, buono ? v->power : "-");
    ok = ok && out.addJson(buono ? v->warn : "");
    ok = ok && out.add("\",\"ic\":\"");                        // icona del driver (solo se installato): la pagina Pin la mette sul blocco
    ok = ok && out.addJson(buono && vdeInstallato(nomi[i]) ? v->icon : "");
    ok = ok && out.add("\"}");
  }
  ok = ok && out.add("]}");
  delete v;
  delete[] nomi;
  return ok && !(out.oom() || out.over());
}
#endif
