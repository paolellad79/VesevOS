// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_io.cpp (vedi vos_sd.h)
// File della microSD a byte, a blocchi piccoli (lettura da un punto, scrittura, cancellazione, cartelle). Li usa Berry.
// I permessi (chi puo scrivere, quali file sono segreti) li decide chi chiama; qui si controlla il percorso e che la scheda sia montata.
// Niente Arduino: solo C, buffer con lunghezza e il driver vos_drv_sd.
#include "vos_sd.h"
#if VOS_WITH_SD
#include <stdio.h>
#include <string.h>
#include "vos_sd_buf.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_sd.h"

// Percorso valido e scheda montata? Per le scritture anche: mai la radice.
static bool pathReady(const char* path, bool notRoot, char* err, size_t en) {
  if (!sdPathOk(path, err, en)) {
    return false;
  }
  if (notRoot && !strcmp(path, "/")) {
    sdSetErr(err, en, tr("La radice della microSD non si tocca"));
    return false;
  }
  if (!drvSdUp()) {
    sdSetErr(err, en, sdFormatting() ? sdBusyText() : tr("La microSD non e montata"));
    return false;
  }

  return true;
}

int sdReadAt(const char* path, uint32_t off, uint8_t* buf, size_t n, char* err, size_t en) {
  if (!buf || n > SD_IO_MAX || !pathReady(path, false, err, en)) {
    return -1;
  }

  sdOpEnter();
  int count = drvSdReadAt(path, off, buf, n);
  sdOpLeave();

  if (count < 0) {
    sdSetErr(err, en, tr("File non trovato o non leggibile"));
  }

  return count;
}

bool sdWriteBytes(const char* path, const uint8_t* buf, size_t n, bool append, char* err, size_t en) {
  if ((n && !buf) || n > SD_IO_MAX || !pathReady(path, true, err, en)) {
    return false;
  }

  sdOpEnter();
  bool done = drvSdWrite(path, buf, n, append);
  sdOpLeave();

  if (!done) {
    sdSetErr(err, en, tr("Scrittura fallita (scheda protetta, piena o tolta?)"));
    return false;
  }

  vlog("SD: %s %s (%u byte)", append ? "aggiunto a" : "scritto", path, (unsigned)n);
  return true;
}

bool sdRemove(const char* path, char* err, size_t en) {
  if (!pathReady(path, true, err, en)) {
    return false;
  }

  sdOpEnter();
  bool done = drvSdRemove(path);
  sdOpLeave();

  if (!done) {
    sdSetErr(err, en, tr("Non riesco a cancellare (e un file?)"));
    return false;
  }

  vlog("SD: cancellato %s", path);
  return true;
}

bool sdMakeDir(const char* path, char* err, size_t en) {
  if (!pathReady(path, true, err, en)) {
    return false;
  }

  sdOpEnter();
  bool done = drvSdMkdir(path);
  sdOpLeave();

  if (!done) {
    sdSetErr(err, en, tr("Non riesco a creare la cartella"));
    return false;
  }

  vlog("SD: creata la cartella %s", path);
  return true;
}

#endif
