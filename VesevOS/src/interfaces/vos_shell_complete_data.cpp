// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_complete_data.cpp
// Tabelle del completamento col Tab che NON stanno nella tabella dei comandi. I nomi dei comandi e le parole che seguono il comando
// (la prima dopo il nome) stanno in vos_shell_cmdtab.cpp; qui restano solo le parole piu a fondo, che dipendono da quella prima
// (per esempio "cpu governor <nome>", "btn 2 <azione>") e i comandi con percorsi. Niente e inventato: le parole vengono dai messaggi
// "Uso:" dei comandi. Se cambi un comando, aggiorna anche la tabella.
// I RUOLI non stanno qui: chi puo usare cosa e deciso solo da vos_shell_roles.cpp (il Tab lo chiede li).
#include "vos_shell_complete_data.h"

const SubRule kSubRules[] = {
  {"serial", 2, "eol", "crlf lf cr"},
  {"serial", 2, "echo input log banner", "on off"},
  {"cpu", 2, "info", "-v"},
  {"cpu", 2, "governor", "ondemand conservative schedutil fixed"},
  {"cpu", 3, "fixed", "80 160 240"},
  {"cpu", 2, "freq", "80 160 240"},
  {"cpu", 2, "set", "up down hold"},
  {"vesevos-services", 2, "http https redirect ap captive dhcp mdns wifi wps mqtt mesh ble ntp rules script wd", "start stop restart status"},
  {"vesevos-config", 2, "setup", "done"},
  {"vesevos-config", 2, "system", "hostname domain lang"},
  {"vesevos-config", 2, "network", "wifi airplane"},
  {"vesevos-config", 2, "region", "country tx antenna"},
  {"vesevos-config", 2, "time", "date ntp"},
  {"vesevos-config", 2, "services", "svc vesevos-services"},
  {"vesevos-config", 2, "users", "user passwd"},
  {"vesevos-config", 2, "security", "serial-auth serial firewall unban"},
  {"vesevos-config", 2, "led", "trigger color brightness threshold"},
  {"vesevos-config", 2, "cpu", "info governor freq why set"},
  {"vesevos-config", 3, "wifi", "set off"},
  {"vesevos-config", 3, "airplane", "on off"},
  {"vesevos-config", 3, "date", "set"},
  {"vesevos-config", 3, "ntp", "on off sync every server"},
  {"vesevos-config", 3, "serial-auth", "on off"},
  {"vesevos-config", 3, "antenna", "int ext"},
  {"vesevos-cleaner", 2, "-y tmp crash log reboots cache", "tmp crash log reboots cache"},
  {"vesevos-cleaner", 3, "tmp crash log reboots cache", "tmp crash log reboots cache"},
  {"vesevos-cleaner", 4, "tmp crash log reboots cache", "tmp crash log reboots cache"},
  {"vesevos-cleaner", 5, "tmp crash log reboots cache", "tmp crash log reboots cache"},
  {"wps", 2, "pin", "on off"},
  {"wps", 2, "start", "pin"},
  {"btn", 2, "1 2 3", "none airplane wps find"},
  {"svc", 2, "ap captive http https redirect dhcp mdns", "on off"},
  {"led", 2, "trigger", "none state heartbeat flash wifi cpu default-on"},
  {"led", 2, "color", "flash-read flash-write wifi-rx wifi-tx cpu"},
  {"io", 2, "add", "relay led pin input analog i2c"},
  {"pin", 2, nullptr, "high low blink read off"},
  {"pin", 3, "read", "up down"},
  {"user", 3, "add role", "admin oper guest"},
  {"region", 2, "antenna", "int ext"},
  {"wd", 2, "set", "twdt net monitor mesh mqtt led"},
  {"mesh", 2, "key", "new"},
  {"mesh", 2, "role", "0 1 2"},
  {"log", 2, "level", "0 1 2 3"},
  {"selftest", 2, "active", "names"},
  {"crash", 2, "test", "yes"},
};
const size_t kSubRulesN = sizeof(kSubRules) / sizeof(kSubRules[0]);

const PathRule kPathRules[] = {
  {"cd", 1, 1, true, nullptr},
  {"ls", 1, 8, false, "-l -a -h -1 -F"},
  {"cat", 1, 1, false, nullptr},
  {"mkdir", 1, 1, true, nullptr},
  {"rm", 1, 1, false, nullptr},
  {"mv", 1, 2, false, nullptr},
  {"cp", 1, 2, false, nullptr},
  {"write", 1, 1, false, nullptr},
  {"gzip", 1, 16, false, "-d -c -k -f -1 -9"},
  {"gunzip", 1, 16, false, "-c -k -f"},
  {"zcat", 1, 16, false, nullptr},
  {"unzip", 1, 3, false, "-l -o -d"},
  {"zip", 1, 16, false, "-r -q -f -j -0 -1 -9"},
  {"tar", 1, 16, false, "-c -t -x -v -f -C --overwrite -cf -tf -xf"},
};
const size_t kPathRulesN = sizeof(kPathRules) / sizeof(kPathRules[0]);

