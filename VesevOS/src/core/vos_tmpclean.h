// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tmpclean.h
// Dopo un taglio di corrente a meta di un salvataggio resta "nome.tmp~" accanto a "nome" (e occupa una voce del tetto di cartella).
// Questo avvia, una volta sola, la pulizia dei temporanei (regole in vos_files.h, fsTmpClean).
#pragma once

// Da chiamare in setup(), a file system e log pronti. Parte un task di fondo a priorita da idle che aspetta 30 s (Wi-Fi, NTP e HTTPS si
// assestano), pulisce le poche cartelle note, scrive un riepilogo nel registro solo se ha trovato qualcosa, e finisce (libera la sua RAM).
void tmpCleanStart();
