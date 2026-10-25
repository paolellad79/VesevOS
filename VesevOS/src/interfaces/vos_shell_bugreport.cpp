// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_bugreport.cpp
// Comando "vesevos-bugreport" (solo Admin): un rapporto di testo per l'assistenza, scritto SOLO a schermo. Non invia niente fuori dalla scheda e non
// scrive file.
//
//   vesevos-bugreport [-f] [-n <righe>]
//     -n <righe>  quante righe di registro (5-100, di fabbrica 30); le righe degli allarmi sono al massimo 20
//     -f          rapporto COMPLETO: IP e MAC non mascherati e nome della rete di casa. Contiene dati identificativi: non pubblicare.
//
// SEGRETI: mai password, token, chiavi, impronte, codici MFA o di recupero, certificato e chiave privata, credenziali MQTT o Wi-Fi. Questi dati non
// vengono letti. In piu, le righe del registro e degli allarmi che nominano un segreto (elenco kSecretWords) sono sostituite da una riga
// "omessa", anche con -f. Di fabbrica gli indirizzi IPv4 diventano a.b.x.x e i MAC perdono gli ultimi 3 byte. Gli IPv6 non sono mascherati
// (la scheda non li mostra). Nomi utente, hostname e nomi di rete possono comparire nel registro: leggi il rapporto prima di condividerlo.
//
// Niente di lento: non fa scansioni Wi-Fi, prove, benchmark ne "ram audit". Il testo sta in un HeapBuf con tetto BUG_MAX (in PSRAM, oltre 4 KB):
// se non basta, il rapporto finisce troncato e lo dice.
#include "vos_shell_int.h"
#include "../core/vos_cpupm.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include <ctype.h>

namespace {

const size_t BUG_MAX      = 24576;   // tetto del rapporto
const size_t LINE_MAX_LEN = 200;     // una riga di registro piu lunga si taglia
const size_t CRASH_MAX    = 3000;    // dei rapporti dei crash si tengono i primi (i piu recenti) tanti byte
const int    LOG_DEFAULT  = 30;
const int    AUDIT_MAX    = 20;

// Parole che, in una riga di registro, fanno pensare a un segreto: la riga non si mostra.
const char* const kSecretWords[] = {
  "password", "passwd", "pass=", "token", "secret", "chiave", "key", "mfa", "totp", "otp", "recupero", "hash", "impronta", "psk", "privat",
};

bool isHex(char c) { return isxdigit((unsigned char)c) != 0; }

// Un indirizzo MAC "AA:BB:CC:DD:EE:FF" che comincia in s? Ritorna la lunghezza (17) o 0.
size_t macAt(const char* s, size_t n) {
  if (n < 17) return 0;
  for (int g = 0; g < 6; g++) {
    const char* p = s + g * 3;
    if (!isHex(p[0]) || !isHex(p[1])) return 0;
    if (g < 5 && p[2] != ':') return 0;
  }
  if (n > 17 && (isHex(s[17]) || s[17] == ':')) return 0;
  return 17;
}

// Un indirizzo IPv4 "a.b.c.d" che comincia in s? Ritorna la lunghezza (0 = no); keep = lunghezza di "a.b".
size_t ipAt(const char* s, size_t n, size_t& keep) {
  size_t i = 0;
  for (int g = 0; g < 4; g++) {
    size_t d = 0;
    while (i < n && isdigit((unsigned char)s[i]) && d < 4) { i++; d++; }
    if (d == 0 || d > 3) return 0;
    if (g < 3) {
      if (i >= n || s[i] != '.') return 0;
      if (g == 1) keep = i;
      i++;
    }
  }
  if (i < n && (isdigit((unsigned char)s[i]) || (s[i] == '.' && i + 1 < n && isdigit((unsigned char)s[i + 1])))) return 0;
  return i;
}

// Copia s[0..n) in rep mascherando: gli IPv4 diventano a.b.x.x e i MAC AA:BB:CC:xx:xx:xx.
void addMasked(HeapBuf& rep, const char* s, size_t n) {
  size_t i = 0;
  while (i < n) {
    bool boundary = i == 0 || !isalnum((unsigned char)s[i - 1]);
    if (boundary && isHex(s[i])) {
      size_t m = macAt(s + i, n - i);
      if (m) { rep.add(s + i, 8); rep.add(":xx:xx:xx"); i += m; continue; }
    }
    if (boundary && (i == 0 || s[i - 1] != '.') && isdigit((unsigned char)s[i])) {
      size_t keep = 0, m = ipAt(s + i, n - i, keep);
      if (m) { rep.add(s + i, keep); rep.add(".x.x"); i += m; continue; }
    }
    rep.add(s[i]);
    i++;
  }
}

bool looksSecret(const char* line, size_t n) {
  char low[LINE_MAX_LEN + 1];
  if (n > LINE_MAX_LEN) n = LINE_MAX_LEN;
  for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)line[i]);
  low[n] = 0;
  for (const char* w : kSecretWords) if (strstr(low, w)) return true;
  return false;
}

