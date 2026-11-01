// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble_types.h: costanti e tipi del Bluetooth dei programmi, comuni a vos_ble.h e a vos_ble_int.h (cosi i due non si includono a vicenda).
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_common.h"
#include "../drivers/vos_drv_ble.h"

#define BLE_OWNER_MAX    24        // lunghezza massima del nome del programma proprietario
#define BLE_MINUTES_MAX  60        // tetto di ble_on(minuti)
#define BLE_STATUS_MAX   160

#if VOS_WITH_BLE
struct BleAppEvent {               // evento dal telefono, da consegnare al programma
  uint8_t kind;                    // 1 = scrittura, 2 = lettura
  uint8_t idx;                     // caratteristica
  uint8_t len;
  uint8_t data[BLE_VALUE_MAX];
};
#define BLE_EV_WRITE 1
#define BLE_EV_READ  2

#endif
