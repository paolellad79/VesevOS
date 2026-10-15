// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sys.cpp
#include "vos_drv_sys.h"
#include <Arduino.h>

void drvSysRestart() { ESP.restart(); }
