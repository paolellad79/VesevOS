// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_data.cpp (vedi vos_prog_data.h)
#include "vos_prog_data.h"
#include <stdio.h>
#include <string.h>
#include "../core/vos_config.h"
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_fs.h"
#include "../devices/vos_sd.h"

namespace {

const char kFlashBase[] = "/var/lib";            // flash: /var/lib/<nome>
const char kSdBase[] = "/media/sd/app";          // microSD: /media/sd/app/<nome>

bool kindOk(uint8_t kind) { return kind == PK_APP_B || kind == PK_APP_E; }

bool nomeOk(const char* n) {
  size_t l = n ? strlen(n) : 0;
  if (l < 1 || l > VOS_BOOTPROG_NAME) return false;
  for (size_t i = 0; i < l; i++) {
    char c = n[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

// Nome di un file di dati: 1-40 caratteri a-z A-Z 0-9 _ . - , non comincia ne finisce con il punto (FAT li taglia), niente ".." ne barre.
bool fileOk(const char* f) {
  size_t l = f ? strlen(f) : 0;
  if (l < 1 || l > PD_FILE_MAX || f[0] == '.' || f[l - 1] == '.') return false;
  for (size_t i = 0; i < l; i++) {
    char c = f[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
    if (!ok || (c == '.' && f[i + 1] == '.')) return false;
  }
  return true;
}

int slotSd(uint8_t kind, const char* name) {
  for (int i = 0; i < cfg.dataSdN; i++) if (cfg.dataSd[i].kind == kind && !strcmp(cfg.dataSd[i].name, name)) return i;
  return -1;
}

// Cartella dei dati (senza crearla). false se il nome non e valido.
bool dirPath(uint8_t kind, const char* name, char* out, size_t cap) {
  if (!kindOk(kind) || !nomeOk(name)) return false;
  int n = snprintf(out, cap, "%s/%s", progWhere(kind, name) == PW_SD ? kSdBase : kFlashBase, name);
  return n > 0 && (size_t)n < cap;
}

// Crea la cartella e quella che la contiene (un livello alla volta: la microSD e la flash non passano da fsMkdirs, che non entra in /media).
bool makeDirs(const char* dir) {
  char parent[FS_PATH_MAX + 1];
  snprintf(parent, sizeof parent, "%s", dir);
  char* ls = strrchr(parent, '/');
  if (ls && ls != parent) { *ls = 0; if (!drvFsExists(parent) && !drvFsMkdir(parent)) return false; }
  return drvFsExists(dir) || drvFsMkdir(dir);
}

// Percorso del file di dati; tiene conto della scheda (deve essere utilizzabile se la scelta e SD) e conta l'operazione come "in corso".
struct Operazione {
  char path[FS_PATH_MAX + 1];
  bool sd = false, held = false;
  ~Operazione() {
#if VOS_WITH_SD
    if (held) sdOpLeave();
#endif
  }
  int apri(uint8_t kind, const char* name, const char* file) {
    if (!kindOk(kind) || !nomeOk(name)) return PD_E_ARG;
    if (!fileOk(file)) return PD_E_PERCORSO;
    char dir[FS_PATH_MAX + 1];
    if (!dirPath(kind, name, dir, sizeof dir)) return PD_E_ARG;
    sd = progWhere(kind, name) == PW_SD;
    if (sd) {
      if (!progSdUsable()) return PD_E_NOSD;
#if VOS_WITH_SD
      sdOpEnter();
      held = true;
#endif
    }
    if (snprintf(path, sizeof path, "%s/%s", dir, file) >= (int)sizeof path) return PD_E_PERCORSO;
    return 0;
  }
};

}  // namespace

bool progSdUsable() {
#if VOS_WITH_SD
  return sdUp() && !sdFormatting();
#else
  return false;
#endif
}

int progWhere(uint8_t kind, const char* name) {
  return kindOk(kind) && name && slotSd(kind, name) >= 0 ? PW_SD : PW_FLASH;
}

bool progWhereStore(uint8_t kind, const char* name, int where, ErrBuf& err) {
  err = "";
  if (!kindOk(kind) || !nomeOk(name)) { err = tr("Nome non valido"); return false; }
  if (where != PW_FLASH && where != PW_SD) { err = tr("Scelta non valida: flash oppure sd"); return false; }
  int s = slotSd(kind, name);
  if (where == PW_SD && s < 0) {
    if (!progSdUsable()) { err = tr("microSD non montata: i dati restano sulla flash"); return false; }
    if (cfg.dataSdN >= VOS_BOOTPROG_MAX) { err.setf(tr("Massimo %d app sulla microSD"), VOS_BOOTPROG_MAX); return false; }
    cfg.dataSd[cfg.dataSdN].kind = kind;
    snprintf(cfg.dataSd[cfg.dataSdN].name, sizeof cfg.dataSd[0].name, "%s", name);
    cfg.dataSdN++;
  } else if (where == PW_FLASH && s >= 0) {
    for (int i = s; i + 1 < cfg.dataSdN; i++) cfg.dataSd[i] = cfg.dataSd[i + 1];
    cfg.dataSdN--;
  } else {
    return true;                                              // gia cosi: niente da salvare
  }
  cfgSetOrigin("dati app");
  cfgSave();
  return true;
}

bool progDataReady(uint8_t kind, const char* name, ErrBuf& err) {
  if (!kindOk(kind) || progWhere(kind, name) != PW_SD || progSdUsable()) return true;
  err = tr("La microSD scelta per i dati dell'app non e montata");
  return false;
}

bool progDataDir(uint8_t kind, const char* name, char* out, size_t cap, ErrBuf& err) {
  err = "";
  if (!dirPath(kind, name, out, cap)) { err = tr("Nome non valido"); return false; }
  if (progWhere(kind, name) == PW_SD && !progSdUsable()) { err = tr("microSD non montata"); return false; }
  if (!makeDirs(out)) { err = tr("Impossibile creare la cartella dei dati"); return false; }
  return true;
}

int progDataWrite(uint8_t kind, const char* name, const char* file, const uint8_t* buf, size_t n, bool append) {
  if (!buf && n) return PD_E_ARG;
  if (n > PD_IO_MAX) return PD_E_GRANDE;
  Operazione op;
  int e = op.apri(kind, name, file);
  if (e) return e;
  char dir[FS_PATH_MAX + 1];
  ErrBuf er;
  if (!progDataDir(kind, name, dir, sizeof dir, er)) return op.sd && !progSdUsable() ? PD_E_NOSD : PD_E_IO;
  if (n + 8192 > fsFreeAt(op.path)) return PD_E_PIENO;
  VosFile f = drvFsOpen(op.path, append ? "a" : "w");
  if (!f) return PD_E_IO;
  size_t w = n ? f.write(buf, n) : 0;
  f.close();
  return w == n ? (int)n : PD_E_PIENO;
}

int progDataRead(uint8_t kind, const char* name, const char* file, uint32_t off, uint8_t* buf, size_t n) {
  if (!buf || n == 0) return PD_E_ARG;
  if (n > PD_IO_MAX) return PD_E_GRANDE;
  Operazione op;
  int e = op.apri(kind, name, file);
  if (e) return e;
  VosFile f = drvFsOpen(op.path, "r");
  if (!f || f.isDirectory()) return PD_E_NONTROVATO;
  if (off >= f.size()) return 0;
  if (off && !f.seek(off)) return PD_E_IO;
  return (int)f.read(buf, n);
}

int progDataRemove(uint8_t kind, const char* name, const char* file) {
  Operazione op;
  int e = op.apri(kind, name, file);
  if (e) return e;
  if (!drvFsExists(op.path)) return PD_E_NONTROVATO;
  return drvFsRemove(op.path) ? 0 : PD_E_IO;
}

bool progDataJson(HeapBuf& out) {
  out.addf("{\"sd\":%s,\"list\":[", progSdUsable() ? "true" : "false");
  for (int i = 0; i < cfg.dataSdN; i++) {
    out.addf("%s{\"k\":\"%c\",\"n\":\"", i ? "," : "", cfg.dataSd[i].kind == PK_APP_B ? 'b' : 'e');
    out.addJson(cfg.dataSd[i].name);
    out.add("\"}");
  }
  out.add("]}");
  return !(out.oom() || out.over());
}
