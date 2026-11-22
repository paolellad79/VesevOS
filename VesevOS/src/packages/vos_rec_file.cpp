// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec_file.cpp
#include "vos_rec_file.h"
#include "../core/vos_common.h"
#if VOS_WITH_REC
#include "../core/vos_common.h"
#include "../core/vos_files.h"
#include "../core/vos_files_stream.h"
#include "../core/vos_i18n.h"
#include "../core/vos_time.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_fs.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

const char* recDirOf(int where) { return where == 1 ? REC_DIR_SD : REC_DIR_FLASH; }

// Ogni file normale della cartella si mostra (anche non .csv), purche il nome entri nell'elenco
static bool nomeEntra(const char* n) {
  size_t l = strlen(n);
  return l > 0 && l < sizeof(RecFileInfo::name);
}

// Data dal nome "...-AAAAMMGG.estensione": epoca di mezzogiorno locale (la pagina la formatta), 0 se non c'e
static uint32_t giornoDelNome(const char* n) {
  const char* dot = strrchr(n, '.');
  size_t l = dot ? (size_t)(dot - n) : strlen(n);
  if (l < 9 || n[l - 9] != '-') return 0;
  const char* d = n + l - 8;
  for (int i = 0; i < 8; i++) if (d[i] < '0' || d[i] > '9') return 0;
  struct tm tmv = {};
  tmv.tm_year = (d[0] - '0') * 1000 + (d[1] - '0') * 100 + (d[2] - '0') * 10 + (d[3] - '0') - 1900;
  tmv.tm_mon = (d[4] - '0') * 10 + (d[5] - '0') - 1;
  tmv.tm_mday = (d[6] - '0') * 10 + (d[7] - '0');
  tmv.tm_hour = 12;
  tmv.tm_isdst = -1;
  if (tmv.tm_year < 124 || tmv.tm_mon < 0 || tmv.tm_mon > 11 || tmv.tm_mday < 1 || tmv.tm_mday > 31) return 0;
  time_t t = mktime(&tmv);
  return t > 1700000000 ? (uint32_t)t : 0;
}

