// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board.h
// Sceglie il file di scheda. Di fabbrica: ESP32-S3 SuperMini (nessun flag da dare al compilatore).
// Altra scheda: -DVOS_BOARD_FILE="vos_board_altra.h"  (il file sta in questa cartella).
// Solo caratteri ASCII.
#pragma once
#ifdef VOS_BOARD_FILE
#include VOS_BOARD_FILE
#else
#include "vos_board_s3_supermini.h"
#endif
