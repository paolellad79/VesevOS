// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_ask.cpp
// Conferme e richieste di password della shell (vedi vos_shell_ask.h per il perche).
#include "vos_shell_line.h"
#include "vos_shell_apply.h"
#include "../core/vos_i18n.h"
#include "../core/vos_serial.h"
#include "../core/vos_common.h"
#include "../security/vos_auth.h"
#include "vos_shell_sess.h"
#include "vos_shell_ask.h"
#include "../drivers/vos_drv_time.h"

using namespace shx;

#define ASK_LINE_MAX    208        // riga da rieseguire dopo una conferma (la seriale ne accetta 200, piu " yes")
#define ASK_PASS_MAX    72         // una password (al massimo 63 caratteri) e il suo zero
#define ASK_NAME_MAX    40         // nome utente o rete Wi-Fi (al massimo 32)
#define ASK_TIMEOUT_MS  60000UL    // una domanda in sospeso sulla seriale scade dopo un minuto

namespace {

enum Kind : uint8_t { K_NONE, K_YESNO, K_PASS, K_PASS_AGAIN };

// La domanda in sospeso sulla seriale (e una sola, come la seriale). Niente heap.
Kind      g_kind = K_NONE;
SecretFor g_what = SECRET_PASSWD;
bool      g_twice = false;
int       g_role = 0;
uint32_t  g_since = 0;
char      g_line[ASK_LINE_MAX];     // YESNO: il comando da rieseguire
char      g_name[ASK_NAME_MAX];     // PASS: utente o rete
char      g_first[ASK_PASS_MAX];    // PASS_AGAIN: la prima password scritta

// Lo stato della pagina: vale solo dentro una chiamata di shellExecWeb (le richieste della pagina arrivano una alla volta).
const char* g_webSecret = nullptr;
uint8_t     g_webAsk = 0;           // 0 niente, 1 conferma, 2 password, 3 password due volte

// Cancella davvero: volatile impedisce al compilatore di togliere la scrittura "inutile" prima della fine della variabile.
void wipe(char* p, size_t n) {
  volatile char* v = p;
  while (n--) *v++ = 0;
}

void reset() {
  g_kind = K_NONE;
  wipe(g_first, sizeof g_first);
  wipe(g_line, sizeof g_line);
  wipe(g_name, sizeof g_name);
}

bool sayYes(const char* a) {
  Word w(a, strlen(a));
  w.lower();
  return w == "y" || w == "yes" || w == "s" || w == "si";
}

void ask(OutSink& o, const char* text) { o.print(text); o.print(" "); }

// La password e arrivata: la si usa e basta.
void applySecret(OutSink& o, SecretFor what, const char* name, int role, const char* pass) {
  switch (what) {
    case SECRET_PASSWD:   shPasswdApply(name, pass, o); break;
    case SECRET_USER_ADD: shUserAddApply(name, role, pass, o); break;
    case SECRET_WIFI_SET: shWifiSetApply(name, pass, o); break;
    case SECRET_BACKUP_CREATE: case SECRET_BACKUP_RESTORE: case SECRET_BACKUP_RESTORE_YES: shBackupSecret(what, name, pass, o); break;
  }
}

}  // namespace

bool shx::askConfirmed(const char* line) {
  size_t n = strlen(line);
  while (n && line[n - 1] == ' ') n--;
  return n >= 3 && !strncmp(line + n - 3, "yes", 3) && (n == 3 || line[n - 4] == ' ');
}

void shx::askConfirm(OutSink& o, const char* line, const char* question) {
  if (serIsOut(o) && strlen(line) < ASK_LINE_MAX - 5) {
    reset();
    snprintf(g_line, sizeof g_line, "%s yes", line);
    g_kind = K_YESNO;
    g_since = drvMillis();
    ask(o, question);
    o.print("(y/n):");
    return;
  }
  // pagina (o altro): niente domanda in sospeso. Si dice come confermare; la pagina chiede con una finestra e rimanda il comando con "yes"
  o.println(question);
  o.println(trf("Per confermare scrivi: %s yes", line));
  g_webAsk = 1;
}

