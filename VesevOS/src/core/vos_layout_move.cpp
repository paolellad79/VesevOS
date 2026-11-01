// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_layout_move.cpp: passo B della struttura cartelle (vedi vos_layout.h).
//   moveAll()   marcatore 1 -> 2: copia verificata di /apps /scripts /backup e di audit.log, crash.txt, boots3.bin nei posti nuovi.
//               I vecchi file NON si toccano. Ripetibile: se manca la corrente si riparte, i file che nel posto nuovo ci sono gia si saltano.
//   removeOld() marcatore 2 -> 3: toglie i vecchi file (si chiama dall'avvio DOPO quello della copia, a avvio riuscito), ma solo quelli
//               che nel posto nuovo ci sono con lo stesso contenuto: un vecchio diverso resta, con un avviso, e il marcatore resta a 2.
//               Eccezione: audit.log, crash.txt, boots3.bin li aggiorna il firmware stesso di continuo, quindi dopo la copia il vecchio e
//               sempre diverso. Da marcatore 2 il firmware usa i percorsi nuovi: il vecchio si toglie appena il nuovo esiste (mai senza).
// Solo cartelle "piatte" (file senza sottocartelle): se ne trova una non sposta niente e lo dice. Il contenuto dei file non passa mai dal registro.
#include "vos_layout.h"
#include "vos_layout_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../drivers/vos_drv_fs.h"

namespace {

const size_t kBlock = 4096;             // un file occupa almeno un blocco della memoria interna
const size_t kMoveMargin = 16384;       // spazio da lasciare libero oltre ai file (i temporanei, le cartelle)
const int kMaxNames = 24;               // file raccolti in una cartella (il tetto delle cartelle e 20 voci)
const size_t kNameMax = 40;

struct Names {
  char name[kMaxNames][kNameMax];
  int n;
  bool hasSub;                          // una sottocartella, o un nome troppo lungo: non si sa spostare
  bool more;                            // ci sono piu file di quanti ne entrano: gli altri al giro dopo
  size_t bytes;                         // spazio stimato per copiarli (a blocchi)
};

bool endsWithTmp(const char* s) {
  size_t k = strlen(s);
  return k >= 5 && strcmp(s + k - 5, ".tmp~") == 0;
}

// Raccoglie i nomi dei file di dir (i temporanei ".tmp~" solo se withTmp: per copiare no, per togliere si). La cartella si chiude prima
// di toccare qualunque file. Con newDir i file che hanno gia il loro posto li' non contano nello spazio da copiare.
void collect(const char* dir, Names& out, bool withTmp = false, const char* newDir = nullptr) {
  memset(&out, 0, sizeof out);
  VosFile d = drvFsOpen(dir, "r");
  if (!d || !d.isDirectory()) return;
  for (VosFile c = d.openNextFile(); c; c = d.openNextFile()) {
    const char* nm = c.name();
    if (c.isDirectory()) { out.hasSub = true; continue; }
    if (!withTmp && endsWithTmp(nm)) continue;
    if (strlen(nm) >= kNameMax) { out.hasSub = true; continue; }
    if (out.n >= kMaxNames) { out.more = true; continue; }
    strcpy(out.name[out.n++], nm);
    if (newDir) {
      char np[LAY_PATH_BUF];
      if (snprintf(np, sizeof np, "%s/%s", newDir, nm) < (int)sizeof np && drvFsExists(np)) continue;   // gia copiato: niente spazio in piu
    }
    out.bytes += (c.size() + kBlock - 1) / kBlock * kBlock;
  }
}

bool joinPath(char* out, const char* dir, const char* name) {
  return snprintf(out, LAY_PATH_BUF, "%s/%s", dir, name) < LAY_PATH_BUF;
}

// Un file: se il nuovo c'e gia non si tocca mai (puo essere piu recente), altrimenti copia verificata.
bool moveOne(const char* from, const char* to) {
  if (drvFsExists(to)) return true;
  return layoutint::copyFile(from, to);
}

bool moveDir(LayoutItem it) {
  const char* from = layoutPathOld(it);
  const char* to = layoutPathNew(it);
  if (!drvFsExists(from)) return true;                    // niente da spostare
  if (!drvFsExists(to) && !drvFsMkdir(to)) return false;
  Names* nm = (Names*)malloc(sizeof(Names));
  if (!nm) return false;
  collect(from, *nm);
  bool ok = !nm->hasSub && !nm->more;
  for (int i = 0; ok && i < nm->n; i++) {
    char a[LAY_PATH_BUF], b[LAY_PATH_BUF];
    ok = joinPath(a, from, nm->name[i]) && joinPath(b, to, nm->name[i]) && moveOne(a, b);
  }
  free(nm);
  return ok;
}

}  // namespace