bool recNomeValido(const char* nome) {
  size_t l = nome ? strlen(nome) : 0;
  if (l < 1 || l > REC_NOME_MAX) return false;
  for (size_t i = 0; i < l; i++) {
    char c = nome[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

// Il file e del registratore con questo nome? Solo le quattro forme esatte: un file "rec-2-20261119.csv" non e di "rec".
// *ev dice se e un file delle azioni.
static bool isOwn(const char* file, const char* nome, bool* ev) {
  size_t nl = strlen(nome);
  if (strncmp(file, nome, nl) != 0) return false;
  const char* r = file + nl;
  bool e = false;
  if (strncmp(r, "-eventi", 7) == 0) { e = true; r += 7; }
  if (strcmp(r, ".csv") != 0) {
    if (*r != '-') return false;
    r++;
    for (int i = 0; i < 8; i++) if (r[i] < '0' || r[i] > '9') return false;
    if (strcmp(r + 8, ".csv") != 0) return false;
  }
  if (ev) *ev = e;
  return true;
}

static bool sdReady() {
#if VOS_WITH_SD
  return sdUp();
#else
  return false;
#endif
}

// Elenco ordinato per nome (le date si ordinano da sole). Ritorna quanti file ci sono (anche con altro nome o altra estensione: solo i "own" contano nel limite e si cancellano da soli).
int recFileList(int where, const char* nome, RecFileInfo* out, int max, uint32_t* totalBytes) {
  if (totalBytes) *totalBytes = 0;
  const char* dir = recDirOf(where);
  if (where == 1 && !sdReady()) return 0;
  if (!drvFsExists(dir)) return 0;
  VosFile d = drvFsOpen(dir, "r");
  if (!d || !d.isDirectory()) return 0;
  int n = 0, seen = 0;
  for (VosFile e = d.openNextFile(); e && seen < REC_FILES_MAX; e = d.openNextFile(), seen++) {
    if (e.isDirectory() || !nomeEntra(e.name())) continue;
    if (n >= max) continue;
    snprintf(out[n].name, sizeof out[n].name, "%s", e.name());
    out[n].size = (uint32_t)e.size();
    out[n].ev = false;
    out[n].day = giornoDelNome(out[n].name);
    out[n].own = isOwn(out[n].name, nome, &out[n].ev);
    if (!out[n].own) out[n].ev = false;
    if (totalBytes && out[n].own) *totalBytes += out[n].size;
    n++;
  }
  for (int i = 1; i < n; i++) {                              // ordinamento semplice: sono poche voci
    RecFileInfo t = out[i];
    int j = i - 1;
    while (j >= 0 && strcmp(out[j].name, t.name) > 0) { out[j + 1] = out[j]; j--; }
    out[j + 1] = t;
  }
  return n;
}

static void joinPath(char* out, size_t cap, const char* dir, const char* name) { snprintf(out, cap, "%s/%s", dir, name); }

// Toglie i file piu vecchi finche c'e posto per "need" byte sotto il tetto e sotto il numero di voci. keep = il file da scrivere (non si toglie se e l'ultimo).
static void makeRoom(int where, const char* nome, int maxKb, int limite, size_t need, const char* keep) {
  RecFileInfo L[REC_FILES_MAX];
  for (int giro = 0; giro < REC_FILES_MAX; giro++) {
    uint32_t total = 0;
    int n = recFileList(where, nome, L, REC_FILES_MAX, &total);
    bool tooBig = limite == 0 && (uint64_t)total + need > (uint64_t)maxKb * 1024ULL;   // limite 1 (per singolo file): il totale non conta
    bool tooMany = where == 0 && n >= REC_FILES_MAX - 4;
    if (!tooBig && !tooMany) return;
    int victim = -1;
    for (int i = 0; i < n; i++) if (L[i].own && strcmp(L[i].name, keep) != 0) { victim = i; break; }   // i file con un altro nome non si toccano
    if (victim < 0) return;                                  // resta solo il file di oggi: si lascia crescere fino al tetto (sotto controlla maxKb)
    char p[FS_PATH_MAX + 1];
    joinPath(p, sizeof p, recDirOf(where), L[victim].name);
    if (!drvFsRemove(p)) return;
  }
}

bool recFileAppend(int where, const char* nome, int maxKb, int limite, int tipo, bool oraValida, const char* header, bool* needHeader, const char* text, size_t n, ErrBuf& err) {
  const char* dir = recDirOf(where);
  if (where == 1 && !sdReady()) { err = tr("microSD non montata"); return false; }
  if (!recNomeValido(nome)) { err = tr("Nome del file non valido"); return false; }          // mai un percorso: il file resta nella cartella
  char name[48];
  const char* mezzo = tipo == REC_TIPO_EVENTI ? "-eventi" : "";
  if (oraValida) {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char d[12];
    strftime(d, sizeof d, "%Y%m%d", &tmv);
    snprintf(name, sizeof name, "%s%s-%s.csv", nome, mezzo, d);
  } else {
    snprintf(name, sizeof name, "%s%s.csv", nome, mezzo);
  }
  size_t hl = (needHeader && *needHeader) ? strlen(header) : 0;
  size_t soglia = where == 1 ? 1048576UL : 65536UL;
  if (fsFreeAt(dir) < soglia + n + hl + 64) { err = tr("Poco spazio libero: il registratore non scrive"); return false; }
  if (!drvFsExists(dir)) {
    if (!fsMkdirs(dir, err)) return false;
  }
  char path[FS_PATH_MAX + 1];
  joinPath(path, sizeof path, dir, name);
  makeRoom(where, nome, maxKb, limite, n + hl + 64, name);
  VosFile f = drvFsOpen(path, "a");
  if (!f) { err = tr("Impossibile scrivere il file del registratore"); return false; }
  size_t cur = f.size();
  if ((uint64_t)cur + n + hl > (uint64_t)maxKb * 1024ULL) {   // il file di oggi da solo ha riempito lo spazio (o il tetto del singolo file): si riparte da capo
    f.close();
    if (!drvFsRemove(path)) { err = tr("Impossibile scrivere il file del registratore"); return false; }
    f = drvFsOpen(path, "a");
    if (!f) { err = tr("Impossibile scrivere il file del registratore"); return false; }
    cur = 0;
  }
  if (cur == 0 && needHeader) *needHeader = true;
  if (needHeader && *needHeader) {
    if (f.write((const uint8_t*)header, strlen(header)) != strlen(header)) { err = tr("Scrittura non riuscita"); return false; }
  }
  if (f.write((const uint8_t*)text, n) != n) { err = tr("Scrittura non riuscita"); return false; }
  f.close();
  if (needHeader) *needHeader = false;
  return true;
}

bool recFileClear(const char* nome, ErrBuf& err) {
  bool ok = true;
  for (int where = 0; where < 2; where++) {
    RecFileInfo L[REC_FILES_MAX];
    int n = recFileList(where, nome, L, REC_FILES_MAX, nullptr);
    for (int i = 0; i < n; i++) {
      if (!L[i].own) continue;                               // un file con un altro nome non si cancella
      char p[FS_PATH_MAX + 1];
      joinPath(p, sizeof p, recDirOf(where), L[i].name);
      if (!drvFsRemove(p)) ok = false;
    }
  }
  if (!ok) err = tr("Alcuni file non si sono cancellati");
  return ok;
}
#endif
