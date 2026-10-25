// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_roles.cpp
// La tabella dei ruoli della shell. Si legge dall'alto in basso: vince la prima riga che combacia col comando e col sottocomando.
//   sub = ""  : il comando scritto da solo
//   sub = "*" : qualunque altro sottocomando (va messa DOPO le righe piu precise dello stesso comando)
// Regola di fondo: all'Operatore si lasciano solo lettura e uso della scheda (stato, LED, pin, regole). Tutto cio che cambia la rete,
// la radio (paese, potenza, antenna), l'ora, gli utenti, i file, i servizi o spegne la scheda e dell'Admin. L'Ospite non ha comandi.
// Un comando che non compare qui e solo Admin.
//
// I nomi: qui si scrivono i nomi UFFICIALI. I vecchi nomi (aereo, licenza, users, watchdog, locale, restart, logout, ?) diventano quelli ufficiali
// prima del controllo (canonicalName, vos_shell_cmdtab.cpp). Le varianti nascoste (led-color, led-bright, lscpu, sleep, wifi-scan) hanno la loro riga.
//
// ECCEZIONE VOLUTA: led, led-color, led-bright, airplane e "ntp sync" restano all'Operatore perche le REGOLE (Automazioni) girano con
// ruolo Operatore e solo queste cinque azioni sono nella loro lista bianca (vedi actionCheck in vos_rules.cpp). "airplane on" isola la
// scheda dalla rete: e un rischio noto e accettato, perche e proprio cio che le regole devono poter fare.
#include "vos_shell_roles.h"
#include "../core/vos_common.h"

