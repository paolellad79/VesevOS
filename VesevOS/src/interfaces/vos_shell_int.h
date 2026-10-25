// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_int.h
// INTERNO alla shell: include, aiuti e i tre gruppi di comandi (vos_shell_sys/net/admin.cpp). L'interfaccia pubblica e vos_shell.h.
#pragma once
#include "vos_shell.h"
#include "vos_shell_line.h"
#include "vos_shell_sess.h"
#include "vos_shell_path.h"
#include "vos_shell_roles.h"
#include "vos_shell_cmdtab.h"
#include "vos_shell_secret.h"
#include "vos_shell_apply.h"
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
#include "../packages/vos_berry.h"
#include "../packages/vos_app.h"
#include "../packages/vos_selftest.h"
#include "../packages/vos_benchsha.h"
#include "../core/vos_diario.h"
extern bool g_extmem;                 // definita in vos_main.cpp: i blocchi grandi vanno in PSRAM?
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_ota.h"
#include "../core/vos_recupd.h"
#include "../core/vos_crash.h"


namespace shx {
int    roleArg(const Word& r);
void   wzStart(OutSink& o);                        // configurazione guidata da seriale (vos_shell_wiz.cpp)
void   wzLine(OutSink& o, char* line);             // una risposta della guida (la riga viene modificata)
bool   wzActive();                               // la guida e in corso?
void   wzAbort();                                // la guida si lascia (logout della seriale)
bool   scanRunning();                            // la scansione Wi-Fi sta ancora lavorando?
// Le funzioni "...Apply" (cosa fare quando la password c e) sono in vos_shell_apply.h
uint32_t shellClientIp();                                                               // vos_shell.cpp: IP del computer che usa la shell della pagina (0 = seriale o sconosciuto)
}  // namespace shx
using namespace shx;

// Gruppi di comandi: ognuno ritorna true se ha riconosciuto e gestito il comando (stesso ordine della vecchia catena if/else).
bool shCmdFiles(const Word& c, const Word& a1, const char* line, OutSink& o, int role, ShellSess& ss);  // vos_shell_files.cpp: pwd cd ls cat mkdir rm mv cp write (cartella corrente della sessione)
void shLs(const char* line, OutSink& o, const ShellSess& ss);                                       // vos_shell_ls.cpp   : comando "ls" (stile shell classica)
void shPing(const char* line, OutSink& o, uint32_t sess);                          // vos_shell_ping.cpp : comando "ping" (solo Admin; il tetto di durata dipende dalla sessione)
bool shCmdArchive(const Word& c, const char* line, OutSink& o, const ShellSess& ss, uint32_t sess);   // vos_shell_arch.cpp: gzip gunzip zcat unzip zip tar (compressione e archivi, solo Admin)
void shGzip(const char* cmd, const char* line, OutSink& o, const ShellSess& ss, uint32_t sess);       // vos_shell_gzip.cpp : gzip, gunzip, zcat
void shUnzip(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess);                       // vos_shell_zip.cpp  : unzip (elenco ed estrazione)
void shZip(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess);                         // vos_shell_zipc.cpp : zip (crea archivi .zip)
void shTar(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess);                         // vos_shell_tar.cpp  : tar -t, -x (e -c con vos_shell_tarc.cpp)
bool shCmdBasic(const Word& c, const Word& a1, const char* line, OutSink& o, int role, uint32_t sess);   // vos_shell_basic.cpp: help man whoami id echo df uname exit time
bool shCmdSys(const Word& c, const Word& a1, const char* line, OutSink& o, int role);     // vos_shell_sys.cpp  : help ... ntp (stato, memoria, ora)
bool shCmdCpu(const Word& c, const Word& a1, const char* line, OutSink& o, int role);     // vos_shell_cpu.cpp : cpu, lscpu, vesevos-cpuinfo
bool shServicesActive(HeapBuf& out);                                              // vos_shell_services.cpp: nomi dei servizi non fermi, separati da virgola
void shBugreport(const char* line, OutSink& o, int role);                         // vos_shell_bugreport.cpp: vesevos-bugreport
namespace shx {
void shConfig(const char* line, OutSink& o, int role, uint32_t sess);             // vos_shell_cfg.cpp    : vesevos-config (riepilogo, inoltro dei comandi che ci sono gia)
bool shCfgArea(const char* area, OutSink& o, int role);                           // vos_shell_cfgarea.cpp: dettagli di un'area (false = area sconosciuta)
void shNetwork(const char* line, OutSink& o, int role, uint32_t sess);            // vos_shell_network.cpp: vesevos-network (riepilogo, scan, ping)
void shNetworkSummary(OutSink& o, bool full);                                     // vos_shell_network.cpp: il riepilogo di rete (full = MAC intero)
void shNetBrief(HeapBuf& h);                                                      // vos_shell_network.cpp: righe Modo, Rete Wi-Fi, IP
void shCleaner(const char* line, OutSink& o, int role);                           // vos_shell_cleaner.cpp: vesevos-cleaner (pulizia sicura, prova a secco, solo Admin)
void shDriveManager(const char* line, OutSink& o, int role, uint32_t sess);       // vos_shell_drive.cpp  : vesevos-drive_manager (volumi; mount/unmount/test inoltrati a "sd")
void shBackup(const char* line, OutSink& o, int role);                            // vos_shell_backup.cpp : vesevos-backup (backup cifrato della configurazione in /backup, solo Admin)
void shUpdate(const char* line, OutSink& o, int role);                            // vos_shell_update.cpp : vesevos-update (stato e ordini di aggiornamento tramite recovery, solo Admin)
}  // namespace shx
bool shCmdVesevos(const Word& c, const Word& a1, const char* line, OutSink& o, int role, uint32_t sess); // vos_shell_services.cpp: dispatch dei vesevos-* (services, bugreport, config, network)
bool shCmdLed(const Word& c, const Word& a1, const char* line, OutSink& o, int role);     // vos_shell_led.cpp  : led, led-color, led-bright (modo, trigger, colori)
bool shCmdNet(const Word& c, const Word& a1, const char* line, OutSink& o, int role);     // vos_shell_net.cpp  : wifi ... pins (rete, LED, pin, periferiche)
void shSd(const char* line, OutSink& o, int role);                                 // vos_shell_sd.cpp   : comando "sd" (microSD), senza String
bool shCmdAdmin(const Word& c, const Word& a1, const char* line, OutSink& o, int role);   // vos_shell_admin.cpp: config ... reboot (utenti, sicurezza, servizi)
