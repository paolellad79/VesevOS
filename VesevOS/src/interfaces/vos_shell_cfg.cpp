// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cfg.cpp
// Comando "vesevos-config": fronte unico di configurazione, NON interattivo (le domande y/n e le password le fanno gia i comandi).
//
//   vesevos-config                       riepilogo per aree (tutti i ruoli; utenti e sicurezza nel dettaglio solo Admin)
//   vesevos-config <area>                valori dell'area e comandi che li cambiano (vos_shell_cfgarea.cpp)
//   vesevos-config setup [done]          la guida "setup" (solo Admin)
//   vesevos-config <area> <comando ...>  esegue quel comando (solo Admin)
//   Aree: system network region time services users security led cpu
//
// NESSUN MODO NUOVO DI CAMBIARE IMPOSTAZIONI. Chi scrive "vesevos-config region country IT" fa eseguire la riga "region country IT" da
// shellExecSess, cioe dal percorso normale: nome canonico, tabella dei ruoli (vos_shell_roles.cpp), conferme, domande di password,
// limiti della radio, registro delle modifiche. Qui non c'e nessuna scrittura della configurazione. Tre cose garantiscono che l'inoltro
// non apra niente:
//   1. la prima parola dopo l'area deve essere uno dei comandi UFFICIALI dell'elenco dell'area (kAreas): "vesevos-config" non e in nessun
//      elenco, quindi non si richiama da solo, e niente fuori dall'elenco si raggiunge da qui;
//   2. si inoltra solo se chi scrive e Admin (l'Operatore vede soltanto, come da richiesta); in piu la riga inoltrata ripassa dal controllo
//      dei ruoli come se l'avesse scritta l'utente;
//   3. i comandi che mostrano segreti (ap, wps, mesh key, mqtt) non sono negli elenchi: restano comandi a parte.
#include "vos_shell_int.h"
#include "vos_shell_rep.h"
#include "../packages/vos_mfa.h"
#include "../devices/vos_led_trigger.h"
#include "../drivers/vos_drv_cpu.h"
#include "../core/vos_cpupm.h"

namespace {

const size_t CFG_OUT_MAX = 4096;       // tetto del riepilogo
const size_t FWD_MAX     = 320;        // riga inoltrata (la pagina ne manda al massimo 200)

struct Area {
  const char* name;
  const char* cmds;      // comandi ufficiali che si possono inoltrare, separati da spazi
  const char* deflt;     // comando che si aggiunge davanti se la prima parola non e uno dei cmds ("" = nessuno)
};

const Area kAreas[] = {
  {"system",   "hostname domain lang",              ""},
  {"network",  "wifi airplane",                     ""},
  {"region",   "region",                            "region"},     // vesevos-config region country IT = region country IT
  {"time",     "date ntp",                          ""},
  {"services", "svc vesevos-services",              ""},
  {"users",    "user passwd",                       ""},
  {"security", "serial-auth serial firewall unban", ""},
  {"led",      "led",                               "led"},
  {"cpu",      "cpu",                               "cpu"},
};

const char* const kAreaNames = "system network region time services users security led cpu setup";

bool inList(const char* list, const char* w) {
  size_t wl = strlen(w);
  while (*list) {
    while (*list == ' ') list++;
    const char* b = list;
    while (*list && *list != ' ') list++;
    if ((size_t)(list - b) == wl && wl && strncmp(b, w, wl) == 0) return true;
  }
  return false;
}

const Area* findArea(const char* name) {
  for (const Area& a : kAreas) if (!strcmp(a.name, name)) return &a;
  return nullptr;
}

// ---- riepilogo ----

void secSystem(HeapBuf& h) {
  char up[64];
  uptimeTo(sysUptimeSec(), up, sizeof up);
  repHead(h, TRK("Sistema"));
  repRow(h, TRK("Nome host"), cfg.hostname.c_str());
  repRow(h, TRK("Versione"), VOS_VERSION);
  repRow(h, TRK("Acceso da"), up);
}

void secRegion(HeapBuf& h) {
  char v[64];
  repHead(h, TRK("Paese e radio"));
  repRow(h, TRK("Paese"), cfg.country.length() ? cfg.country.c_str() : tr("non scelto (regole prudenti)"));
  snprintf(v, sizeof v, "%d dBm", regionTxDbm());
  repRow(h, TRK("Potenza"), v);
  if (cfg.antExt) snprintf(v, sizeof v, "%s, %d dBi", tr("esterna"), (int)cfg.antGain);
  else snprintf(v, sizeof v, "%s", tr("interna"));
  repRow(h, TRK("Antenna"), v);
}

void secTime(HeapBuf& h) {
  char now[32];
  timeNowFmt(now, sizeof now);
  repHead(h, TRK("Ora e fuso"));
  repRow(h, TRK("Ora"), now);
  repRow(h, TRK("Fuso"), cfg.tzName.length() ? cfg.tzName.c_str() : cfg.tz.c_str());
  repRow(h, TRK("NTP"), repOnOff(cfg.ntpOn));
}

void secServices(HeapBuf& h) {
  HeapBuf names(256);
  repHead(h, TRK("Servizi"));
  repRow(h, TRK("Attivi"), shServicesActive(names) && names.length() ? names.c_str() : "-");
  repRow(h, TRK("Dettagli"), "vesevos-services");
}

// Utenti: Admin vede nomi e ruoli; gli altri solo quanti sono (l'elenco degli utenti e un comando da Admin).
void secUsers(HeapBuf& h, bool adm) {
  repHead(h, TRK("Utenti"));
  int n = 0;
  HeapBuf list(256);
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (!cfg.users[i].name.length()) continue;
    n++;
    if (list.length()) list.add(", ");
    list.addf("%s (%s)", cfg.users[i].name.c_str(), roleName(cfg.users[i].role));
  }
  char c[12];
  snprintf(c, sizeof c, "%d", n);
  repRow(h, TRK("Numero"), c);
  if (adm && list.length()) repRow(h, TRK("Nomi e ruoli"), list.c_str());
}

