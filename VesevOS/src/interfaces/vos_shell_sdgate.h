// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sdgate.h
// I comandi dei file (cd, ls, cat, mkdir, rm, mv, cp, write) lavorano anche sulla microSD, sotto /media/sd. Prima di toccarla controllano che sia montata
// e non occupata (formattazione), e contano l'operazione in corso: cosi "sd unmount" non la smonta a meta (sdInUse).
#pragma once
#include "../core/vos_common.h"
#include "../drivers/vos_drv_print.h"

// Tiene "in corso" un'operazione sulla microSD finche vive (anche nessuna: non fa niente senza microSD).
class SdHold {
 public:
  SdHold() = default;
  ~SdHold() { release(); }
  SdHold(const SdHold&) = delete;
  SdHold& operator=(const SdHold&) = delete;
  void take();
  void release();
 private:
  bool held_ = false;
};

// path: percorso pulito. Se non e sulla microSD ritorna true senza fare niente. Se lo e: scheda montata e libera = true (e, con hold, conta l'operazione);
// altrimenti scrive "<shown>: <motivo>" e ritorna false. shown e il testo digitato dall'utente.
bool shellSdReady(const char* path, const char* shown, OutSink& o, SdHold* hold = nullptr);
