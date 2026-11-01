// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_progext.cpp (vedi vos_progext.h)
#include "vos_progext.h"
#include <stdio.h>
#include <string.h>
#include "../core/vos_ext.h"
#include "../core/vos_files.h"
#include "../drivers/vos_drv_fs.h"

const ProgExt PEXT_SCRIPT = { VOS_EXT_SCRIPT, VOS_EXT_OLD_SCRIPT };
const ProgExt PEXT_APP_B  = { VOS_EXT_APP_B,  VOS_EXT_OLD_APP_B };
const ProgExt PEXT_APP_E  = { VOS_EXT_APP_E,  VOS_EXT_OLD_APP_E };

static bool finisce(std::string_view s, const char* ext) {
  size_t e = strlen(ext);
  return s.size() >= e && s.substr(s.size() - e) == ext;
}

size_t progExtLen(std::string_view name, const ProgExt& e) {
  if (finisce(name, e.nuovo)) return strlen(e.nuovo);
  if (finisce(name, e.vecchio)) return strlen(e.vecchio);
  return 0;
}

bool progHaExt(const char* name, const ProgExt& e) { return progExtLen(name ? name : "", e) != 0; }

size_t progTogliExt(char* out, size_t n, const ProgExt& e) {
  size_t k = progExtLen(std::string_view(out, n), e);
  out[n - k] = 0;
  return n - k;
}

void progPercorso(char* out, size_t cap, const char* dir, const char* nome, const ProgExt& e) {
  snprintf(out, cap, "%s/%s%s", dir, nome, e.nuovo);
  if (drvFsExists(out)) return;
  char old[96];
  snprintf(old, sizeof old, "%s/%s%s", dir, nome, e.vecchio);
  if (drvFsExists(old)) snprintf(out, cap, "%s", old);
}

void progPercorsoAvvio(char* out, size_t cap, const char* dir, const char* nome, const ProgExt& e) {
  progPercorso(out, cap, dir, nome, e);
  if (drvFsExists(out)) return;
  char sdDir[48], sd[96];
  snprintf(sdDir, sizeof sdDir, "/media/sd%s", dir);
  progPercorso(sd, sizeof sd, sdDir, nome, e);
  if (drvFsExists(sd)) snprintf(out, cap, "%s", sd);
}

bool progToglieVecchio(const char* dir, const char* nome, const ProgExt& e) {
  char old[96];
  snprintf(old, sizeof old, "%s/%s%s", dir, nome, e.vecchio);
  if (!drvFsExists(old)) return false;
  ErrBuf er;
  return fsRemove(old, er);
}

bool progVecchioCoperto(const char* dir, std::string_view nome, const ProgExt& e) {
  char nuovo[96];
  snprintf(nuovo, sizeof nuovo, "%s/%.*s%s", dir, (int)nome.size(), nome.data(), e.nuovo);
  return drvFsExists(nuovo);
}
