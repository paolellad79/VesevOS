// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_layout.cpp (vedi vos_layout.h). Nel file c'e la configurazione con le password: qui non se ne stampa mai il contenuto.
#include "vos_layout.h"
#include <stdio.h>
#include <string.h>
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_nvs.h"
#include "vos_layout_int.h"

namespace {

const char* const kDirs[] = {"/etc", "/var", "/tmp", "/home", "/bin", "/sbin"};
const char NVS_NS[] = "vos";
const char NVS_KEY[] = "layout";       // 0 = niente, 1 = configurazione copiata, 2 = tutto copiato, 3 = vecchi file tolti
const size_t kMargin = 8192;           // spazio da lasciare sempre libero (come le scritture a pezzi dei file)
const size_t kChunk = 256;

// Voce per voce: dove stava (vecchio) e dove sta ora (nuovo). Stesso ordine di LayoutItem.
const char* const kOld[LAY_COUNT] = {"/apps", "/scripts", "/backup", "/audit.log", "/crash.txt", "/boots3.bin"};
const char* const kNew[LAY_COUNT] = {"/bin", "/sbin", "/var/backup", "/var/audit.log", "/var/crash.txt", "/var/boots3.bin"};

int g_state = 0;                       // marcatore letto all'avvio (e aggiornato quando cambia)
int g_startState = 0;                  // marcatore com'era all'inizio di QUESTO avvio: i vecchi file si tolgono solo se era gia 2

int stateLoad() {
  DrvNvs p;
  if (!p.begin(NVS_NS, true)) return 0;
  int v = p.getUChar(NVS_KEY, 0);
  p.end();
  return v > 3 ? 3 : v;
}

}  // namespace

namespace layoutint {

void stateSave(int v) {
  DrvNvs p;
  if (p.begin(NVS_NS, false)) { p.putUChar(NVS_KEY, (uint8_t)v); p.end(); g_state = v; }
}

// Due file hanno lo stesso contenuto, byte per byte?
bool sameContent(const char* a, const char* b) {
  VosFile fa = drvFsOpen(a, "r");
  VosFile fb = drvFsOpen(b, "r");
  if (!fa || !fb || fa.size() != fb.size()) return false;
  uint8_t x[kChunk], y[kChunk];
  for (;;) {
    size_t n = fa.read(x, sizeof x);
    if (n == 0) return true;
    if (fb.read(y, n) != n || memcmp(x, y, n) != 0) return false;
  }
}

// Copia a pezzi (da from a "to.tmp~", poi rename). Il contenuto non passa mai dal registro.
bool copyFile(const char* from, const char* to) {
  char tmp[LAY_PATH_BUF];
  if (snprintf(tmp, sizeof tmp, "%s.tmp~", to) >= (int)sizeof tmp) return false;
  VosFile in = drvFsOpen(from, "r");
  VosFile out = drvFsOpen(tmp, "w");
  if (!in || !out) { out.close(); drvFsRemove(tmp); return false; }
  uint8_t buf[kChunk];
  size_t total = 0, n;
  bool ok = true;
  while (ok && (n = in.read(buf, sizeof buf)) > 0) {
    ok = out.write(buf, n) == n;
    total += n;
  }
  size_t want = in.size();
  in.close();
  out.close();
  if (!ok || total != want || !sameContent(from, tmp) || !drvFsRename(tmp, to)) { drvFsRemove(tmp); return false; }
  return true;
}

}  // namespace layoutint

void layoutEnsureDirs() {
  for (const char* d : kDirs) if (!drvFsExists(d)) drvFsMkdir(d);   // direttamente dal driver: sono cartelle di sistema, senza il tetto delle voci della radice
}

bool layoutWriteAtomic(const char* path, const uint8_t* data, size_t n) {
  char tmp[LAY_PATH_BUF];
  if (snprintf(tmp, sizeof tmp, "%s.tmp~", path) >= (int)sizeof tmp) return false;
  VosFile f = drvFsOpen(tmp, "w");
  if (!f) return false;
  size_t w = f.write(data, n);
  f.close();
  VosFile chk = drvFsOpen(tmp, "r");
  bool sizeOk = w == n && chk && chk.size() == n;
  chk.close();
  if (!sizeOk || !drvFsRename(tmp, path)) { drvFsRemove(tmp); return false; }
  return true;
}

const char* layoutPathNew(LayoutItem it) { return it < LAY_COUNT ? kNew[it] : ""; }
const char* layoutPathOld(LayoutItem it) { return it < LAY_COUNT ? kOld[it] : ""; }
int layoutState() { return g_state; }
const char* layoutPath(LayoutItem it) { return g_state >= 2 ? layoutPathNew(it) : layoutPathOld(it); }

namespace {

// Tempo 1: copia /vesevos.conf in /etc (marcatore 0 -> 1).
const char* confStep() {
  if (!drvFsExists(VOS_CONF_OLD) || drvFsExists(VOS_CONF_FILE)) {   // scheda nuova (la configurazione nascera in /etc) oppure /etc c'e gia, piu recente
    layoutint::stateSave(1);
    return nullptr;
  }
  VosFile src = drvFsOpen(VOS_CONF_OLD, "r");
  size_t need = src.size();
  src.close();
  drvFsUsedRefresh(0);                                   // lo spazio usato ricordato e 0 prima del primo calcolo: qui serve quello vero (una volta sola)
  size_t total = drvFsTotal(), used = drvFsUsed();
  size_t freeB = total > used ? total - used : 0;
  if (need == 0 || freeB < need + kMargin) {
    return "ATTENZIONE: struttura cartelle: spazio insufficiente per copiare la configurazione in /etc (la lettura resta dal vecchio posto)";
  }
  if (!layoutint::copyFile(VOS_CONF_OLD, VOS_CONF_FILE)) {
    return "ATTENZIONE: struttura cartelle: copia della configurazione in /etc non riuscita (si riprova al prossimo avvio)";
  }
  layoutint::stateSave(1);
  return "Struttura cartelle: configurazione copiata in /etc (il file vecchio resta)";
}

}  // namespace

const char* layoutInit() {
  if (!drvFsTotal()) return nullptr;                     // memoria interna non montata: non si tocca niente (ne cartelle ne marcatore)
  layoutEnsureDirs();
  g_state = stateLoad();
  g_startState = g_state;
  const char* msg = nullptr;
  if (g_state == 0) msg = confStep();
  if (g_state == 1) {                                    // passo B: il resto (si ripete a ogni avvio finche non e riuscito)
    const char* m = layoutint::moveAll();
    if (m) msg = m;
  }
  if (g_state >= 2 && !drvFsExists(kNew[LAY_BACKUP])) drvFsMkdir(kNew[LAY_BACKUP]);
  return msg;
}

const char* layoutFinish() {
  if (g_startState != 2 || g_state != 2) return nullptr;
  g_startState = 3;                                      // una volta sola per avvio
  return layoutint::removeOld();
}
