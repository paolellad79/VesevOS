// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_int.h
// INTERNO alla shell: include, aiuti e i tre gruppi di comandi (vos_shell_sys/net/admin.cpp). L'interfaccia pubblica e vos_shell.h.
#pragma once
#include "vos_shell.h"
#include "../packages/vos_rules.h"
#include "../core/vos_boot.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_serial.h"
#include "esp_heap_caps.h"
#include "../security/vos_auth.h"
#include "../core/vos_sys.h"
#include "../net/vos_net.h"
#include "../packages/vos_mqtt.h"
#include "../devices/vos_led.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_dev.h"
#include "../core/vos_util.h"
#include "../core/vos_ram.h"
#include "../core/vos_files.h"
#include "../core/vos_time.h"
#include "../core/vos_log.h"
#include "../core/vos_serial.h"
#include "../core/vos_i18n.h"
#include "../core/vos_license.h"
#include "../core/vos_region.h"
#include "../core/vos_audit.h"
#include "../core/vos_fw.h"
#include "../core/vos_wd.h"
#include "../packages/vos_mesh.h"
#include "../packages/vos_ble.h"
#include "../core/vos_service.h"
#include "../security/vos_tls.h"
#include "vos_web.h"
#include "../packages/vos_power.h"
#include "../packages/vos_stats.h"
#include "../core/vos_selftest.h"
#include "../core/vos_diario.h"
extern bool g_extmem;                 // definita in VesevOS.ino: i blocchi grandi vanno in PSRAM?
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_ota.h"


namespace shx {
String argAt(const String& s, int idx);          // parola numero idx della riga (0 = comando)
String restFrom(const String& s, int idx);       // il resto della riga dalla parola idx
void   cmdHelp(Print& o);
int    roleArg(const String& r);
String hex6(uint32_t c);                         // colore in 6 cifre esadecimali
void   wzStart(Print& o);                        // configurazione guidata da seriale
}  // namespace shx
using namespace shx;

// Gruppi di comandi: ognuno ritorna true se ha riconosciuto e gestito il comando (stesso ordine della vecchia catena if/else).
bool shCmdSys(String c, String a1, const String& line, Print& o, int role);     // vos_shell_sys.cpp  : help ... cpu (stato, file, ora)
bool shCmdNet(String c, String a1, const String& line, Print& o, int role);     // vos_shell_net.cpp  : wifi ... pins (rete, LED, pin, periferiche)
bool shCmdAdmin(String c, String a1, const String& line, Print& o, int role);   // vos_shell_admin.cpp: config ... reboot (utenti, sicurezza, servizi)