void secLed(HeapBuf& h) {
  char b[12];
  snprintf(b, sizeof b, "%d", (int)cfg.ledBrightness);
  repHead(h, TRK("LED"));
  repRow(h, TRK("Trigger"), ledTrigName(cfg.ledMode));
  repRow(h, TRK("Luminosita"), b);
}

void secCpu(HeapBuf& h) {
  char v[64];
  if (cfg.cpuMhz) snprintf(v, sizeof v, "fixed, %u MHz", (unsigned)cfg.cpuMhz);
  else snprintf(v, sizeof v, "%s", cpuChoiceName());
  repHead(h, TRK("CPU"));
  repRow(h, TRK("Governor"), v);
  snprintf(v, sizeof v, "%u MHz", (unsigned)drvCpuMhz());
  repRow(h, TRK("Frequenza"), v);
}

// Sicurezza: Admin vede seriale, blocco e MFA per utente (solo "si/no"); gli altri una riga che rimanda all'Admin.
void secSecurity(HeapBuf& h, bool adm) {
  repHead(h, TRK("Sicurezza"));
  if (!adm) { repRow(h, TRK("Dettagli"), tr("solo per l'Admin")); return; }
  repRow(h, TRK("Login seriale"), repOnOff(cfg.serialAuth));
  repRow(h, TRK("Blocco dopo"), trf("%d errori", (int)cfg.banFails).c_str());
  HeapBuf mfa(256);
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (!cfg.users[i].name.length()) continue;
    if (mfa.length()) mfa.add(", ");
    mfa.addf("%s %s", cfg.users[i].name.c_str(), repYesNo(mfaOn(i)));
  }
  repRow(h, TRK("MFA"), mfa.length() ? mfa.c_str() : "-");
}

void overview(OutSink& o, int role) {
  bool adm = role == ROLE_ADMIN;
  HeapBuf h(CFG_OUT_MAX);
  h.addf("%s %s - %s\n", VOS_NAME, VOS_VERSION, tr("configurazione"));
  h.add('\n');
  secSystem(h);
  repHead(h, TRK("Rete"));
  shNetBrief(h);
  secRegion(h);
  secTime(h);
  secServices(h);
  secUsers(h, adm);
  secLed(h);
  secCpu(h);
  secSecurity(h, adm);
  h.add('\n');
  h.addf("%s %s\n", tr("Dettagli e comandi di un'area: vesevos-config <area>. Aree:"), kAreaNames);
  repFlush(o, h);
}

// ---- inoltro ----

// Esegue il comando che c'e gia, dal percorso normale. Solo Admin.
void forwardLine(const Area& a, const char* line, OutSink& o, int role, uint32_t sess) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  Word first = argAt(line, 2);
  if (first.cut()) { o.println(tr("Comando troppo lungo")); return; }
  first.lower();
  canonicalName(first);                                   // "locale" -> region, "users" -> user: si confronta il nome ufficiale
  const char* rest = restFrom(line, 2);
  char fwd[FWD_MAX];
  int n;
  if (inList(a.cmds, first.c_str())) n = snprintf(fwd, sizeof fwd, "%s", rest);
  else if (a.deflt[0]) n = snprintf(fwd, sizeof fwd, "%s %s", a.deflt, rest);
  else { o.println(trf("In quest'area si possono usare: %s", a.cmds)); return; }
  if (n < 0 || (size_t)n >= sizeof fwd) { o.println(tr("Comando troppo lungo")); return; }
  shellExecSess(fwd, o, role, sess);                      // ruoli, conferme, password e limiti: quelli di sempre
}

// vesevos-config setup [done] = setup [done]
void forwardSetup(const char* line, OutSink& o, int role, uint32_t sess) {
  Word w = argAt(line, 2);
  w.lower();
  if (!w.empty() && w != "done") { o.println(tr("Uso: vesevos-config setup [done]")); return; }
  shellExecSess(w.empty() ? "setup" : "setup done", o, role, sess);
}

}  // namespace

void shx::shConfig(const char* line, OutSink& o, int role, uint32_t sess) {
  Word area = argAt(line, 1);
  area.lower();
  if (area.cut()) { o.println(tr("Comando troppo lungo")); return; }
  if (area.empty()) { overview(o, role); return; }
  if (area == "setup") { forwardSetup(line, o, role, sess); return; }
  const Area* a = findArea(area.c_str());
  if (!a) { o.println(trf("Area sconosciuta: %s (aree: %s)", area.c_str(), kAreaNames)); return; }
  if (argAt(line, 2).empty()) { shCfgArea(a->name, o, role); return; }
  forwardLine(*a, line, o, role, sess);
}
