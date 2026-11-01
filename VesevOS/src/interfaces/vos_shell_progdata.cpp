// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_progdata.cpp
// "app data <nome> [flash|sd]" e "vesevos-elf data <nome> [flash|sd]": dove un'app salva i suoi dati (decisione dell'utente: lo sceglie lui, come dalla
// pagina App). Senza il secondo argomento mostra la scelta e la cartella; con flash o sd la cambia (solo Admin, solo per le app che salvano dati,
// non con l'app in corso). Il lavoro vero e in packages/vos_prog_data.h e vos_prog_where.h.
#include "vos_shell_int.h"
#include "../packages/vos_prog_data.h"
#include "../packages/vos_prog_where.h"

void shx::shProgData(uint8_t kind, const char* name, const char* where, OutSink& o) {
  ErrBuf err;
  if (!where || !where[0]) {
    if (!progSaves(kind, name)) { o.println(trf("%s: %s", name, tr("L'app non salva dati"))); return; }
    char dir[FS_PATH_MAX + 1];
    bool sd = progWhere(kind, name) == PW_SD;
    snprintf(dir, sizeof dir, "%s/%s", sd ? "/media/sd/app" : "/var/lib", name);
    o.println(trf("%s: dati su %s (%s)", name, sd ? "microSD" : "flash", dir));
    return;
  }
  int w = !strcmp(where, "sd") ? PW_SD : !strcmp(where, "flash") ? PW_FLASH : -1;
  if (w < 0) { o.println(tr("Scelta non valida: flash oppure sd")); return; }
  o.println(progWhereSet(kind, name, w, err) ? tr("Fatto") : err.c_str());
}
