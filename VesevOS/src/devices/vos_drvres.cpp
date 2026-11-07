// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvres.cpp (vedi vos_drvres.h).
#include "vos_drvres.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vos_pins.h"
#include "vos_io.h"
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_lock.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"
#include "../drivers/vos_drv_fs.h"

#define CFG_FILE "/drvres.cfg"

namespace {

DrvRes g_res[DRVRES_MAX];
int    g_n = 0;
VosMutex& mx() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

int cerca(const char* name) {
  for (int i = 0; i < g_n; i++) if (!strcmp(g_res[i].name, name)) return i;
  return -1;
}

int pinsAttesi(uint8_t bus, int npin) {
  if (bus == DRB_I2C) return 2;
  if (bus == DRB_SPI) return 4;
  return npin >= 1 && npin <= DRVRES_PINS_MAX ? npin : 0;
}

// Prende i pin nel registro; se uno non si puo prendere, rende quelli gia presi.
bool prendi(const DrvRes& r) {
  for (int i = 0; i < r.npin; i++) {
    if (!pinClaim((uint8_t)r.pin[i], r.name, drvresBusName(r.bus))) {
      for (int k = 0; k < i; k++) pinRelease((uint8_t)r.pin[k]);
      return false;
    }
  }
  return true;
}

void rendi(const DrvRes& r) { for (int i = 0; i < r.npin; i++) pinRelease((uint8_t)r.pin[i]); }

// Scrive /drvres.cfg: "nome bus npin p0 p1 p2 p3 addr khz" per riga. Dentro il blocco.
bool salva(ErrBuf& err, int salta = -1) {
  HeapBuf out(DRVRES_MAX * 80 + 16);
  for (int i = 0; i < g_n; i++) {
    if (i == salta) continue;
    const DrvRes& r = g_res[i];
    char riga[192];
    snprintf(riga, sizeof riga, "%s %u %u %d %d %d %d %u %u\n", r.name, (unsigned)r.bus, (unsigned)r.npin,
             r.pin[0], r.pin[1], r.pin[2], r.pin[3], (unsigned)r.addr, (unsigned)r.khz);
    out.add(riga);
  }
  if (out.oom()) { err = tr("Memoria insufficiente"); return false; }
  if (out.length() == 0) {                                  // niente da tenere: il file si toglie
    if (drvFsExists(CFG_FILE) && !drvFsRemove(CFG_FILE)) { err = tr("Impossibile scrivere"); return false; }
    return true;
  }
  return fsWriteText(CFG_FILE, std::string_view(out.c_str(), out.length()), err);
}

}  // namespace

const char* drvresBusName(uint8_t bus) { return bus == DRB_I2C ? "i2c" : bus == DRB_SPI ? "spi" : "gpio"; }

int drvresCount() { VosLock L(mx()); return g_n; }

bool drvresGet(int i, DrvRes& out) {
  VosLock L(mx());
  if (i < 0 || i >= g_n) return false;
  out = g_res[i];
  return true;
}

bool drvresFind(const char* name, DrvRes& out) {
  if (!name) return false;
  VosLock L(mx());
  int i = cerca(name);
  if (i < 0) return false;
  out = g_res[i];
  return true;
}

