// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_drvcmd.cpp
// I comandi dei DRIVER nella shell (esempio: "sd ls /"). Il firmware non li conosce: sono dichiarati nel file del driver ([commands]) e scritti in Berry
// ([cmd]); qui la shell li cerca nel registro (packages/vos_drvcmd.cpp), controlla il ruolo e passa le parole come lista di argomenti.
// I comandi interni vincono SEMPRE (questa funzione si prova dopo di loro). Un driver non puo dare piu diritti di chi lancia il comando.
//   <comando> <sotto> [parole...]     ogni parola e un argomento; con "testo" nella dichiarazione: la prima parola e un argomento
//                                     e tutto il resto della riga e il secondo (puo contenere spazi)
#include "vos_shell_int.h"
#include "../packages/vos_drvcmd.h"
#include "../devices/vos_mod_cmd.h"

namespace {

// Elenco dei sottocomandi che il ruolo puo usare, separati da "|"
void subsFor(const char* cmd, int role, char* out, size_t cap) {
  DrvCmd s[MOD_CMD_MAX];
  int n = drvCmdSubs(cmd, s, MOD_CMD_MAX);
  out[0] = 0;
  for (int i = 0; i < n; i++) {
    if (s[i].admin && role < ROLE_ADMIN) continue;
    size_t l = strlen(out);
    snprintf(out + l, cap - l, "%s%s", l ? "|" : "", s[i].sub);
  }
}

void usage(OutSink& o, const char* cmd, int role) {
  char subs[160];
  subsFor(cmd, role, subs, sizeof subs);
  o.println(trf("Uso: %s <%s>", cmd, subs));
}

// Copia un argomento (al massimo CMD_ARG_LEN caratteri: la lunghezza e gia stata controllata da chi chiama)
void putArg(char* dst, const char* src) {
  size_t n = strnlen(src, CMD_ARG_LEN);
  memcpy(dst, src, n);
  dst[n] = 0;
}

// Le parole della riga dopo il sottocomando, come lista di argomenti. false = una parola e troppo lunga o sono troppe (il motivo e gia stampato).
bool buildArgs(const char* line, const DrvCmd& d, CmdCall& call, OutSink& o) {
  call.n = 0;
  int i = 2;
  for (;; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (d.rest && call.n == 1) {                               // con "testo": quello che resta della riga e il secondo argomento
      putArg(call.arg[1], restFrom(line, i));
      call.n = 2;
      break;
    }
    if (call.n >= CMD_ARGS_MAX) { o.println(tr("Troppi argomenti (massimo 8)")); return false; }
    if (w.cut() || w.length() > CMD_ARG_LEN) { o.println(tr("Argomento vuoto o troppo lungo (massimo 96 caratteri)")); return false; }
    putArg(call.arg[call.n++], w.c_str());
  }
  if (d.rest && call.n == 2 && strlen(restFrom(line, 3)) > CMD_ARG_LEN) { o.println(tr("Argomento vuoto o troppo lungo (massimo 96 caratteri)")); return false; }
  return true;
}

}  // namespace

// help <comando> per un comando di driver. false = non e un comando di driver.
bool shx::drvCmdHelp(OutSink& o, const char* name, int role) {
  if (!drvCmdHas(name) || role < ROLE_OPER) return false;
  usage(o, name, role);
  o.println(tr("Comando di un driver (scritto come script VesevOS, dichiarato nel suo file in /devs)"));
  return true;
}

bool shDriverCmd(const Word& c, const char* line, OutSink& o, int role) {
  if (!drvCmdHas(c.c_str())) return false;
  Word sub = argAt(line, 1);
  sub.lower();
  if (sub.empty()) { usage(o, c.c_str(), role); return true; }
  DrvCmd d;
  if (sub.cut() || !drvCmdFind(c.c_str(), sub.c_str(), d)) { o.println(tr("Sottocomando sconosciuto")); usage(o, c.c_str(), role); return true; }
  if (d.admin && role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }   // anche se la tabella dei ruoli lo lascia passare fino a qui
  CmdCall* call = new CmdCall();
  memset(call, 0, sizeof *call);
  snprintf(call->sub, sizeof call->sub, "%s", d.sub);
  HeapBuf out(BERRY_OUT_MAX + 64); ErrBuf err;
  if (buildArgs(line, d, *call, o)) {
    if (drvCmdRun(d, *call, role >= ROLE_ADMIN, out, err)) { if (out.length()) o.println(out.c_str()); }
    else { if (out.length()) o.println(out.c_str()); o.println(err.c_str()); }
  }
  delete call;
  return true;
}
