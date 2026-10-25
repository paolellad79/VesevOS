// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cfgarea.cpp
// Dettagli di un'area di "vesevos-config <area>": i valori di adesso e, sotto, l'elenco dei COMANDI ESISTENTI che li cambiano.
// Qui si legge soltanto: niente cambia da questo file. Per i valori che hanno gia un comando di lettura (region, cpu, vesevos-services,
// vesevos-network) si richiama quel comando, senza ricopiarne la logica. Mai password, hash, chiavi o segreti MFA.
#include "vos_shell_int.h"
#include "vos_shell_rep.h"
#include "../packages/vos_mfa.h"
#include "../devices/vos_led_trigger.h"

namespace {

const size_t AREA_OUT_MAX = 3072;      // tetto del testo di un'area

void tailNote(HeapBuf& h) {
  h.add('\n');
  h.add(tr("Per cambiare (solo Admin) scrivi il comando dopo l'area: vesevos-config <area> <comando ...>"));
  h.add('\n');
  h.add(tr("Passa dal percorso normale della shell: stessi ruoli, limiti della radio, conferme e domande di password."));
  h.add('\n');
}

void areaSystem(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  char up[64];
  uptimeTo(sysUptimeSec(), up, sizeof up);
  repHead(h, TRK("Sistema"));
  repRow(h, TRK("Nome host"), cfg.hostname.c_str());
  repRow(h, TRK("Dominio"), cfg.domain.length() ? cfg.domain.c_str() : tr("(nessuno)"));
  repRow(h, TRK("Lingua"), cfg.lang.c_str());
  repRow(h, TRK("Versione"), VOS_VERSION);
  repRow(h, TRK("Acceso da"), up);
  repRow(h, TRK("Guida iniziale"), cfg.setupDone ? tr("completata") : tr("da finire (scrivi setup)"));
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "hostname <nome>", TRK("nome host (anche nome.local)"));
  repCmd(h, "domain <nome|->", TRK("dominio, o '-' per toglierlo"));
  repCmd(h, "lang <codice>", TRK("lingua"));
  tailNote(h);
  repFlush(o, h);
}

void areaRegion(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  repHead(h, TRK("Paese e radio"));
  h.add(regionText().c_str());
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "region country IT", TRK("paese (fissa canali e potenza ammessi)"));
  repCmd(h, "region tx <dBm>", TRK("potenza, mai oltre il limite del paese"));
  repCmd(h, "region antenna int|ext <dBi>", TRK("antenna interna o esterna"));
  tailNote(h);
  repFlush(o, h);
}

void areaTime(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  char now[32], every[40];
  timeNowFmt(now, sizeof now);
  if (cfg.ntpEvery) snprintf(every, sizeof every, "%s %lu min", tr("ogni"), (unsigned long)cfg.ntpEvery);
  else snprintf(every, sizeof every, "%s", tr("solo all'avvio"));
  repHead(h, TRK("Ora e fuso"));
  repRow(h, TRK("Ora"), now);
  repRow(h, TRK("Ora valida"), repYesNo(timeValid()));
  repRow(h, TRK("Fuso"), cfg.tzName.length() ? cfg.tzName.c_str() : cfg.tz.c_str());
  repRow(h, TRK("NTP"), repOnOff(cfg.ntpOn));
  repRow(h, TRK("Server NTP"), cfg.ntpServer.c_str());
  repRow(h, TRK("Sincronizza"), every);
  repRow(h, TRK("Ultima sincr."), timeLastSync() ? tr("avvenuta") : tr("mai"));
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "date set AAAA-MM-GG HH:MM", TRK("imposta l'ora a mano"));
  repCmd(h, "ntp on|off|sync", TRK("accende, spegne o sincronizza subito"));
  repCmd(h, "ntp every <minuti>", TRK("ogni quanto sincronizzare"));
  repCmd(h, "ntp server <nome>", TRK("server NTP"));
  tailNote(h);
  repFlush(o, h);
}

void areaLed(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  char col[12], br[12], thr[12];
  snprintf(col, sizeof col, "%06lX", (unsigned long)(cfg.ledColor & 0xFFFFFFUL));
  snprintf(br, sizeof br, "%d", (int)cfg.ledBrightness);
  snprintf(thr, sizeof thr, "%d%%", (int)cfg.ledCpuThr);
  repHead(h, TRK("LED"));
  repRow(h, TRK("Trigger"), ledTrigName(cfg.ledMode));
  repRow(h, TRK("Colore"), col);
  repRow(h, TRK("Luminosita"), br);
  repRow(h, TRK("Soglia CPU"), thr);
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "led trigger <nome>", TRK("che cosa accende il LED"));
  repCmd(h, "led color [sorgente] RRGGBB", TRK("colore (fisso o per sorgente)"));
  repCmd(h, "led brightness <0-255>", TRK("luminosita"));
  repCmd(h, "led threshold <10-95>", TRK("soglia di carico del trigger cpu"));
  tailNote(h);
  repFlush(o, h);
}

// Elenco dei comandi che cambiano, dopo un riepilogo che stampa un altro comando (cpu, rete, servizi).
void commandsOnly(OutSink& o, const char* a, const char* ak, const char* b, const char* bk, const char* c, const char* ck) {
  HeapBuf h(AREA_OUT_MAX);
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, a, ak);
  if (b) repCmd(h, b, bk);
  if (c) repCmd(h, c, ck);
  tailNote(h);
  repFlush(o, h);
}