const char* shx::askSecretGiven(OutSink& o) { return serIsOut(o) ? nullptr : g_webSecret; }

void shx::askSecret(OutSink& o, SecretFor what, const char* name, int role, bool twice) {
  if (serIsOut(o)) {
    reset();
    g_kind = K_PASS; g_what = what; g_twice = twice; g_role = role; g_since = drvMillis();
    snprintf(g_name, sizeof g_name, "%s", name);
    if (what == SECRET_WIFI_SET) ask(o, trf("Password della Wi-Fi '%s' (Invio = rete aperta; non si vede mentre scrivi):", g_name));
    else if (what == SECRET_BACKUP_CREATE) ask(o, trf("Frase per cifrare il backup '%s' (almeno 10 caratteri; non si vede mentre scrivi):", g_name));
    else if (what == SECRET_BACKUP_RESTORE || what == SECRET_BACKUP_RESTORE_YES) ask(o, trf("Frase del backup '%s' (non si vede mentre scrivi):", g_name));
    else ask(o, trf("Password per '%s' (almeno 6 caratteri; non si vede mentre scrivi):", g_name));
    return;
  }
  bool backup = what == SECRET_BACKUP_CREATE || what == SECRET_BACKUP_RESTORE || what == SECRET_BACKUP_RESTORE_YES;
  o.println(backup ? tr("Scrivi la frase del backup: la pagina la chiede senza mostrarla e non la salva.")
                   : tr("Scrivi la password: la pagina la chiede senza mostrarla e non la salva."));
  g_webAsk = twice ? 3 : 2;
}

bool shx::askActive() { return g_kind != K_NONE; }
bool shx::askIsSecret() { return g_kind == K_PASS || g_kind == K_PASS_AGAIN; }

bool shx::askExpired(uint32_t nowMs) { return g_kind != K_NONE && nowMs - g_since >= ASK_TIMEOUT_MS; }

void shx::askClear() { reset(); }

void shx::askCancel(OutSink& o, bool timeout) {
  if (g_kind == K_NONE) return;
  reset();
  o.println();
  o.println(timeout ? tr("Troppo tempo senza risposta: annullato") : tr("Annullato"));
}

void shx::askLine(OutSink& o, char* answer, AskRunFn run) {
  Kind k = g_kind;
  if (k == K_YESNO) {
    char cmd[ASK_LINE_MAX];
    memcpy(cmd, g_line, sizeof cmd);
    reset();
    int role = serialRole();                  // il ruolo di chi e entrato dalla seriale: la conferma non alza i permessi
    if (sayYes(answer)) run(cmd, o, role < 0 ? ROLE_GUEST : role, SESS_SERIAL);   // la riga con " yes" in fondo: questa volta esegue
    else o.println(tr("Annullato"));
  } else if (k == K_PASS && g_twice) {
    snprintf(g_first, sizeof g_first, "%s", answer);
    g_kind = K_PASS_AGAIN;
    g_since = drvMillis();
    ask(o, tr("Ripeti la password:"));
  } else {                                    // l'unica (K_PASS) oppure la ripetizione (K_PASS_AGAIN)
    bool same = (k == K_PASS) || !strcmp(g_first, answer);
    SecretFor what = g_what;
    int role = g_role;
    char name[ASK_NAME_MAX];
    memcpy(name, g_name, sizeof name);
    reset();                                  // la domanda e chiusa prima di usare la risposta
    if (same) applySecret(o, what, name, role, answer);
    else o.println(tr("Le password non coincidono: non cambia niente"));
    wipe(name, sizeof name);
  }
  wipe(answer, strlen(answer));
}

void shx::askWebBegin(const char* secret) {
  g_webSecret = secret;
  g_webAsk = 0;
}

void shx::askWebEnd(char* kind, size_t cap) {
  const char* k = g_webAsk == 1 ? "confirm" : g_webAsk == 2 ? "secret" : g_webAsk == 3 ? "secret2" : "";
  snprintf(kind, cap, "%s", k);
  g_webSecret = nullptr;
  g_webAsk = 0;
}
