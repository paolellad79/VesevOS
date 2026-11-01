// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_auto.cpp: la microSD configurata si monta DA SOLA appena la scheda risponde (all'avvio e a ogni inserimento) e passa a
// "non rilevata" quando la scheda viene tolta. Un task a priorita bassa chiama sdAutoStep() (vos_sd.cpp) ogni 2,5 secondi.
// Il modulo a 6 pin (VCC, GND, CS, MOSI, SCK, MISO) non ha un contatto che dica "scheda inserita": la scheda si "sente" solo provando a
// parlarle. Un pin di rilevamento (CD) per i moduli che lo hanno e una miglioria futura. La durata di un tentativo senza scheda e quella del
// driver (drivers/vos_drv_sd): NON misurata su scheda. Se la scheda e stata espulsa a mano (sdUnmount) o il formato non e supportato, il
// sondaggio sta fermo fino a un "monta" o al riavvio: il blocco NON si salva, al riavvio la scheda presente si rimonta sempre.
#include "vos_sd.h"

#if VOS_WITH_SD
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"

#define SDAUTO_STACK    6144    // il montaggio FAT gira in questo task
#define SDAUTO_PRIO     1       // la piu bassa: web, shell e rete non aspettano mai il sondaggio
#define SDAUTO_FIRST_MS 4000    // dopo l'avvio: rete e memoria interna sono gia pronte, l'avvio non rallenta
#define SDAUTO_EVERY_MS 2500

static SdLostFn g_lost = nullptr;

void sdSetLostHook(SdLostFn fn) { g_lost = fn; }

static void sdAutoTask(void*) {
  bool wasUp = false;

  drvDelayMs(SDAUTO_FIRST_MS);
  for (;;) {
    sdSnapshotStep();
    sdAutoStep();
    bool up = sdUp();
    if (wasUp && !up && g_lost) {
      g_lost();                                // chi usava la scheda (le app con i dati sulla microSD) lo viene a sapere
    }
    wasUp = up;
    drvDelayMs(SDAUTO_EVERY_MS);
  }
}

void sdAutoStart() {
  static bool started = false;

  if (started) {
    return;
  }
  started = drvTaskCreate(sdAutoTask, "sdauto", SDAUTO_STACK, nullptr, SDAUTO_PRIO, nullptr, DRV_CORE_ANY);
}
#endif
