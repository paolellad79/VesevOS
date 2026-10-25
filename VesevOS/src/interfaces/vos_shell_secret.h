// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_secret.h
// A cosa serve una password o frase chiesta dalla shell. Sta qui da sola, cosi vos_shell_int.h e vos_shell_ask.h non si includono a vicenda.
#pragma once
#include <stdint.h>

namespace shx {
enum SecretFor : uint8_t { SECRET_PASSWD, SECRET_USER_ADD, SECRET_WIFI_SET, SECRET_BACKUP_CREATE, SECRET_BACKUP_RESTORE, SECRET_BACKUP_RESTORE_YES };
}  // namespace shx
