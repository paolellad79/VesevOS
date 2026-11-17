// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_funzioni_dev.cpp: funzioni delle APP dev_get(id) e dev_set(id, valore) (spostate da vos_funzioni.cpp).
#include "vos_funzioni_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_FUNZIONI
#include "../third_party/funzioni/src/funzioni.h"
#include <stdio.h>
#include <string.h>
#include "vos_app_rt.h"
#include "../core/vos_errbuf.h"
#include "../devices/vos_io.h"

// dev_get: numero per switch/slider/button/chart, testo per color/text e per i dati di [data]. dev_set: aggiorna cio che la pagina mostra
// (per chart aggiunge un punto). Nessun oggetto con distruttore vivo quando si chiama be_raise.
static char g_ioMsg[160];
static bool useHasDev(const char* nm) {                     // "bmp1.p" (pressione) si autorizza con "bmp1"
  char b[16]; size_t n = strcspn(nm, "."); if (n >= sizeof b) return false;
  memcpy(b, nm, n); b[n] = 0; return useHas(b);
}
// periferica a classi (rele1, led1, pin1, ingresso1, adc1): serve il nome nel use= del manifest. 0 = non e una periferica, 1 = fatto, -1 = errore (testo in g_ioMsg)
static int ioGetFor(const char* nm, long& v) {
  if (!ioHas(nm)) return 0;
  if (!useHasDev(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return -1; }
  int x = 0; ErrBuf e; if (!ioGet(nm, x, e)) { snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str()); return -1; }
  v = x; return 1;
}
static int ioSetFor(const char* nm, long v) {
  if (!ioHas(nm)) return 0;
  if (!useHas(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return -1; }
  ErrBuf e; if (!ioSet(nm, v != 0, ROLE_ADMIN, e)) { snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str()); return -1; }   // il permesso lo ha dato l'Admin installando l'app
  return 1;
}
static int bfDevGet(bvm* vm) {
  funzioniRichiedeFunz(vm, "dev");
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "dev_get(\"id\"): serve il nome di un controllo");
  { long pv = 0; int pr = ioGetFor(be_tostring(vm, 1), pv); if (pr < 0) be_raise(vm, "value_error", g_ioMsg); if (pr > 0) { be_pushint(vm, (bint)pv); be_return(vm); } }
  bool isNum = false, ok; long iv = 0; char sv[RT_TXT + 1];
  ok = rtGet(be_tostring(vm, 1), isNum, iv, sv, sizeof(sv));
  if (!ok) be_raise(vm, "value_error", "id sconosciuto (non e un controllo ne un dato dell'app)");
  if (isNum) be_pushint(vm, (bint)iv); else be_pushstring(vm, sv);
  be_return(vm);
}
static int bfDevSet(bvm* vm) {
  funzioniRichiedeFunz(vm, "dev");
  if (be_top(vm) < 2 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "dev_set(\"id\", valore)");
  bool isNum = false; long iv = 0; const char* sv = NULL; const char* why = "";
  if (be_isint(vm, 2)) { isNum = true; iv = (long)be_toint(vm, 2); }
  else if (be_isreal(vm, 2)) { isNum = true; iv = (long)be_toreal(vm, 2); }
  else if (be_isbool(vm, 2)) { isNum = true; iv = be_tobool(vm, 2) ? 1 : 0; }
  else if (be_isstring(vm, 2)) sv = be_tostring(vm, 2);
  else be_raise(vm, "type_error", "dev_set: il valore deve essere un numero o un testo");
  { int pr = isNum ? ioSetFor(be_tostring(vm, 1), iv) : 0; if (pr < 0) be_raise(vm, "value_error", g_ioMsg); if (pr > 0) be_return_nil(vm); }
  if (!rtSetApp(be_tostring(vm, 1), isNum, iv, sv, &why)) be_raise(vm, "value_error", why);
  be_return_nil(vm);
}

#if VOS_WITH_PWM
static bool g_pwmApp = false, g_pwmAdmin = false;
// pwm("nome", duty [, frequenza]): duty 0-100; la frequenza (10-40000 Hz) solo se il chiamante e Admin. Le app devono avere il nome in use=.
static bool pwmDo(const char* nm, int duty, int hz) {
  if (g_pwmApp && !useHas(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return false; }
  ErrBuf e;
  if (ioPwmSet(nm, hz, duty, g_pwmAdmin ? ROLE_ADMIN : ROLE_OPER, e)) return true;
  snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str());
  return false;
}
static int bfPwm(bvm* vm) {
  funzioniRichiedeFunz(vm, "dev");
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isint(vm, 2)) be_raise(vm, "type_error", "pwm(\"nome\", duty 0-100 [, frequenza Hz])");
  int hz = (be_top(vm) >= 3 && be_isint(vm, 3)) ? (int)be_toint(vm, 3) : 0;
  if (!pwmDo(be_tostring(vm, 1), (int)be_toint(vm, 2), hz)) be_raise(vm, "value_error", g_ioMsg);
  be_return_nil(vm);
}
#endif
#if VOS_WITH_CONTATORE
static bool g_cntApp = false;
// contatore("nome" [, azzera]): impulsi contati; con azzera vero il contatore riparte da 0 dopo la lettura. Le app devono avere il nome in use=.
static bool cntDo(const char* nm, bool azzera, int& n) {
  if (g_cntApp && !useHas(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return false; }
  int mhz = 0;
  if (!ioCntGet(nm, n, mhz)) { snprintf(g_ioMsg, sizeof g_ioMsg, "non e un contatore"); return false; }
  ErrBuf e;
  if (azzera && !ioCntReset(nm, ROLE_OPER, e)) { snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str()); return false; }
  return true;
}
static int bfContatore(bvm* vm) {
  funzioniRichiedeFunz(vm, "dev");
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "contatore(\"nome\" [, azzera])");
  int n = 0;
  if (!cntDo(be_tostring(vm, 1), be_top(vm) >= 2 && be_tobool(vm, 2), n)) be_raise(vm, "value_error", g_ioMsg);
  be_pushint(vm, n);
  be_return(vm);
}
#endif
void funzioniRegistraContatore(bvm* vm, bool app) {
#if VOS_WITH_CONTATORE
  g_cntApp = app;
  be_regfunc(vm, "contatore", bfContatore);
#else
  (void)vm; (void)app;
#endif
}

void funzioniRegistraPwm(bvm* vm, bool app, bool admin) {
#if VOS_WITH_PWM
  g_pwmApp = app; g_pwmAdmin = admin;
  be_regfunc(vm, "pwm", bfPwm);
#else
  (void)vm; (void)app; (void)admin;
#endif
}

void funzioniRegistraDev(bvm* vm) {
  be_regfunc(vm, "dev_get", bfDevGet);
  be_regfunc(vm, "dev_set", bfDevSet);
}
#else
struct bvm;
void funzioniRegistraDev(bvm*) {}
void funzioniRegistraPwm(bvm*, bool, bool) {}
void funzioniRegistraContatore(bvm*, bool) {}
#endif
