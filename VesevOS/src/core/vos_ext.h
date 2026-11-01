// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ext.h
// UN SOLO PUNTO per le estensioni dei file di VesevOS. Schema deciso il 31/10/2026: v + tipo (s script, a app, d driver) + linguaggio
// (b Berry, e ELF). Dal 31/10/2026 valgono i nomi nuovi (.vsb, .vab, .vae): si scrivono solo quelli. I nomi vecchi (.vsc, .vap, .vel)
// si leggono ancora (VOS_EXT_OLD_*): i file gia installati continuano a funzionare. Se esistono tutti e due vale il nuovo.
// Gli aiuti per cercare, elencare e togliere il vecchio omonimo stanno in packages/vos_progext.h.
#pragma once
#define VOS_EXT_SCRIPT   ".vsb"    // script Berry, in /sbin
#define VOS_EXT_APP_B    ".vab"    // app Berry, in /bin
#define VOS_EXT_APP_E    ".vae"    // app ELF (codice nativo), in /bin
#define VOS_EXT_OLD_SCRIPT ".vsc"  // vecchio nome degli script: si legge ancora
#define VOS_EXT_OLD_APP_B  ".vap"  // vecchio nome delle app Berry: si legge ancora
#define VOS_EXT_OLD_APP_E  ".vel"  // vecchio nome delle app ELF: si legge ancora
#define VOS_EXT_DRV_B    ".vdb"    // driver Berry, in /devs
#define VOS_EXT_DRV_E    ".vde"    // driver ELF, in /devs
#define VOS_EXT_BACKUP   ".vbk"    // backup cifrato della configurazione, in /backup
