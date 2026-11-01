// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_cmd.cpp: argomenti dei COMANDI dei driver (vedi vos_drvcmd.cpp e docs/progetto/piano-comandi-driver-berry.md).
//
//   cmd()   ->  il sottocomando scelto (testo)
//   args()  ->  la lista delle parole dopo il sottocomando (testi, gia controllati); lista vuota se non ce ne sono
//
// REGOLE
//   - Gli argomenti arrivano come LISTA di parole, mai come riga da valutare: il codice Berry non li esegue.
//   - Limiti (berryCmdArgsOk): massimo 8 parole, 96 caratteri ciascuna, 300 in tutto, solo ASCII stampabile (32-126).
//   - Una sola esecuzione di Berry alla volta: la copia degli argomenti e unica (cmdCallSet si chiama quando il "busy" e gia nostro).
//   - Nessun oggetto con distruttore resta vivo quando si chiama be_raise (longjmp).
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <string.h>
#include "../core/vos_i18n.h"

static CmdCall g_call;

void cmdCallSet(const CmdCall& call) { g_call = call; }

bool berryCmdArgsOk(const CmdCall& call, ErrBuf& err) {
  if (call.n < 0 || call.n > CMD_ARGS_MAX) { err = tr("Troppi argomenti (massimo 8)"); return false; }
  size_t total = 0;
  for (int i = 0; i < call.n; i++) {
    size_t n = strlen(call.arg[i]);
    if (n == 0 || n > CMD_ARG_LEN) { err = tr("Argomento vuoto o troppo lungo (massimo 96 caratteri)"); return false; }
    for (size_t k = 0; k < n; k++) {
      unsigned char c = (unsigned char)call.arg[i][k];
      if (c < 32 || c > 126) { err = tr("Argomento non valido: solo lettere, numeri e simboli ASCII"); return false; }
    }
    total += n;
  }
  if (total > CMD_ARGS_TOTAL) { err = tr("Argomenti troppo lunghi (massimo 300 caratteri in tutto)"); return false; }
  return true;
}

static int bfCmd(bvm* vm) {
  be_pushstring(vm, g_call.sub);
  be_return(vm);
}

static int bfArgs(bvm* vm) {
  be_newlist(vm);
  for (int i = 0; i < g_call.n; i++) {
    be_pushstring(vm, g_call.arg[i]);
    be_data_push(vm, -2);                                    // nella lista (che sta sotto il testo)
    be_pop(vm, 1);
  }
  berryWrap(vm, "list");
  be_return(vm);
}

void berryCmdRegister(bvm* vm) {
  be_regfunc(vm, "cmd", bfCmd);
  be_regfunc(vm, "args", bfArgs);
}
#else
struct bvm;
void berryCmdRegister(bvm*) {}
#endif