// Aggiunge le ultime tail righe di text (0 = tutte), una per volta, saltando quelle che nominano un segreto. omitted conta le saltate.
// Il mascheramento di IP e MAC si fa dopo, una volta sola, su tutto il rapporto.
void addLines(HeapBuf& rep, const char* text, int tail, int& omitted) {
  size_t total = 0;
  for (const char* p = text; *p; p++) if (*p == '\n') total++;
  if (*text && text[strlen(text) - 1] != '\n') total++;
  size_t skip = (tail > 0 && total > (size_t)tail) ? total - (size_t)tail : 0;
  const char* p = text;
  while (*p) {
    const char* e = strchr(p, '\n');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len && p[len - 1] == '\r') len--;
    if (skip) skip--;
    else if (len) {
      if (looksSecret(p, len)) { rep.add(tr("[riga omessa: puo contenere un segreto]")); omitted++; }
      else {
        bool cut = len > LINE_MAX_LEN;
        rep.add(p, cut ? LINE_MAX_LEN : len);
        if (cut) rep.add("...");
      }
      rep.add('\n');
    }
    if (!e) break;
    p = e + 1;
  }
}

// Una riga: il testo e a-capo. (Il testo tradotto non porta il \n: resta fuori dalle chiavi di lingua.)
void repLine(HeapBuf& rep, const char* text) {
  rep.add(text);
  rep.add('\n');
}

void repSection(HeapBuf& rep, const char* title) { rep.addf("\n-- %s --\n", title); }

const char* netStateText(NetState s) {
  switch (s) {
    case NET_BOOT:       return tr("avvio");
    case NET_AP:         return tr("solo hotspot");
    case NET_CLIENT_TRY: return tr("collegamento in corso");
    case NET_CLIENT_OK:  return tr("collegata alla Wi-Fi di casa");
    case NET_AIR:        return tr("modo aereo");
  }
  return tr("avvio");
}

void secVersion(HeapBuf& rep) {
  char build[40];
  drvBuildDateTo(build, sizeof build);
  unsigned rev = (unsigned)drvChipRevision();
  rep.addf("%s %s   build: %s\n", VOS_NAME, VOS_VERSION, build[0] ? build : "?");
  rep.addf("%s rev %u.%u, %u core, ESP-IDF %s\n", drvChipModel(), rev / 100, rev % 100, (unsigned)drvChipCores(), drvSdkVersion());
  repLine(rep, trf("Flash %u KB, PSRAM %u KB", (unsigned)(drvFlashSize() / 1024), (unsigned)(drvPsramTotal() / 1024)));
}