namespace {

const int ADM = ROLE_ADMIN;
const int OPE = ROLE_OPER;

struct RoleRule {
  const char* cmd;
  const char* sub;
  int         min;     // ruolo minimo
};

const RoleRule kRoles[] = {
  // --- lettura e uso: Operatore ---
  {"help", "*", OPE}, {"man", "*", OPE}, {"uname", "*", OPE}, {"about", "*", OPE}, {"uptime", "*", OPE},
  {"whoami", "*", OPE}, {"id", "*", OPE}, {"echo", "*", OPE}, {"exit", "*", OPE},                  // identita e testo: niente che l'utente non sappia gia
  {"time", "*", OPE},                                                      // misura soltanto: il comando interno ha il SUO controllo (come se fosse scritto da solo)
  {"free", "*", OPE}, {"df", "*", OPE}, {"temp", "*", OPE}, {"top", "*", OPE}, {"ps", "*", OPE},
  {"info", "*", OPE}, {"net", "*", OPE}, {"ip", "*", OPE}, {"events", "*", OPE},
  {"legal", "*", OPE}, {"license", "*", OPE},
  {"pin", "*", OPE}, {"pins", "*", OPE}, {"rules", "*", OPE}, {"reboot", "*", OPE},
  // --- usati dalle regole (vedi sopra) ---
  {"led", "*", OPE}, {"led-color", "*", OPE}, {"led-bright", "*", OPE}, {"airplane", "*", OPE},   // led trigger/threshold/color <sorgente>: il comando chiede Admin per CAMBIARE (lettura: Operatore)
  {"ntp", "", OPE}, {"ntp", "sync", OPE}, {"ntp", "*", ADM},
  // --- con sottocomandi: la lettura all'Operatore, le modifiche all'Admin ---
  {"wifi", "set", ADM}, {"wifi", "off", ADM}, {"wifi", "scan", ADM}, {"wifi", "*", OPE},   // cambiare la rete di casa puo isolare la scheda; scan = wifi-scan (solo Admin, come prima)
  {"region", "", OPE}, {"region", "*", ADM},                               // paese, potenza, antenna: radio regolamentata (locale e il vecchio nome)
  {"date", "set", ADM}, {"date", "*", OPE},                                // cambiare l'ora altera registri e certificati
  {"audit", "ack", ADM}, {"audit", "*", OPE},                              // tacitare gli allarmi: Admin
  {"cpu", "", OPE}, {"cpu", "why", OPE}, {"cpu", "info", OPE}, {"cpu", "governor", OPE}, {"cpu", "freq", OPE}, {"cpu", "*", ADM},   // governor e freq: lettura OPE (la tabella qui sotto), con un valore il comando chiede Admin
  {"lscpu", "*", OPE}, {"vesevos-cpuinfo", "*", OPE},                      // alias di sola lettura di "cpu info" e del riepilogo esteso
  {"vesevos-services", "*", OPE},                                          // elenco e status: lettura; start/stop/restart: il comando chiede Admin
  {"vesevos-config", "setup", ADM}, {"vesevos-config", "users", ADM}, {"vesevos-config", "security", ADM}, {"vesevos-config", "*", OPE},   // riepilogo e dettagli in lettura: Operatore; users, security e setup: Admin. Cambiare: solo Admin (controllato nel comando), poi la riga inoltrata ripassa da questa tabella
  {"vesevos-network", "-f", ADM}, {"vesevos-network", "scan", ADM}, {"vesevos-network", "ping", ADM}, {"vesevos-network", "*", OPE},   // riepilogo: lettura; scan e ping sono "wifi scan" e "ping" (Admin, come oggi); -f = MAC intero
  {"vesevos-led_control", "*", OPE},                                       // alias VISIBILE di "led": nella lista di help; l'esecuzione usa la riga di "led" (canonicalName lo cambia prima)
  {"vesevos-drive_manager", "mount", ADM}, {"vesevos-drive_manager", "unmount", ADM}, {"vesevos-drive_manager", "test", ADM}, {"vesevos-drive_manager", "*", OPE},   // elenco e status: lettura; mount/unmount/test inoltrano a "sd" (solo Admin)
  {"vesevos-cleaner", "*", ADM},                                           // pulizia: solo Admin (cancella rapporti e registri)
  {"vesevos-bugreport", "*", ADM},                                         // rapporto per l'assistenza: solo Admin
  {"vesevos-backup", "*", ADM},                                            // backup cifrato della configurazione: contiene password e chiavi, solo Admin
  {"vesevos-update", "*", ADM},                                            // ordini di aggiornamento tramite recovery: solo Admin
  {"wd", "", OPE}, {"wd", "status", OPE}, {"wd", "*", ADM},
  {"log", "clear", ADM}, {"log", "level", ADM}, {"log", "*", OPE},
  {"reboots", "clear", ADM}, {"reboots", "*", OPE},
  {"mqtt", "", OPE}, {"mqtt", "status", OPE}, {"mqtt", "pub", OPE}, {"mqtt", "*", ADM},
  {"mesh", "", OPE}, {"mesh", "status", OPE}, {"mesh", "send", OPE}, {"mesh", "cmd", OPE}, {"mesh", "*", ADM},
  {"dev", "", OPE}, {"dev", "list", OPE}, {"dev", "status", OPE}, {"dev", "act", OPE}, {"dev", "*", ADM},   // dev act controlla poi il ruolo di ogni periferica
  // --- solo Admin (elenco per chiarezza: lo sarebbero anche senza riga) ---
  {"suspend", "*", ADM}, {"sleep", "*", ADM}, {"poweroff", "*", ADM},      // sonno profondo / spegnimento
  {"ping", "*", ADM},                                                      // solo Admin: genera traffico di rete verso indirizzi scelti dall'utente (rischio di sondare/inondare); con limiti di conteggio, intervallo e durata
  {"gzip", "*", ADM}, {"gunzip", "*", ADM}, {"zcat", "*", ADM}, {"unzip", "*", ADM}, {"zip", "*", ADM}, {"tar", "*", ADM},   // solo Admin come i comandi sui file: scrivono e tolgono file, usano la PSRAM e tempo di CPU (tetti in vos_shell_arch.h)
  {"factory-reset", "*", ADM}, {"crash", "*", ADM}, {"passwd", "*", ADM}, {"user", "*", ADM},
};

}  // namespace

int shx::roleMin(const Word& cmd, const Word& sub) {
  Word s = sub;
  s.lower();                                  // "WIFI SET" non deve passare per un'altra riga di "wifi set"
  Word c = cmd;
  c.lower();
  for (const RoleRule& r : kRoles) {
    if (c != r.cmd) continue;
    if (r.sub[0] == '*' || s == r.sub) return r.min;
  }
  return ADM;
}

bool shx::roleAllows(int role, const Word& cmd, const Word& sub) { return role >= roleMin(cmd, sub); }