void areaCpu(OutSink& o, int role) {
  shCmdCpu(Word("cpu", 3), Word(), "cpu", o, role);             // il riepilogo lo scrive il comando cpu
  o.println();
  commandsOnly(o, "cpu governor <nome>", TRK("ondemand, conservative, schedutil, fixed <MHz>"),
               "cpu freq 80|160|240", TRK("frequenza fissa"), "cpu set up|down|hold <n>", TRK("parametri dei governor"));
}

void areaNetwork(OutSink& o) {
  shNetworkSummary(o, false);
  o.println();
  commandsOnly(o, "wifi set <rete>", TRK("collega alla Wi-Fi di casa (la password te la chiedo io)"),
               "wifi off", TRK("scollega dalla Wi-Fi di casa"), "airplane on <come>|off", TRK("modo aereo (radio spenta)"));
}

void areaServices(OutSink& o, int role) {
  shCmdVesevos(Word("vesevos-services", 16), Word(), "vesevos-services", o, role, SESS_NONE);   // l'elenco lo scrive vesevos-services
  o.println();
  commandsOnly(o, "vesevos-services <servizio> start|stop|restart", TRK("avvia, ferma, riavvia (fermare http, https, ap chiede conferma)"),
               "svc <nome> on|off", TRK("hotspot, portale, HTTP, HTTPS, redirect, DHCP, mDNS"), nullptr, nullptr);
}

// Utenti: nomi e ruoli, mai hash o sale. Solo Admin.
void areaUsers(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  repHead(h, TRK("Utenti"));
  HeapBuf t(AUTH_TEXT_MAX);
  usersText(t);
  h.add(t.c_str());
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "user add <nome> <admin|oper|guest>", TRK("nuovo utente (la password te la chiedo io)"));
  repCmd(h, "user del <nome>", TRK("toglie un utente"));
  repCmd(h, "user role <nome> <ruolo>", TRK("cambia il ruolo"));
  repCmd(h, "user on|off <nome>", TRK("attiva o blocca un utente"));
  repCmd(h, "passwd <utente>", TRK("cambia la password (senza mostrarla)"));
  tailNote(h);
  repFlush(o, h);
}

// Sicurezza: solo Admin. L'MFA e solo "si/no" per utente: mai il segreto ne i codici di recupero.
void areaSecurity(OutSink& o) {
  HeapBuf h(AREA_OUT_MAX);
  char v[64];
  repHead(h, TRK("Sicurezza"));
  repRow(h, TRK("Login seriale"), repOnOff(cfg.serialAuth));
  if (cfg.serialIdleMin) snprintf(v, sizeof v, "%u min", (unsigned)cfg.serialIdleMin);
  else snprintf(v, sizeof v, "%s", tr("mai"));
  repRow(h, TRK("Chiusura seriale"), v);
  repRow(h, TRK("Blocco dopo"), trf("%d errori, primo blocco %lu s", (int)cfg.banFails, (unsigned long)cfg.banSecs).c_str());
  repRow(h, TRK("Filtro IP"), trf("modo %d (0 spento)", (int)cfg.fwMode).c_str());
  repRow(h, TRK("HTTPS"), repOnOff(cfg.https));
  repHead(h, TRK("MFA per utente"));
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (!cfg.users[i].name.length()) continue;
    repRaw(h, cfg.users[i].name.c_str(), repYesNo(mfaOn(i)));
    n++;
  }
  if (!n) h.addf("  %s\n", tr("Nessun utente"));
  repHead(h, TRK("Comandi che cambiano"));
  repCmd(h, "serial-auth on|off", TRK("login sulla seriale"));
  repCmd(h, "serial idle <minuti>", TRK("chiusura della seriale per inattivita"));
  repCmd(h, "firewall mode <0-3>", TRK("filtro degli indirizzi IP"));
  repCmd(h, "unban <ip|all>", TRK("sblocca un indirizzo"));
  h.addf("  %s\n", tr("L'MFA di ogni utente si gestisce dalla pagina web."));
  tailNote(h);
  repFlush(o, h);
}

}  // namespace

// I dettagli dell'area (nome gia controllato da chi chiama). false = area che non esiste.
bool shx::shCfgArea(const char* area, OutSink& o, int role) {
  bool adm = role == ROLE_ADMIN;
  if ((!strcmp(area, "users") || !strcmp(area, "security")) && !adm) { o.println(tr("Serve il livello Admin")); return true; }
  if (!strcmp(area, "system")) areaSystem(o);
  else if (!strcmp(area, "network")) areaNetwork(o);
  else if (!strcmp(area, "region")) areaRegion(o);
  else if (!strcmp(area, "time")) areaTime(o);
  else if (!strcmp(area, "services")) areaServices(o, role);
  else if (!strcmp(area, "users")) areaUsers(o);
  else if (!strcmp(area, "security")) areaSecurity(o);
  else if (!strcmp(area, "led")) areaLed(o);
  else if (!strcmp(area, "cpu")) areaCpu(o, role);
  else return false;
  return true;
}
