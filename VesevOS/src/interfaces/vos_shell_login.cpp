// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_login.cpp
// Il login della seriale (vedi vos_shell_login.h). La password la controlla security (serialPasswordCheck), il codice MFA
// packages/vos_mfa (mfaLoginCheck): le stesse funzioni e le stesse regole del web. Qui c'e solo la conversazione col terminale.
#include "vos_shell_login.h"
#include "vos_shell_apply.h"
#include "../core/vos_i18n.h"
#include "../core/vos_config.h"
#include "../core/vos_audit.h"
#include "../core/vos_time.h"
#include "../security/vos_auth.h"
#include "../packages/vos_mfa.h"
#include "../drivers/vos_drv_time.h"

using namespace shx;

#define LOGIN_STEP_MS   60000UL     // tempo per ogni passo
#define LOGIN_BASE_MS   2000UL      // attesa dopo un errore...
#define LOGIN_STEP_ADD  1000UL      // ... piu un secondo per ogni errore di fila, fino a 3 in piu
#define LOGIN_NAME_MAX  24          // un nome vero ha al massimo 20 caratteri

namespace {

enum Step : uint8_t { S_NAME, S_PASS, S_MFA, S_WAIT };

Step     g_step = S_NAME;
char     g_name[LOGIN_NAME_MAX + 1];
char     g_tok[MFA_TOK_BUF];        // gettone MFA di questo accesso
int      g_idx = -1;                // utente la cui password era giusta (solo nel passo MFA)
uint32_t g_since = 0;               // inizio del passo attuale
uint32_t g_until = 0;               // fine dell'attesa dopo un errore
uint8_t  g_run = 0;                 // errori di fila (per l'attesa crescente)
bool     g_blocked = false;         // l'ultimo errore ha fatto scattare il blocco

void clearState() {
  shx::wipeBytes(g_name, sizeof g_name);
  shx::wipeBytes(g_tok, sizeof g_tok);
  g_idx = -1;
}

void say(OutSink& o, const char* text) { o.println(text); }

void sayBlocked(OutSink& o, uint32_t wait) {
  o.println(trf("Troppi errori: la seriale e bloccata, riprova tra %lu secondi", (unsigned long)wait));
}

// Il nome da scrivere nel registro: solo se l'utente esiste (un nome sbagliato potrebbe essere una password scritta nel posto sbagliato)
const char* logName(const char* typed) {
  int i = userFind(typed);
  return i >= 0 ? cfg.users[i].name.c_str() : tr("(sconosciuto)");
}

// Un errore (utente, password o codice): conta nel blocco condiviso, va nel registro, poi si aspetta e si dice sempre la stessa cosa.
// t0 = quando e arrivata la riga: l'attesa parte da li, cosi dura uguale anche se la verifica della password ha richiesto del tempo.
void fail(uint32_t t0, bool mfaStep) {
  g_blocked = authFailIp(AUTH_IP_SERIAL, mfaStep ? "seriale: codice MFA errato" : "seriale: password errata");
  auditEvent(AUD_INFO, "serlogin", trf("Seriale: accesso non riuscito (utente %s, %s)", logName(g_name), mfaStep ? tr("codice MFA") : tr("utente o password")).c_str());
  uint8_t extra = g_run > 3 ? 3 : g_run;
  if (g_run < 250) g_run++;
  g_until = t0 + LOGIN_BASE_MS + LOGIN_STEP_ADD * extra;
  if (g_blocked) g_run = 0;                         // il blocco fa da attesa lunga: si riparte da capo
  clearState();
  g_step = S_WAIT;
}

// Password (e codice) giusti: la sessione si apre.
bool success(OutSink& o, int idx) {
  authIpOk(AUTH_IP_SERIAL);
  g_run = 0;
  if (mfaOn(idx)) cfgSave();                        // il contatore MFA e i codici di recupero usati non devono tornare indietro
  serialOpen(idx);
  auditEvent(AUD_INFO, "serlogin", trf("Seriale: accesso di %s", cfg.users[idx].name.c_str()).c_str());
  clearState();
  g_step = S_NAME;
  say(o, tr("Accesso eseguito."));
  return true;
}

// Il passo del codice MFA, dopo la password giusta.
void startMfa(OutSink& o, int idx) {
  if (!mfaLoginTokenSerial(idx, g_tok)) { fail(drvMillis(), true); return; }
  g_idx = idx;
  g_step = S_MFA;
  g_since = drvMillis();
  if (!timeValid()) say(o, tr("L'ora della scheda non e sincronizzata: non posso controllare il codice a 6 cifre. Scrivi un codice di recupero (XXXX-XXXX)."));
  loginPrompt(o);
}

}  // namespace

