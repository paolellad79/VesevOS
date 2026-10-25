// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell.cpp
// Shell: esecuzione di una riga, schermata di benvenuto e riga in digitazione sulla seriale. Scritta senza String sulla riga:
// le parole sono Word (vos_shell_line.h), il resto della riga un puntatore. La guida da seriale e in vos_shell_wiz.cpp.
#include "vos_shell.h"
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "vos_shell_login.h"
#include "vos_shell_sess.h"
#include "vos_shell_prompt.h"
#include "vos_shell_complete.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_strbuf.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_console.h"
#include <ctype.h>

#define SHELL_LINE_MAX 1024                 // riga di comando piu lunga che si esegue (la pagina ne manda al massimo 200)
#define SER_LINE_MAX   200                  // riga in digitazione sulla seriale
static char g_serCmd[24] = "";              // ultimo comando dalla seriale (solo la prima parola: niente password nel who)
static uint32_t g_serCmdT = 0;

bool shx::scanRunning() {
  return netScanRunning();
}

int shx::roleArg(const Word& r) { return r == "admin" ? ROLE_ADMIN : r == "oper" ? ROLE_OPER : r == "guest" ? ROLE_GUEST : -1; }

void shellExec(const char* lineIn, OutSink& o, int role) { shellExecSess(lineIn, o, role, SESS_NONE); }

void shellExecSess(const char* lineIn, OutSink& o, int role, uint32_t sess) {
  if (!lineIn) return;
  while (isspace((unsigned char)*lineIn)) lineIn++;
  size_t n = strlen(lineIn);
  while (n && isspace((unsigned char)lineIn[n - 1])) n--;
  if (n == 0) return;
  HeapBuf copy(SHELL_LINE_MAX);             // la riga senza spazi in coda: una copia sull'heap solo se serve
  const char* line = lineIn;
  if (lineIn[n] != 0) {
    if (!copy.add(lineIn, n)) { o.println(copy.oom() ? tr("Memoria insufficiente") : tr("Comando troppo lungo")); return; }
    line = copy.c_str();
  } else if (n > SHELL_LINE_MAX) { o.println(tr("Comando troppo lungo")); return; }
  Word c = argAt(line, 0);
  c.lower();
  canonicalName(c);                                      // aereo, licenza, users, watchdog, locale, restart, logout, ?: i vecchi nomi diventano quelli ufficiali (vos_shell_cmdtab.cpp)
  Word a1 = argAt(line, 1);
  if (c.cut() || a1.cut()) { o.println(tr("Comando troppo lungo")); return; }

  if (role < 0 && authIsSet()) { o.println(tr("Non autorizzato. Fai login.")); return; }
  if (role < 0) role = ROLE_ADMIN;                       // nessuna password ancora: primo avvio
  if (!roleAllows(role, c, a1)) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }   // tabella unica: vos_shell_roles.cpp

  ShellSess ss, before;                                  // cartella corrente di questa sessione: copia, poi si risalva se e cambiata
  shellSessLoad(sess, ss);
  before = ss;
  bool done = shCmdBasic(c, a1, line, o, role, sess) || shCmdFiles(c, a1, line, o, role, ss) || shCmdArchive(c, line, o, ss, sess) || shCmdSys(c, a1, line, o, role) || shCmdLed(c, a1, line, o, role) || shCmdNet(c, a1, line, o, role) || shCmdAdmin(c, a1, line, o, role)
              || shCmdCpu(c, a1, line, o, role) || shCmdVesevos(c, a1, line, o, role, sess);
  if (strcmp(ss.cwd, before.cwd) != 0 || strcmp(ss.prev, before.prev) != 0) shellSessStore(sess, ss);
  if (!done) o.println(tr("Comando sconosciuto. Scrivi 'help'."));
}

static uint32_t g_webIp = 0;                              // vale solo dentro shellExecWeb (le richieste della pagina arrivano una alla volta)

uint32_t shx::shellClientIp() { return g_webIp; }

void shellExecWeb(const char* line, const char* secret, OutSink& o, int role, uint32_t sess, char* ask, size_t askCap, uint32_t clientIp) {
  askWebBegin(secret);
  g_webIp = clientIp;
  shellExecSess(line, o, role, sess);
  g_webIp = 0;
  askWebEnd(ask, askCap);
}

