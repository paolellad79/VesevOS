// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_where.h
// STRATO 6 (packages): la parte di vos_prog_data.h che deve conoscere le app e i programmi in corso: se un'app salva dati, la scelta flash/microSD
// (non si cambia con l'app in corso) e lo STOP delle app quando la microSD sparisce. Sta a parte perche vos_prog_data.h e chiamato da vos_app, vos_prog
// e dal motore Berry: se conoscesse anche loro sarebbe un ciclo tra moduli.
#pragma once
#include <stdint.h>
#include "vos_prog_kind.h"
#include "../core/vos_errbuf.h"

bool  progSaves(uint8_t kind, const char* name);                              // l'app dichiara che salva dati (Berry: data=1; ELF: importa vos_data_*)
bool  progWhereSet(uint8_t kind, const char* name, int where, ErrBuf& err);   // la scelta dell'utente (la pagina la offre solo ad Admin); non con l'app in corso
void  progDataStart();                                                        // una volta all'avvio: se la microSD sparisce, le app con i dati li' si fermano