namespace layoutint {

const char* moveAll() {
  Names* nm = (Names*)malloc(sizeof(Names));
  if (!nm) return "ATTENZIONE: struttura cartelle: memoria insufficiente per spostare i file (si riprova al prossimo avvio)";
  // 1) controllo: niente sottocartelle, spazio sufficiente per le copie (i vecchi restano finche l'avvio dopo non riesce)
  size_t need = kMoveMargin;
  bool flat = true;
  for (int it = LAY_APPS; it <= LAY_BACKUP; it++) {
    collect(layoutPathOld((LayoutItem)it), *nm, false, layoutPathNew((LayoutItem)it));
    flat = flat && !nm->hasSub && !nm->more;
    need += nm->bytes + kBlock;                           // + la cartella nuova
  }
  free(nm);
  for (int it = LAY_AUDIT; it <= LAY_DIARY; it++) {
    VosFile f = drvFsOpen(layoutPathOld((LayoutItem)it), "r");
    if (f && !f.isDirectory() && !drvFsExists(layoutPathNew((LayoutItem)it))) need += (f.size() + kBlock - 1) / kBlock * kBlock;
  }
  if (!flat) return "ATTENZIONE: struttura cartelle: una cartella vecchia ha sottocartelle o troppi file, non si sposta (resta tutto com'e)";
  drvFsUsedRefresh(0);                                    // lo spazio usato ricordato puo essere vecchio: qui serve quello vero
  size_t total = drvFsTotal(), used = drvFsUsed();
  size_t freeB = total > used ? total - used : 0;
  if (freeB < need) return "ATTENZIONE: struttura cartelle: spazio insufficiente per copiare cartelle e registri nei posti nuovi (resta tutto com'e)";
  // 2) copia verificata
  bool ok = true;
  for (int it = LAY_APPS; it <= LAY_BACKUP && ok; it++) ok = moveDir((LayoutItem)it);
  for (int it = LAY_AUDIT; it <= LAY_DIARY && ok; it++) {
    const char* from = layoutPathOld((LayoutItem)it);
    if (drvFsExists(from)) ok = moveOne(from, layoutPathNew((LayoutItem)it));
  }
  if (!ok) return "ATTENZIONE: struttura cartelle: copia non riuscita (resta tutto com'e, si riprova al prossimo avvio)";
  stateSave(2);
  return "Struttura cartelle: copiati /apps /scripts /backup e i registri in /bin /sbin /var (i vecchi si tolgono al prossimo avvio riuscito)";
}

// Il vecchio file a si puo togliere? Un temporaneo ".tmp~" si'; gli altri solo se il nuovo b c'e ed e uguale (altrimenti diff = true, resta).
static bool removable(const char* a, const char* b, bool& diff) {   // solo per le cartelle (apps, scripts, backup)
  if (endsWithTmp(a)) return true;
  if (drvFsExists(b) && layoutint::sameContent(a, b)) return true;
  diff = true;
  return false;
}

const char* removeOld() {
  bool clean = true, diff = false;
  Names* nm = (Names*)malloc(sizeof(Names));
  if (!nm) return nullptr;                                // si riprova al prossimo avvio
  for (int it = LAY_APPS; it <= LAY_BACKUP; it++) {
    const char* from = layoutPathOld((LayoutItem)it);
    const char* to = layoutPathNew((LayoutItem)it);
    if (!drvFsExists(from)) continue;
    if (!drvFsExists(to)) { clean = false; continue; }    // la cartella nuova deve esserci: altrimenti non si tocca niente
    bool sub = false;
    for (int round = 0; round < 3; round++) {             // piu giri se i file sono piu di quanti ne entrano
      collect(from, *nm, true);
      sub = nm->hasSub;
      int kept = 0;                                       // file lasciati: al giro dopo li ritroverei uguali
      for (int i = 0; i < nm->n; i++) {
        char a[LAY_PATH_BUF], b[LAY_PATH_BUF];
        if (!joinPath(a, from, nm->name[i]) || !joinPath(b, to, nm->name[i])) { clean = false; continue; }
        if (!removable(a, b, diff)) { kept++; continue; }
        if (!drvFsRemove(a)) clean = false;
      }
      if (!nm->more || kept > 0) break;
    }
    if (!sub) drvFsRmdir(from);                           // fallisce da sola se dentro c'e ancora qualcosa (come i ".tmp~")
    if (drvFsExists(from)) clean = false;
  }
  free(nm);
  for (int it = LAY_AUDIT; it <= LAY_DIARY; it++) {
    const char* from = layoutPathOld((LayoutItem)it);
    if (!drvFsExists(from)) continue;
    if (!drvFsExists(layoutPathNew((LayoutItem)it))) { clean = false; continue; }   // senza copia nuova non si toglie niente
    if (!drvFsRemove(from)) clean = false;           // il contenuto non si confronta: il nuovo e quello in uso, il vecchio e fermo
  }
  if (diff) return "ATTENZIONE: struttura cartelle: alcuni vecchi file sono diversi dai nuovi, non sono stati tolti (il marcatore resta a 2, controllali a mano)";
  if (!clean) return "ATTENZIONE: struttura cartelle: non tutti i vecchi file sono stati tolti (si riprova al prossimo avvio)";
  stateSave(3);
  return "Struttura cartelle: vecchi file tolti (/apps /scripts /backup /audit.log /crash.txt /boots3.bin)";
}

}  // namespace layoutint