// ---------- schermata di benvenuto (solo ASCII, 68 colonne) ----------
#define WW 68
// una riga del riquadro: s e la parte a sinistra (con il bordo sinistro), si riempie di spazi e si chiude con '|'
static void padBox(OutSink& o, const char* s) {
  char l[WW + 1];
  size_t n = strlen(s);
  if (n > WW - 1) n = WW - 1;
  memcpy(l, s, n);
  while (n < WW - 1) l[n++] = ' ';
  l[n++] = '|'; l[n] = 0;
  o.println(l);
}
static void wBar(OutSink& o) {
  char b[WW + 1];
  b[0] = '+'; memset(b + 1, '-', WW - 2); b[WW - 1] = '+'; b[WW] = 0;
  o.println(b);
}
// testo di n caratteri, tagliato alla larghezza del riquadro
static void wLine(OutSink& o, const char* t, size_t n) {
  char l[WW + 1];
  if (n > WW - 4) n = WW - 4;
  memcpy(l, "|  ", 3); memcpy(l + 3, t, n); l[3 + n] = 0;
  padBox(o, l);
}
static void wLine(OutSink& o, const char* t) { wLine(o, t, strlen(t)); }
// testo lungo: a capo sulle parole
static void wPara(OutSink& o, const char* t) {
  size_t n = strlen(t);
  while (n) {
    if (n <= WW - 4) { wLine(o, t, n); break; }
    int cut = -1;                                      // ultimo spazio entro la larghezza
    for (int i = WW - 4; i >= 0; i--) if (t[i] == ' ') { cut = i; break; }
    if (cut <= 0) cut = WW - 4;
    wLine(o, t, (size_t)cut);
    t += cut + 1; n -= (size_t)cut + 1;
  }
}

static char g_lcodes[9][LANG_CODE_BUF], g_lnames[9][LANG_NAME_BUF];
static int g_ln = 0;

static void langMenu(OutSink& o) {
  g_ln = langList(g_lcodes, g_lnames, 9);
  char buf[320];
  StrBuf l(buf, sizeof buf);
  l.add(tr("Lingua:")).add(' ');
  for (int i = 0; i < g_ln; i++) l.addf(" [%d] %s%s ", i + 1, g_lnames[i], cfg.lang == g_lcodes[i] ? " *" : "");
  l.add(' ').add(tr("(premi il numero)"));
  wPara(o, l.c_str());
}

void shellWelcome(OutSink& o, bool full) {
  char buf[200];
  o.println();
  wBar(o);
  snprintf(buf, sizeof buf, "|   /\\      %s %s", VOS_NAME, VOS_VERSION); padBox(o, buf);
#define VOS_STR_(x) #x
#define VOS_STR(x) VOS_STR_(x)
#if VOS_HAS_PSRAM
  padBox(o, "|  /  \\     " VOS_BOARD " - " VOS_STR(VOS_FLASH_MB) " MB flash, " VOS_STR(VOS_PSRAM_MB) " MB PSRAM");
#else
  padBox(o, "|  /  \\     " VOS_BOARD " - " VOS_STR(VOS_FLASH_MB) " MB flash");
#endif
  padBox(o, "| /____\\    github.com/paolellad79/VesevOS");
  wBar(o);
  if (full) {
    wLine(o, tr("BENVENUTO! Questa scheda non e ancora configurata."));
    wLine(o, "");
    wLine(o, tr("1. Collegati alla rete Wi-Fi:"));
    snprintf(buf, sizeof buf, "     %s      %s", tr("Nome:"), cfg.apSsid.c_str()); wLine(o, buf);
    snprintf(buf, sizeof buf, "     %s  %s", tr("Password:"), cfg.apPass.c_str()); wLine(o, buf);
    snprintf(buf, sizeof buf, "%s      http://192.168.4.1", tr("2. Apri:")); wLine(o, buf);
    snprintf(buf, sizeof buf, "   %s", tr("(di solito la pagina si apre da sola)")); wLine(o, buf);
    wLine(o, tr("3. Segui la configurazione guidata."));
    wLine(o, "");
    wLine(o, tr("Il QR per il telefono e nella pagina Home."));
    wBar(o);
    wPara(o, tr("Perche una password diversa per ogni scheda? Lo chiedono le leggi sulla sicurezza dei dispositivi: UE (RED/EN 18031, Cyber Resilience Act) e Regno Unito (PSTI). Niente password uguali."));
    wBar(o);
    wLine(o, tr("Password persa?  Tieni premuto BOOT 8 s  -> la riscrivo qui"));
    wLine(o, tr("Da capo?         Tieni premuto BOOT 20 s -> fabbrica"));
    wLine(o, tr("Aiuto: http://192.168.4.1/#aiuto   Comandi: scrivi  help"));
  } else {
    snprintf(buf, sizeof buf, "%s %s  -  %s (%s.local)", VOS_NAME, VOS_VERSION, cfg.hostname.c_str(), cfg.hostname.c_str()); wLine(o, buf);
    char ip[16]; netIpTo(ip, sizeof ip);
    if (netState() == NET_CLIENT_OK) wLine(o, trf("Wi-Fi casa: %s  %s  %d dBm", drvWifiSsid().s, ip, (int)drvWifiRssi()).c_str());
    else wLine(o, trf("Hotspot: %s  %s", cfg.apSsid.c_str(), ip).c_str());
    char now[32]; timeNowFmt(now, sizeof now);
    wLine(o, trf("Paese: %s   Ora: %s", cfg.country.length() ? cfg.country.c_str() : "--", now).c_str());
    int red = auditCount(AUD_RED), yel = auditCount(AUD_YELLOW);
    wLine(o, (red || yel) ? trf("Allarmi: %d rossi, %d gialli -> scrivi audit", red, yel).c_str() : tr("Allarmi: nessuno"));
    wLine(o, trf("Avvio: %s (n. %lu)", sysResetText(), (unsigned long)sysBootCount()).c_str());
    const char* fp = tlsFingerprint();
    if (cfg.https && *fp) { snprintf(buf, sizeof buf, "%s %.47s...", tr("Impronta HTTPS:"), fp); wPara(o, buf); }
    wLine(o, trf("Pagina: https://%s   Comandi: help", ip).c_str());
  }
  wBar(o);
  langMenu(o);
  wBar(o);
}

