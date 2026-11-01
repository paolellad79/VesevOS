// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_path.cpp (vedi vos_sd.h)
// Controllo dei percorsi della microSD. Le usano le primitive dei file (vos_sd_io.cpp, vos_vol.cpp) e quelle di Berry.
// Niente Arduino: solo C.
#include "vos_sd.h"
#if VOS_WITH_SD
#include <stdio.h>
#include <string.h>
#include "vos_sd_buf.h"
#include "../core/vos_paths.h"
#include "../core/vos_i18n.h"

// ---- Controllo dei percorsi ----
// La regola dei nomi sta in core/vos_paths (pathSdRelOk): la stessa per i comandi dei file, la pagina, Berry ed ELF.

bool sdPathOk(const char* path, char* err, size_t en) {
  bool ok = pathSdRelOk(path);

  if (!ok) {
    sdSetErr(err, en, tr("Percorso non valido: parte da / e usa solo lettere, numeri, spazio e _ - . ( )"));
  }

  return ok;
}
#endif