void shx::wipeBytes(void* p, size_t n) {
  volatile char* v = (volatile char*)p;
  while (n--) *v++ = 0;
}

void shx::loginReset() {
  clearState();
  g_step = S_NAME;
}

bool shx::loginEchoes() { return g_step == S_NAME; }
bool shx::loginBusy() { return g_step == S_WAIT; }

void shx::loginPrompt(OutSink& o) {
  if (g_step == S_PASS) { o.print(tr("Password:")); o.print(" "); return; }
  if (g_step == S_MFA) { o.print(tr("Codice MFA:")); o.print(" "); return; }
  o.print(cfg.hostname.length() ? cfg.hostname.c_str() : "vesevos");
  o.print(" login: ");
}

bool shx::loginLine(OutSink& o, char* line) {
  uint32_t t0 = drvMillis();
  uint32_t wait = 0;
  bool done = false;
  switch (g_step) {
    case S_NAME:
      if (line[0] == 0) break;                                   // Invio a vuoto: si richiede il nome, come la shell classica
      snprintf(g_name, sizeof g_name, "%s", line);
      if (authIpBlocked(AUTH_IP_SERIAL, wait)) {                 // bloccata: lo dice e non prova nemmeno la verifica
        sayBlocked(o, wait);
        clearState();
        break;
      }
      g_step = S_PASS;
      g_since = t0;
      break;
    case S_PASS: {
      if (authIpBlocked(AUTH_IP_SERIAL, wait)) { sayBlocked(o, wait); clearState(); g_step = S_NAME; break; }
      int idx = serialPasswordCheck(g_name, line);               // stesso lavoro per un nome che non esiste
      if (idx < 0) { fail(t0, false); break; }
      if (mfaOn(idx)) { startMfa(o, idx); wipeBytes(line, strlen(line)); return false; }
      done = success(o, idx);
      break;
    }
    case S_MFA: {
      int who = -1;
      ErrBuf e;
      int res = mfaLoginCheck(g_tok, line, 0, who, e);           // stessa funzione del web: 6 cifre o codice di recupero usa e getta
      if (res == 0 && who == g_idx) done = success(o, who);
      else fail(t0, true);
      break;
    }
    case S_WAIT:
      break;
  }
  wipeBytes(line, strlen(line));
  if (!done && g_step != S_WAIT) loginPrompt(o);
  return done;
}

bool shx::loginTick(OutSink& o, uint32_t now) {
  if (g_step == S_WAIT && (int32_t)(now - g_until) >= 0) {
    uint32_t wait = 0;
    o.println();
    o.println(tr("Login errato"));
    if (g_blocked && authIpBlocked(AUTH_IP_SERIAL, wait)) sayBlocked(o, wait);
    g_blocked = false;
    g_step = S_NAME;
    loginPrompt(o);
    return true;
  }
  if ((g_step == S_PASS || g_step == S_MFA) && now - g_since >= LOGIN_STEP_MS) {
    clearState();
    g_step = S_NAME;
    o.println();
    o.println(tr("Tempo scaduto"));
    loginPrompt(o);
    return true;
  }
  return false;
}
