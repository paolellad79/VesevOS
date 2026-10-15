// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sys.h
// STRATO 1 (driver): riavvio del chip. Lo chiama solo sysReboot()/wdReboot() (core/vos_wd), che prima salvano il motivo.
#pragma once

void drvSysRestart();