// ---------- shell seriale ----------
static char g_buf[SER_LINE_MAX + 1];       // riga in digitazione
static size_t g_bufLen = 0;
static uint8_t g_esc = 0;                          // dentro una sequenza ESC (tasti freccia...)
static bool g_fullWelcome = false;
static bool g_tabPrev = false;                     // l'ultimo tasto era un Tab (il secondo Tab di fila elenca i candidati)
static ShellCompletion g_tab;                      // risultato del completamento (la seriale e una sola: un oggetto fisso, niente heap)

static uint32_t g_lastAct = 0;                     // ultimo tasto (per l'uscita automatica)

// La seriale chiede il login solo se: c'e un amministratore con password, l'interruttore e acceso, e nessuno e ancora entrato
static bool serialLocked() { return serialLoginRequired() && !serialAuthed(); }

// Il ruolo con cui girano i comandi della seriale: quello di chi e entrato (amministratore se il login non serve)
static int serRole() { int r = serialRole(); return r < 0 ? ROLE_GUEST : r; }

// Origine delle modifiche di configurazione: "seriale" e, se c'e un login, il nome di chi e entrato
static void setOrigin() {
  char user[24];
  serialUserName(user, sizeof user);
  if (user[0]) cfgSetOriginf("seriale %s", user); else cfgSetOrigin("seriale");
}

// Il prompt della seriale: "host login: " (o il passo del login) finche e chiusa, poi utente@hostname:cartella$ (utente e ruolo veri)
static void prompt() {
  if (serialLocked()) { loginPrompt(serOut()); return; }
  char user[24], p[SHELL_PROMPT_MAX];
  serialUserName(user, sizeof user);
  shellPrompt(SESS_SERIAL, user, serRole(), p, sizeof p);
  serOut().print(p);
}

// Fine della sessione (logout, exit, Ctrl+D, inattivita): niente resta in memoria di chi c'era. Poi si torna a "host login:".
static void endSession(const char* why) {
  char user[24];
  serialUserName(user, sizeof user);
  if (user[0]) auditEvent(AUD_INFO, "serlogin", trf("Seriale: uscita di %s", user).c_str());
  serialClose();
  shellSessDrop(SESS_SERIAL);            // cartella corrente e precedente
  askClear();                            // domanda in sospeso (conferma o password)
  wzAbort();                             // guida di configurazione a meta
  loginReset();
  g_serCmd[0] = 0;
  g_bufLen = 0; wipeBytes(g_buf, sizeof g_buf);
  serOut().println(why);
}

static bool isLogoutWord(const char* l) {
  Word w = argAt(l, 0);
  return w == "logout" || w == "exit";
}

