// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_basic.cpp
// I comandi di base nello stile bash: help, man, whoami, id, echo, df, uname, exit e time (e il collegamento a ping).
// Le descrizioni stanno nella tabella unica dei comandi (vos_shell_cmdtab.cpp), il testo di help e man in vos_shell_man.cpp.
#include "vos_shell_int.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_time.h"

namespace {

// ---------- whoami e id ----------

// Chi sta scrivendo in questa sessione: il nome e il suo posto nella tabella degli utenti (-1 = nessun utente: regole, script,
// primo avvio senza password).
struct Who {
  char name[24];
  int  index;
};

void whoIs(uint32_t sess, Who& w) {
  w.name[0] = 0;
  w.index = -1;
  if (sess == SESS_SERIAL) {
    serialUserName(w.name, sizeof w.name);
    w.index = serialUser();
  } else if (sess >= SESS_WEB && sess < SESS_WEB + VOS_MAX_USERS) {      // la pagina: SESS_WEB + indice dell'utente
    int i = (int)(sess - SESS_WEB);
    if (!cfg.users[i].name.empty()) {
      snprintf(w.name, sizeof w.name, "%s", cfg.users[i].name.c_str());
      w.index = i;
    }
  }
  if (!w.name[0]) w.index = -1;
}

void cmdWhoami(uint32_t sess, OutSink& o) {
  Who w;
  whoIs(sess, w);
  o.println(w.index >= 0 ? w.name : tr("sistema"));
}

// Come l'id classico, ma i numeri sono SIMULATI: VesevOS non ha uid veri. uid = 1000 + posto dell'utente (0 senza utente),
// gid = 1000 + ruolo (0 Ospite, 1 Operatore, 2 Admin); il gruppo ha il nome del ruolo. Non dice niente che l'utente non sappia gia.
void cmdId(uint32_t sess, int role, OutSink& o) {
  Who w;
  whoIs(sess, w);
  const char* group = role >= ROLE_ADMIN ? "admin" : role == ROLE_OPER ? "oper" : "guest";
  int uid = w.index >= 0 ? 1000 + w.index : 0;
  int gid = 1000 + (role < 0 ? 0 : role);
  const char* user = w.index >= 0 ? w.name : tr("sistema");
  o.printf("uid=%d(%s) gid=%d(%s) groups=%d(%s)\r\n", uid, user, gid, group, gid, group);
}

// ---------- echo ----------

// Solo il testo: nessuna variabile, nessun apice da togliere, nessuna redirezione (per scrivere un file c'e "write").
void cmdEcho(const Word& a1, const char* line, OutSink& o) {
  bool newline = true;
  const char* text = restFrom(line, 1);
  if (a1 == "-n") {
    newline = false;
    text = restFrom(line, 2);
  }
  o.print(text);
  if (newline) o.println();
}

// ---------- df ----------

// Dimensione in KB scritta come df: numero intero di blocchi da 1K, oppure con -h in K, M o G con un decimale.
void sizeText(char* out, size_t cap, uint32_t kb, bool human) {
  if (!human) snprintf(out, cap, "%lu", (unsigned long)kb);
  else if (kb < 1024UL) snprintf(out, cap, "%luK", (unsigned long)kb);
  else if (kb < 1024UL * 1024UL) snprintf(out, cap, "%lu.%luM", (unsigned long)(kb / 1024UL), (unsigned long)((kb % 1024UL) * 10UL / 1024UL));
  else snprintf(out, cap, "%lu.%luG", (unsigned long)(kb / (1024UL * 1024UL)), (unsigned long)((kb % (1024UL * 1024UL)) * 10UL / (1024UL * 1024UL)));
}

void dfRow(OutSink& o, const char* fs, uint32_t totalKb, uint32_t usedKb, const char* mount, bool human) {
  if (usedKb > totalKb) usedKb = totalKb;
  char total[16], used[16], avail[16], pct[8];
  sizeText(total, sizeof total, totalKb, human);
  sizeText(used, sizeof used, usedKb, human);
  sizeText(avail, sizeof avail, totalKb - usedKb, human);
  if (totalKb) snprintf(pct, sizeof pct, "%lu%%", (unsigned long)(((uint64_t)usedKb * 100 + totalKb - 1) / totalKb));   // per eccesso, come df
  else snprintf(pct, sizeof pct, "-");
  o.printf("%-10s %10s %9s %11s %5s %s\r\n", fs, total, used, avail, pct, mount);
}

void cmdDf(const Word& a1, OutSink& o) {
  bool human = (a1 == "-h");
  if (!a1.empty() && !human) { cmdUsage(o, "df"); return; }
  o.printf("%-10s %10s %9s %11s %5s %s\r\n", tr("Filesystem"), tr("1K-blocchi"), tr("Usati"), tr("Disponibili"), tr("Uso%"), tr("Montato su"));
  dfRow(o, "littlefs", (uint32_t)(drvFsTotal() / 1024), (uint32_t)(drvFsUsed() / 1024), "/", human);
#if VOS_WITH_SD
  uint32_t sdTotal = 0, sdUsed = 0;
  if (sdSpaceKb(sdTotal, sdUsed)) dfRow(o, "microsd", sdTotal, sdUsed, "/sd", human);   // montata adesso; si usa con il comando sd
#endif
}

// ---------- uname ----------

// Senza opzioni scrive la riga di sempre. Con le opzioni come il comando uname classico, anche unite (-sr):
// s nome del sistema, n nome host, r versione di VesevOS, v versione di ESP-IDF, m chip, a tutti insieme.
void cmdUname(const char* line, OutSink& o) {
  if (argAt(line, 1).empty()) {
    o.printf("%s %s (%s, core %s)\r\n", VOS_NAME, VOS_VERSION, VOS_CHIP, drvSdkVersion());
    return;
  }
  bool s = false, n = false, r = false, v = false, m = false;
  for (int i = 1; i <= 6; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w.cut() || w.length() < 2 || w.c_str()[0] != '-') { cmdUsage(o, "uname"); return; }
    for (const char* p = w.c_str() + 1; *p; p++) {
      switch (*p) {
        case 'a': s = n = r = v = m = true; break;
        case 's': s = true; break;
        case 'n': n = true; break;
        case 'r': r = true; break;
        case 'v': v = true; break;
        case 'm': m = true; break;
        default: cmdUsage(o, "uname"); return;
      }
    }
  }
  bool first = true;
  auto put = [&](const char* text) {
    if (!first) o.print(' ');
    o.print(text);
    first = false;
  };
  if (s) put(VOS_NAME);
  if (n) put(cfg.hostname.length() ? cfg.hostname.c_str() : "vesevos");
  if (r) put(VOS_VERSION);
  if (v) put(drvSdkVersion());
  if (m) put(drvChipModel());
  o.println();
}

