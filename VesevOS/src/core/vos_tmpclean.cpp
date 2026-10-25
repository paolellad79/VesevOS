// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tmpclean.cpp
// Il lavoro vero e fsTmpClean (vos_files.cpp). Qui c'e solo il task di fondo che lo lancia e il riepilogo nel registro.
#include "vos_tmpclean.h"
#include "vos_files.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "../drivers/vos_drv_task.h"

#define TMPCLEAN_DELAY_MS 30000   // mai nei primi 30 s dopo l'avvio
#define TMPCLEAN_STACK    4096    // il task vive pochi secondi: la RAM torna libera quando finisce

static void tmpCleanTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(TMPCLEAN_DELAY_MS));
  FsTmpStat st;
  fsTmpCleanKnown(st);
  if (st.removed) vlog("%s", trf("File temporanei .tmp~ rimasti da un'interruzione: %u cancellati", (unsigned)st.removed).c_str());
  if (st.orphan) vlog("%s", trf("File temporaneo .tmp~ senza il file definitivo, lasciato com'e: %s (%u in tutto)", st.firstOrphan, (unsigned)st.orphan).c_str());
  if (st.failed) vlog("%s", trf("Pulizia dei .tmp~: %u non cancellati per un errore", (unsigned)st.failed).c_str());
  if (st.more) vlog("%s", tr("Pulizia dei .tmp~: cartella con troppe voci, controllata solo in parte"));
  vlogl(LG_DBG, "tmpclean: %u voci guardate, stack minimo libero %u byte", (unsigned)st.scanned, (unsigned)uxTaskGetStackHighWaterMark(NULL));
  vTaskDelete(NULL);
}

void tmpCleanStart() {
  drvTaskCreate(tmpCleanTask, "tmpclean", TMPCLEAN_STACK, NULL, tskIDLE_PRIORITY, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 0 fisso)
}
