// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_layout_int.h: parti comuni a vos_layout.cpp e vos_layout_move.cpp (non usare altrove).
#pragma once
#include <stddef.h>

#define LAY_PATH_BUF 112     // spazio per un percorso con il suffisso ".tmp~" (i percorsi puliti sono al massimo 100 caratteri)

namespace layoutint {
void stateSave(int v);                                  // scrive il marcatore nella NVS e lo ricorda
bool sameContent(const char* a, const char* b);         // stesso contenuto, byte per byte
bool copyFile(const char* from, const char* to);        // copia verificata (to.tmp~ poi rename)
const char* moveAll();                                  // marcatore 1 -> 2 (vos_layout_move.cpp); riga per il registro o nullptr
const char* removeOld();                                // marcatore 2 -> 3 (vos_layout_move.cpp); riga per il registro o nullptr
}