void drvresInit() {
  VosLock L(mx());
  g_n = 0;
  HeapBuf text(fsTextMax()); ErrBuf e;
  if (!drvFsExists(CFG_FILE) || !fsReadText(CFG_FILE, text, e)) return;
  LineIter it(std::string_view(text.c_str(), text.length())); std::string_view raw;
  while (it.next(raw) && g_n < DRVRES_MAX) {
    char riga[96];
    if (!viewCopy(riga, sizeof riga, trimView(raw))) continue;
    DrvRes r; memset(&r, 0, sizeof r);
    unsigned bus, npin, addr, khz; int p[4];
    if (sscanf(riga, "%24s %u %u %d %d %d %d %u %u", r.name, &bus, &npin, &p[0], &p[1], &p[2], &p[3], &addr, &khz) != 9) continue;
    if (bus < DRB_GPIO || bus > DRB_SPI || npin != (unsigned)pinsAttesi((uint8_t)bus, (int)npin) || !utilNameOk(r.name, strlen(r.name))) continue;
    if (bus == DRB_I2C && (addr < 0x08 || addr > 0x77 || (khz != 100 && khz != 400))) continue;       // righe rovinate: stessi limiti di drvresAdd
    if (bus == DRB_SPI && (khz < 100 || khz > 10000)) continue;
    r.bus = (uint8_t)bus; r.npin = (uint8_t)npin; r.addr = (uint8_t)addr; r.khz = (uint16_t)khz;
    for (int i = 0; i < 4; i++) r.pin[i] = (int8_t)p[i];
    if (cerca(r.name) >= 0 || !prendi(r)) { vlog("DRVRES: %s non caricato (nome doppio o pin occupato)", r.name); continue; }
    g_res[g_n++] = r;
  }
}

bool drvresAdd(const char* name, uint8_t bus, const int* pins, int npin, int addr, int khz, ErrBuf& err) {
  if (!name || !utilNameOk(name, strlen(name)) || strlen(name) > 24) { err = tr("Nome non valido"); return false; }
  if (bus < DRB_GPIO || bus > DRB_SPI || !pins || pinsAttesi(bus, npin) == 0 || npin != pinsAttesi(bus, npin)) { err = tr("Numero di pin non adatto al bus"); return false; }
  if (bus == DRB_I2C && (addr < 0x08 || addr > 0x77 || (khz != 100 && khz != 400))) { err = tr("I2C: indirizzo da 0x08 a 0x77, velocita 100 o 400 kHz"); return false; }
  if (bus == DRB_SPI && (khz < 100 || khz > 10000)) { err = tr("SPI: velocita da 100 a 10000 kHz"); return false; }
  DrvRes r; memset(&r, 0, sizeof r);
  snprintf(r.name, sizeof r.name, "%s", name);
  r.bus = bus; r.npin = (uint8_t)npin; r.addr = (uint8_t)(bus == DRB_I2C ? addr : 0); r.khz = (uint16_t)(bus == DRB_GPIO ? 0 : khz);
  for (int i = 0; i < npin; i++) {
    for (int k = 0; k < i; k++) if (pins[k] == pins[i]) { err = tr("Pin ripetuto"); return false; }
    const char* why = pinTestBlock(pins[i]);                 // il controllo unico: vietati, di avvio, USB, gia usati
    if (why) { err = why; return false; }
    r.pin[i] = (int8_t)pins[i];
  }
  VosLock L(mx());
  if (ioHas(name) || cerca(name) >= 0) { err = tr("Nome gia usato"); return false; }
  if (g_n >= DRVRES_MAX) { err = tr("Troppi driver con pin (massimo 4)"); return false; }
  if (bus == DRB_I2C && ioHasI2c()) { err = tr("Il bus I2C e gia usato dal sensore delle periferiche"); return false; }
  if (!prendi(r)) { err = tr("Pin gia usato"); return false; }
  g_res[g_n++] = r;
  if (!salva(err)) { g_n--; rendi(r); return false; }
  vlog("DRVRES: %s assegnato (%s, %d pin)", name, drvresBusName(bus), npin);
  return true;
}

bool drvresRemove(const char* name, ErrBuf& err) {
  VosLock L(mx());
  int i = name ? cerca(name) : -1;
  if (i < 0) { err = tr("Il driver non ha pin assegnati"); return false; }
  if (!salva(err, i)) return false;
  rendi(g_res[i]);
  for (int k = i; k < g_n - 1; k++) g_res[k] = g_res[k + 1];
  g_n--;
  vlog("DRVRES: %s tolto", name);
  return true;
}
#endif
