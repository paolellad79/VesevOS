// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim_files.cpp
// STRATO 6 (packages): gruppo "files" del registro delle primitive: la CARTELLA DEI DATI dell'app (ELF: vos_data_dir, vos_data_write, vos_data_read,
// vos_data_rm; Berry: data_dir, data_write, data_read, data_rm, vedi vos_berry_data.cpp). Il lavoro vero sta in vos_prog_data.cpp: dove stanno i
// dati (flash o microSD) lo sceglie l'utente dalla pagina App, l'app non lo sa e non lo decide. L'app tocca SOLO la sua cartella, con nomi di
// file semplici (nessun sotto-percorso). Un driver (.vde) non e un'app: non ha cartella dei dati.
#include "vos_prim.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <string.h>
#include "vos_prog_data.h"
#include "../core/vos_files.h"

namespace {

#define FILE_BUF (PD_FILE_MAX + 1)

// Il nome dell'app che chiama; vuoto = non e un'app (driver, shell, pagina).
const char* chiHaChiamato() { return vos_prim_esec_nome(); }

int primDataDir(char* out, int out_max) {
  if (!out || out_max < 1) return VOS_PRIM_E_ARG;
  out[0] = 0;
  const char* nome = chiHaChiamato();
  if (!nome[0]) return VOS_PRIM_E_VIETATO;
  char dir[FS_PATH_MAX + 1];
  ErrBuf e;
  if (!progDataDir(PK_APP_E, nome, dir, sizeof dir, e)) return progWhere(PK_APP_E, nome) == PW_SD ? VOS_PRIM_E_NOSD : VOS_PRIM_E_IO;
  int n = snprintf(out, (size_t)out_max, "%s", dir);
  if (n < 0 || n >= out_max) { out[0] = 0; return VOS_PRIM_E_GRANDE; }
  return n;
}

int primDataWrite(const char* file, const void* dati, int n, int append) {
  char f[FILE_BUF];
  if (!vos_prim_copia_testo(file, f, sizeof f)) return VOS_PRIM_E_ARG;
  if (n < 0 || (n > 0 && !dati)) return VOS_PRIM_E_ARG;
  const char* nome = chiHaChiamato();
  if (!nome[0]) return VOS_PRIM_E_VIETATO;
  return progDataWrite(PK_APP_E, nome, f, (const uint8_t*)dati, (size_t)n, append != 0);
}

int primDataRead(const char* file, int offset, void* buf, int n) {
  char f[FILE_BUF];
  if (!vos_prim_copia_testo(file, f, sizeof f)) return VOS_PRIM_E_ARG;
  if (offset < 0 || n < 1 || !buf) return VOS_PRIM_E_ARG;
  const char* nome = chiHaChiamato();
  if (!nome[0]) return VOS_PRIM_E_VIETATO;
  return progDataRead(PK_APP_E, nome, f, (uint32_t)offset, (uint8_t*)buf, (size_t)n);
}

int primDataRm(const char* file) {
  char f[FILE_BUF];
  if (!vos_prim_copia_testo(file, f, sizeof f)) return VOS_PRIM_E_ARG;
  const char* nome = chiHaChiamato();
  if (!nome[0]) return VOS_PRIM_E_VIETATO;
  return progDataRemove(PK_APP_E, nome, f);
}

const vos_prim_t kFiles[] = {
  { "data_dir",   (const void*)primDataDir,   "int vos_data_dir(char *out, int out_max)", ROLE_OPER, 4 },
  { "data_write", (const void*)primDataWrite, "int vos_data_write(const char *file, const void *dati, int n, int append)", ROLE_OPER, 4 },
  { "data_read",  (const void*)primDataRead,  "int vos_data_read(const char *file, int offset, void *buf, int n)", ROLE_OPER, 4 },
  { "data_rm",    (const void*)primDataRm,    "int vos_data_rm(const char *file)", ROLE_OPER, 4 },
};

}  // namespace

void vos_prim_files_registra() {
  vos_prim_registra("files", kFiles, sizeof(kFiles) / sizeof(kFiles[0]));
}
#endif