// ---------- time ----------

// Come il "time" di bash: esegue il comando con la STESSA sessione e lo stesso ruolo, poi scrive il tempo reale in millisecondi
// (misurato con drvMillis). "user" e "sys" non ci sono: FreeRTOS qui non li misura. Non da' permessi: il comando interno passa dal
// controllo della tabella dei ruoli come se fosse scritto da solo. Non si annida. Non alloca niente.
void cmdTime(const char* line, OutSink& o, int role, uint32_t sess) {
  Word inner = argAt(line, 1);
  if (inner.empty()) {
    cmdUsage(o, "time");
    o.println(tr("Per l'ora corrente usa 'date'."));
    return;
  }
  inner.lower();
  canonicalName(inner);
  if (inner == "time") { o.println(tr("time: non si puo annidare")); return; }
  if (inner.cut() || !roleAllows(role, inner, argAt(line, 2))) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }
  unsigned long t0 = drvMillis();
  shellExecSess(restFrom(line, 1), o, role, sess);
  unsigned long ms = drvMillis() - t0;
  o.printf("\r\nreal    %lum%lu.%03lus\r\n", ms / 60000UL, (ms % 60000UL) / 1000UL, ms % 1000UL);
}

}  // namespace

bool shCmdBasic(const Word& c, const Word& a1, const char* line, OutSink& o, int role, uint32_t sess) {
  if (c == "help") {
    if (a1.empty()) cmdHelpList(o, role);
    else if (!cmdHelpOne(o, a1.c_str(), role, false)) o.println(trf("Nessun aiuto per '%s'", a1.c_str()));
  }
  else if (c == "man") {
    if (a1.empty()) cmdUsage(o, "man");
    else if (!cmdHelpOne(o, a1.c_str(), role, true)) o.println(trf("Nessuna voce di manuale per '%s'", a1.c_str()));
  }
  else if (c == "whoami") cmdWhoami(sess, o);
  else if (c == "id") cmdId(sess, role, o);
  else if (c == "echo") cmdEcho(a1, line, o);
  else if (c == "df") cmdDf(a1, o);
  else if (c == "uname") cmdUname(line, o);
  else if (c == "exit") o.println(tr("exit: vale sulla seriale (scritto da solo); dalla pagina usa il menu utente e Logout"));   // sulla seriale lo ferma isLogoutWord prima
  else if (c == "time") cmdTime(line, o, role, sess);
  else if (c == "ping") shPing(line, o, sess);                     // vos_shell_ping.cpp; il ruolo (solo Admin) lo ha gia deciso la tabella dei ruoli
  else return false;
  return true;
}