void secSystem(HeapBuf& rep) {
  char up[64], now[32];
  uptimeTo(sysUptimeSec(), up, sizeof up);
  repLine(rep, trf("Acceso da: %s", up));
  repLine(rep, trf("Ultimo reset: %s", sysResetText()));
  repLine(rep, trf("Avvii totali: %lu", (unsigned long)sysBootCount()));
  if (timeValid()) { timeNowFmt(now, sizeof now); repLine(rep, trf("Ora della scheda: %s", now)); }
  else repLine(rep, tr("Ora della scheda: non sincronizzata"));
  repLine(rep, trf("RAM interna: libera %u KB, minima %u KB, blocco piu grande %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMinFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024)));
  repLine(rep, trf("PSRAM: libera %u KB su %u KB", (unsigned)(drvPsramFree() / 1024), (unsigned)(drvPsramTotal() / 1024)));
  repLine(rep, trf("/flash usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
  char tt[16];
  fmtTemp(sysCpuTemp(), tt, sizeof tt);
  repLine(rep, trf("CPU: governor %s, %u MHz (tetto %u), temperatura %s (%s)", cpuChoiceName(), (unsigned)drvCpuMhz(), (unsigned)cpuPmLimitMhz(), tt,
           cpuPmThermalText(cpuPmThermal())));
  repLine(rep, trf("CPU: motivo del tetto: %s, cambi dall'avvio %lu", cpuPmWhyText(cpuPmWhy()), (unsigned long)cpuPmChanges()));
}

void secNet(HeapBuf& rep, bool full) {
  char ip[20], mac[20];
  repLine(rep, trf("Rete: %s, modo Wi-Fi %d (1 client, 2 hotspot, 3 entrambi, 0 spento)", netStateText(netState()), (int)drvWifiMode()));
  repLine(rep, trf("Nome host: %s", cfg.hostname.c_str()));
  repLine(rep, trf("Canale: %d", drvWifiChannel()));
  if (netState() == NET_CLIENT_OK) repLine(rep, trf("Segnale: %d dBm", drvWifiRssi()));
  if (cfg.apOn) repLine(rep, trf("Hotspot acceso, collegati: %d", drvWifiApClients()));
  repLine(rep, trf("Wi-Fi di casa configurata: %s", cfg.staEnabled && cfg.staSsid.length() ? tr("si") : tr("no")));
  if (full && cfg.staEnabled && cfg.staSsid.length()) repLine(rep, trf("Nome della rete di casa: %s", cfg.staSsid.c_str()));
  netIpTo(ip, sizeof ip);
  repLine(rep, trf("Indirizzo: %s", ip));
  netMacTo(false, mac, sizeof mac);
  rep.addf("MAC: %s\n", mac);
}

void secServices(HeapBuf& rep) {
  rep.add(tr("Servizi attivi: "));
  HeapBuf names(256);
  if (shServicesActive(names) && names.length()) rep.add(names.c_str()); else rep.add("-");
  rep.add('\n');
}

void secPartitions(HeapBuf& rep) {
  char run[64], next[64];
  drvOtaRunningTo(run, sizeof run);
  drvOtaNextTo(next, sizeof next);
  repLine(rep, trf("Programma in uso: %s", run[0] ? run : "?"));
  repLine(rep, trf("Prossimo aggiornamento in: %s", next[0] ? next : "?"));
  if (drvRecoveryPresent()) { const char* v = recVersion(); repLine(rep, trf("Recovery presente, versione %s", v[0] ? v : "?")); }
  else repLine(rep, tr("Recovery non presente (tabella partizioni vecchia)"));
}

void secCrash(HeapBuf& rep, int& omitted) {
  std::string c = crashText();
  if (c.empty()) { repLine(rep, tr("Nessun rapporto di crash")); return; }
  if (c.size() > CRASH_MAX) {                                     // si tengono i piu recenti (sono in cima); si taglia a fine riga
    size_t cut = c.rfind('\n', CRASH_MAX);
    c.resize(cut == std::string::npos ? CRASH_MAX : cut);
    c += '\n';
    addLines(rep, c.c_str(), 0, omitted);
    repLine(rep, tr("[rapporti piu vecchi omessi: comando crash]"));
    return;
  }
  addLines(rep, c.c_str(), 0, omitted);
  repLine(rep, trf("Per decodificare gli indirizzi: %s -pfiaC -e VesevOS.elf <indirizzi> (con il .elf della stessa versione)", VOS_ADDR2LINE));
}

}  // namespace

void shBugreport(const char* line, OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  bool full = false;
  int logLines = LOG_DEFAULT;
  for (int i = 1;; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w == "-f") full = true;
    else if (w == "-n") {
      long n = argAt(line, ++i).toInt();
      if (n < 5 || n > 100) { o.println(tr("Uso: vesevos-bugreport [-f] [-n <righe 5-100>]")); return; }
      logLines = (int)n;
    } else { o.println(tr("Uso: vesevos-bugreport [-f] [-n <righe 5-100>]")); return; }
  }
  bool mask = !full;
  int omitted = 0;
  HeapBuf rep(BUG_MAX);
  rep.addf("=== %s bugreport ===\n", VOS_NAME);
  repLine(rep, tr("Rapporto locale per l'assistenza: non e stato inviato niente. Password, chiavi e codici non sono inclusi."));
  if (full) repLine(rep, tr("COMPLETO: contiene dati identificativi (indirizzi IP, MAC, nome della rete): non pubblicare."));
  else repLine(rep, tr("Indirizzi IP e MAC mascherati. Nomi utente, nome host e nomi di rete possono comparire nel registro: rileggi prima di condividere."));
  repSection(rep, tr("Versione"));      secVersion(rep);
  repSection(rep, tr("Sistema"));       secSystem(rep);
  repSection(rep, tr("Rete"));          secNet(rep, full);
  repSection(rep, tr("Servizi"));       secServices(rep);
  repSection(rep, tr("Partizioni e recovery")); secPartitions(rep);
  repSection(rep, tr("Ultimi crash"));  secCrash(rep, omitted);
  repSection(rep, tr("Diario dei riavvii"));
  { std::string d = diaryText(8, true); if (d.empty()) repLine(rep, tr("Nessun avvio registrato")); else addLines(rep, d.c_str(), 0, omitted); }
  repSection(rep, tr("Registro (ultime righe)"));
  { std::string l = logGet(logLines); addLines(rep, l.c_str(), logLines, omitted); }
  repSection(rep, tr("Allarmi e modifiche (ultime righe)"));
  { HeapBuf a(AUDIT_JSON_MAX); auditText(a); addLines(rep, a.c_str(), logLines < AUDIT_MAX ? logLines : AUDIT_MAX, omitted); }
  rep.add('\n');
  bool cut = rep.over() || rep.oom();
  if (mask) {                                                 // una sola passata su tutto il testo: anche le righe scritte qui (rete) sono coperte
    HeapBuf shown(BUG_MAX);
    addMasked(shown, rep.c_str(), rep.length());
    o.print(shown.c_str());
  } else o.print(rep.c_str());
  if (cut) o.println(tr("[RAPPORTO TRONCATO: supera il tetto di 24 KB o manca la memoria. Riprova con -n piu basso]"));
  if (omitted) o.println(trf("Righe omesse perche possono contenere un segreto: %d", omitted));
  o.println(tr("Omessi perche lenti: scansione Wi-Fi, selftest, benchmark, ram audit (lanciali a parte)."));
  if (full) o.println(tr("COMPLETO: contiene dati identificativi: non pubblicare."));
  vlog("SISTEMA: rapporto per l'assistenza creato dalla shell%s", full ? " (completo)" : "");
}