// Tab sulla riga in digitazione, come in bash: scrive i caratteri mancanti; al secondo Tab di fila, se restano piu candidati, li elenca
// e ristampa prompt e riga. Su riga vuota non fa niente. L'eco dei caratteri aggiunti segue serial echo.
static void serialTab(bool second) {
  if (!shellComplete(SESS_SERIAL, serRole(), g_buf, (size_t)SER_LINE_MAX - g_bufLen, false, g_tab)) return;
  size_t add = strlen(g_tab.add);
  if (add) {
    memcpy(g_buf + g_bufLen, g_tab.add, add + 1);
    g_bufLen += add;
    if (cfg.serEcho) serOut().print(g_tab.add);
  } else if (second && g_tab.n > 1) {
    serOut().println();
    shellCompletePrintList(serOut(), g_tab);
    prompt();
    if (cfg.serEcho) serOut().print(g_buf);
  }
}

void shellSerialWho(OutSink& o) {
  if (!cfg.serIn) { o.print(tr("seriale: comandi spenti")); o.print("\n"); return; }
  char user[24];
  serialUserName(user, sizeof user);
  char opened[112];
  snprintf(opened, sizeof opened, "%s", user[0] ? trf("sbloccata da %s", user).c_str() : tr("sbloccata"));   // copiato subito: il testo di trf vive solo fino a fine istruzione
  const char* st = serialLocked() ? tr("bloccata (serve il login)") : opened;
  if (!g_serCmd[0]) o.print(trf("seriale (USB): %s", st));
  else o.print(trf("seriale (USB): %s, ultimo comando '%s' %lu s fa", st, g_serCmd, (unsigned long)((drvMillis() - g_serCmdT) / 1000UL)));
  o.print("\n");
}

void shellSerialShowPass() { g_fullWelcome = true; }   // tasto BOOT 8 s: rimostra la password dell'hotspot

bool shellWizardActive() { return wzActive(); }

