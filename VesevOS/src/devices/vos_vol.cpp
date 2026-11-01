// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vol.cpp (vedi vos_vol.h)
#include "vos_vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_sd.h"          // vuoto con VOS_WITH_SD = 0
#include "../core/vos_common.h"
#include "../core/vos_files.h"
#include "../core/vos_paths.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_sd.h"
#include "../drivers/vos_drv_task.h"

namespace {

// Questa voce si puo mostrare a questo ruolo? dir e la cartella che la contiene (percorso pulito).
bool visibleTo(const char* dir, const char* name, int role) {
  char full[FS_PATH_MAX + 1 + VOL_NAME_MAX + 1];
  const char* sep = (dir[0] == '/' && dir[1] == 0) ? "" : "/";
  int n = snprintf(full, sizeof full, "%s%s%s", dir, sep, name);
  if (n <= 0 || (size_t)n >= sizeof full) return false;
  if (pathIsSystemFile(full)) return false;
  return role == ROLE_ADMIN || !pathIsSecret(full);
}

// Il percorso stesso e nascosto a questo ruolo?
bool hiddenFrom(const char* path, int role) {
  return pathIsSystemFile(path) || (role != ROLE_ADMIN && pathIsSecret(path));
}

void setEntry(VolEntry& e, const char* name, bool dir, uint32_t size, uint32_t mtime) {
  snprintf(e.name, sizeof e.name, "%s", name);
  e.dir = dir;
  e.size = dir ? 0 : size;
  e.mtime = mtime;
}

const char* lastPart(const char* path) {
  const char* sl = strrchr(path, '/');
  return sl ? sl + 1 : path;
}

bool cardMounted() {
#if VOS_WITH_SD
  return sdUp();
#else
  return false;
#endif
}

VolErr listMediaRoot(int start, VolEntry* out, int& count, int& consumed) {
  if (start == 0 && cardMounted()) {                              // la sola voce che c'e: la microSD montata
    setEntry(out[0], "sd", true, 0, 0);
    count = 1;
    consumed = 1;
  }
  return VOL_OK;
}

#if VOS_WITH_SD
VolErr listSd(const char* path, const char* rel, int role, int start, VolEntry* out, int max, int& count, int& consumed, bool& more) {
  if (!sdUp()) return VOL_NO_MEDIA;
  char ignored[8];
  if (strcmp(rel, "/") != 0 && !sdPathOk(rel, ignored, sizeof ignored)) return VOL_BAD_PATH;
  DrvSdEntry* raw = (DrvSdEntry*)malloc(sizeof(DrvSdEntry) * (size_t)max);
  if (!raw) return VOL_NO_MEMORY;
  int got = drvSdListFrom(rel, start, raw, max, more);
  if (got < 0) {
    free(raw);
    return sdUp() ? VOL_NOT_FOUND : VOL_NO_MEDIA;          // cartella che non c'e, o scheda tolta proprio adesso
  }
  consumed = got;
  for (int i = 0; i < got; i++) {
    if (strlen(raw[i].name) > VOL_NAME_MAX || !visibleTo(path, raw[i].name, role)) continue;
    setEntry(out[count++], raw[i].name, raw[i].dir, raw[i].size, raw[i].mtime);
  }
  free(raw);
  return VOL_OK;
}
#endif

VolErr listInternal(const char* path, int start, VolEntry* out, int max, int role, int& count, int& consumed, bool& more) {
  VosFile d = drvFsOpen(path);
  if (!d) return VOL_NOT_FOUND;
  if (!d.isDirectory()) return VOL_NOT_DIR;
  int seen = 0;
  for (VosFile f = d.openNextFile(); f; f = d.openNextFile()) {
    drvTaskPause();                                        // una cartella piena non deve affamare il task idle (Task watchdog)
    if (seen < start) { seen++; continue; }
    if (count >= max) { more = true; break; }
    seen++;
    const char* name = lastPart(f.name());
    if (strlen(name) > VOL_NAME_MAX || !visibleTo(path, name, role)) continue;
    setEntry(out[count++], name, f.isDirectory(), (uint32_t)f.size(), 0);
  }
  consumed = seen - start;
  return VOL_OK;
}

}  // namespace

VolErr volList(const char* path, int role, int start, VolEntry* out, int max, int& count, int& consumed, bool& more) {
  count = 0;
  consumed = 0;
  more = false;
  if (!path || !out || max < 1 || max > VOL_PAGE_MAX || start < 0 || start > VOL_START_MAX) return VOL_BAD_ARG;
  if (pathIsMediaRoot(path)) return listMediaRoot(start, out, count, consumed);
  if (pathIsMedia(path)) {
    const char* rel = pathSdRel(path);
    if (!rel) return VOL_NOT_FOUND;                        // "/media/altro": non c'e niente
    if (hiddenFrom(path, role)) return VOL_NOT_FOUND;
#if VOS_WITH_SD
    return listSd(path, rel, role, start, out, max, count, consumed, more);
#else
    return VOL_NO_MEDIA;
#endif
  }
  if (hiddenFrom(path, role)) return VOL_NOT_FOUND;
  return listInternal(path, start, out, max, role, count, consumed, more);
}

VolErr volStat(const char* path, int role, VolEntry& out) {
  if (!path) return VOL_BAD_ARG;
  if (!strcmp(path, "/")) { setEntry(out, "/", true, 0, 0); return VOL_OK; }
  if (pathIsMediaRoot(path)) { setEntry(out, "media", true, 0, 0); return VOL_OK; }
  const char* name = lastPart(path);
  if (strlen(name) > VOL_NAME_MAX) return VOL_TOO_LONG;
  if (pathIsMedia(path)) {
    const char* rel = pathSdRel(path);
    if (!rel || hiddenFrom(path, role)) return VOL_NOT_FOUND;
#if VOS_WITH_SD
    if (!sdUp()) return VOL_NO_MEDIA;
    if (!strcmp(rel, "/")) { setEntry(out, "sd", true, 0, 0); return VOL_OK; }
    char ignored[8];
    if (!sdPathOk(rel, ignored, sizeof ignored)) return VOL_BAD_PATH;
    DrvSdEntry e;
    if (!drvSdStat(rel, e)) return sdUp() ? VOL_NOT_FOUND : VOL_NO_MEDIA;
    setEntry(out, name, e.dir, e.size, e.mtime);
    return VOL_OK;
#else
    return VOL_NO_MEDIA;
#endif
  }
  if (hiddenFrom(path, role)) return VOL_NOT_FOUND;
  VosFile f = drvFsOpen(path);
  if (!f) return VOL_NOT_FOUND;
  setEntry(out, name, f.isDirectory(), (uint32_t)f.size(), 0);
  return VOL_OK;
}