void shellSerialPoll() {
  static bool first = true, wasOpen = false;
  static uint32_t lastBanner = 0, lastKey = 0, closedAt = 0;
  static bool closedLong = true;
  uint32_t now = drvMillis();
  bool open = drvConIsOpen();                      // true quando il monitor seriale e aperto sul PC
  // anti-ripetizione: il segnale "aperto" sull'USB puo cadere e risalire piu volte in pochi decimi di secondo
  // (monitor che si riapre, velocita diversa): conta come nuova apertura solo dopo 2 secondi di chiusura vera
  if (!open) { if (!closedAt) closedAt = now ? now : 1; else if (now - closedAt >= 2000UL) closedLong = true; }
  else closedAt = 0;
  bool show = first || g_fullWelcome;
  if (!cfg.setupDone && open) {
    // finche la configurazione guidata non e finita il benvenuto (con la password dell'hotspot) NON sparisce:
    // si rimostra a ogni apertura del monitor e, se nessuno scrive, ogni 30 secondi
    if (!wasOpen && closedLong && (lastBanner == 0 || now - lastBanner >= 5000UL)) show = true;
    else if (now - lastBanner >= 30000UL && now - lastKey >= 30000UL) show = true;
  }
  if (open && !wasOpen) closedLong = false;
  wasOpen = open;
  if (show) {
    bool full = !cfg.setupDone || g_fullWelcome;
    first = false; g_fullWelcome = false; if (open) lastBanner = now;   // il benvenuto mostrato a monitor chiuso non conta
    if (cfg.serBanner || !cfg.setupDone) shellWelcome(serOut(), full);
    if (!authIsSet()) serOut().println(tr("Nessuna password: impostala dalla pagina web (primo accesso)."));
    if (!cfg.setupDone) { serOut().println(tr("Configurazione non finita: questo messaggio resta finche la guida non e completata (pagina web oppure seriale).")); serOut().println(tr("Per configurare da questa seriale scrivi: setup")); }
    prompt();
  }
  bool passChecked = false;
  if (askExpired(now)) { askCancel(serOut(), true); prompt(); }   // una domanda (conferma o password) rimasta senza risposta
  if (serialLocked() && loginTick(serOut(), now)) { g_bufLen = 0; wipeBytes(g_buf, sizeof g_buf); }   // fine dell'attesa dopo un errore, o passo scaduto
  if (cfg.serialIdleMin && serialAuthed() && now - g_lastAct >= (uint32_t)cfg.serialIdleMin * 60000UL) {
    serOut().println();
    endSession(tr("Sessione chiusa per inattivita."));     // come TMOUT di bash
    prompt();
  }
  while (drvConAvailable()) {
    char ch = (char)drvConRead();
    if (!cfg.serIn) continue;                      // seriale impostata "senza comandi": si butta quello che arriva
    if (serialLocked() && loginBusy()) continue;   // attesa dopo un errore di login: i tasti si buttano
    lastKey = now; g_lastAct = now;
    bool wasTab = g_tabPrev;
    g_tabPrev = false;                             // qualunque altro tasto azzera il "secondo Tab"
    int erase; char typed;
    if (ch == '\t' && !g_esc) {
      if (!serialLocked() && !wzActive() && !askActive()) { serialTab(wasTab); g_tabPrev = true; }   // con la password, la guida o una domanda il Tab non fa niente
      continue;
    }
    if (ch == 3 && askActive()) { g_bufLen = 0; g_buf[0] = 0; askCancel(serOut(), false); prompt(); continue; }   // Ctrl+C lascia perdere la domanda
    if (g_esc || ch == 27) { editKey(g_buf, g_bufLen, SER_LINE_MAX, g_esc, ch, erase, typed); continue; }   // frecce e simili: scartate, non finiscono nella riga
    if (ch == 4 && !g_bufLen && !serialLocked() && !wzActive() && !askActive()) {   // Ctrl+D a riga vuota = exit, come la shell classica
      serOut().println();
      endSession(tr("Uscito."));
      prompt();
      continue;
    }
    // tasto numerico a riga vuota: cambia lingua (solo con la seriale sbloccata, mai durante il login)
    if (!g_bufLen && ch >= '1' && ch <= '9' && !serialLocked() && !wzActive() && !askActive()) {   // (con una domanda in sospeso il numero e parte della risposta)
      int i = ch - '1';
      if (!g_ln) g_ln = langList(g_lcodes, g_lnames, 9);
      if (i < g_ln) {
        ErrBuf err;
        if (langSet(g_lcodes[i], err)) { serOut().println(); serOut().println(trf("Lingua impostata: %s", g_lnames[i])); shellWelcome(serOut(), !cfg.setupDone); }
        else serOut().println(err.c_str());
        prompt();
        continue;
      }
    }
    if (ch == '\r' || ch == '\n') {
      char l[SER_LINE_MAX + 1];
      memcpy(l, g_buf, g_bufLen + 1); size_t ll = g_bufLen; g_bufLen = 0; g_buf[0] = 0;
      if (ll == 0 && ch == '\n') continue;
      serOut().println();
      if (askActive() && !serialLocked()) {   // risposta a una conferma o a una richiesta di password: non e un comando
        for (size_t k = 0; k < ll; k++) g_buf[k] = 0;       // la password non resta nel buffer di digitazione
        setOrigin();
        askLine(serOut(), l, shellExecSess);
        if (!askActive()) prompt();
        continue;
      }
      if (wzActive() && !serialLocked()) { setOrigin(); wzLine(serOut(), l); prompt(); continue; }   // risposta alla guida
      if (ll == 0 && !cfg.setupDone) { g_fullWelcome = true; continue; }   // Invio a vuoto: rimostra il benvenuto
      if (serialLocked()) {                                                     // login: utente, password, codice MFA (vos_shell_login.cpp)
        bool entered = loginLine(serOut(), l);                                  // la riga viene cancellata dopo l'uso
        wipeBytes(g_buf, sizeof g_buf);
        if (entered) { g_lastAct = drvMillis(); prompt(); }                     // se non e entrato, loginLine ha gia scritto la richiesta del passo dopo
        passChecked = true;
      } else if (isLogoutWord(l)) endSession(tr("Uscito."));                   // poi il prompt: "host login:" se serve il login
      else { strncpy(g_serCmd, argAt(l, 0).c_str(), sizeof g_serCmd - 1); g_serCmdT = drvMillis(); setOrigin(); shellExecSess(l, serOut(), serRole() < 0 ? ROLE_GUEST : serRole(), SESS_SERIAL); }
      if (!askActive() && !passChecked) prompt();            // se il comando ha fatto una domanda, il prompt e la domanda
      wipeBytes(l, sizeof l);
      if (passChecked) break;   // un solo controllo password (circa 1 s) per giro di loop: le altre righe restano nel buffer della seriale e il watchdog non scatta
    } else {
      editKey(g_buf, g_bufLen, SER_LINE_MAX, g_esc, ch, erase, typed);          // Backspace, Ctrl+U, Ctrl+C e lettere
      bool echo = serialLocked() ? loginEchoes() : !askIsSecret();              // niente eco di password e codice MFA (ne di quelle chiesta da passwd, user add, wifi set); "\b \b" cancella anche sullo schermo
      if (echo && cfg.serEcho) {
        for (int k = 0; k < erase; k++) serOut().print("\b \b");
        if (typed) serOut().print(typed);
      }
    }
  }
}
