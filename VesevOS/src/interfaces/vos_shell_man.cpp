// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_man.cpp
// Aiuto della shell: l'elenco "help", "help <comando>" e "man <comando>". Tutto viene dalla tabella unica dei comandi
// (vos_shell_cmdtab.cpp) e dai ruoli (vos_shell_roles.cpp): niente e copiato. I testi di descrizione e gli esempi stanno nel file
// /usr/share/vesevos/manuale.txt (fuori dal firmware); i comandi che non vi compaiono mostrano sintassi, descrizione breve, alias e ruolo.
// Per aggiungere o cambiare un testo: tools/data/manuale_src.txt, poi tools/mkrisorse.py.
#include "vos_shell_cmdtab.h"
#include "vos_shell_roles.h"
#include "../core/vos_common.h"
#include "../packages/vos_drvcmd.h"
#include "../core/vos_i18n.h"
#include "../core/vos_risorse_file.h"
#include "vos_shell_man_rule.h"
#include <string.h>

namespace {

using shx::CmdInfo;

// Il manuale e un file di testo, /usr/share/vesevos/manuale.txt (generato da tools/mkrisorse.py da tools/data/manuale_src.txt, controllato con
// SHA256SUMS): una voce per comando, "@comando", poi le righe di descrizione (ogni riga e una frase tradotta con tr()), "$ esempio" per gli esempi
// e ">" al posto delle righe quando l'aiuto e LUNGO (oltre 300 byte): il testo completo sta su GitHub, in sito-github/aiuto/<comando>.md.
// Se il file manca la shell lo dice; il resto di man (nome, uso, sottocomandi, alias, ruolo) viene dalla tabella dei comandi.
#define VOS_AIUTO_URL "https://github.com/paolellad79/VesevOS/blob/main/aiuto/"      // uguale a URL_AIUTO in tools/manuale_lib.py

void stampaRiga(OutSink& o, const char* t) {
  o.print("    ");
  o.println(t);
}

// Cerca la voce nel file. Con o = nullptr si sa solo se c'e. Altrimenti si scrive la descrizione (esempi = false, con il titolo)
// oppure gli esempi (esempi = true, con il titolo). Il file si controlla con SHA256SUMS la prima volta (risFileApri).
ManEsito manVoce(const char* name, OutSink* o, bool esempi) {
  VosFile f;
  size_t len = 0;
  if (risFileApri(RF_MANUALE, f, len) != RIS_OK) {
    if (o && !esempi) {
      o->println(tr("DESCRIZIONE"));
      stampaRiga(*o, tr("Manuale non disponibile: il file /usr/share/vesevos/manuale.txt manca o non e valido."));
    }
    return MAN_FILE_NO;
  }
  bool titolo = false;
  return manScorri(
      name, [&](uint8_t* b, size_t n) { return f.read(b, n); },
      [&](const char* riga) {                                // descrizione: la riga e italiano, tr() la cerca nel dizionario della lingua scelta
        if (!o || esempi) return;
        if (!titolo) o->println(tr("DESCRIZIONE"));
        titolo = true;
        stampaRiga(*o, tr(riga));
      },
      [&](const char* riga) {                                // esempio (non si traduce)
        if (!o || !esempi) return;
        if (!titolo) o->println(tr("ESEMPI"));
        titolo = true;
        stampaRiga(*o, riga);
      },
      [&]() {                                                // voce lunga: il testo completo e su internet
        if (!o || esempi) return;
        o->println(tr("DESCRIZIONE"));
        stampaRiga(*o, tr("Il testo completo di questo comando e su internet (serve la rete):"));
        o->print("    ");
        o->print(VOS_AIUTO_URL);
        o->print(name);
        o->println(".md");
      });
}

const char* roleName(int role) {
  return role >= ROLE_ADMIN ? tr("Amministratore") : role == ROLE_OPER ? tr("Operatore") : tr("Ospite");
}

// Il ruolo minimo per il comando scritto da solo, e per ognuna delle sue parole (sottocomandi): la tabella dei ruoli e l'unica fonte.
int minRole(const CmdInfo& ci, const char* sub) {
  return shx::roleMin(Word(ci.name, strlen(ci.name)), Word(sub, strlen(sub)));
}

// Scrive le parole di ci.subs il cui ruolo minimo e (elevated = false) uguale o sotto a quello del comando, oppure (true) sopra.
// Ritorna quante sono; con o = nullptr le conta soltanto. Le parole che non sono sottocomandi (numeri, "-h") valgono come il comando.
int printSubs(OutSink* o, const CmdInfo& ci, bool elevated) {
  int base = minRole(ci, "");
  int n = 0;
  const char* p = ci.subs;
  while (*p) {
    while (*p == ' ') p++;
    const char* b = p;
    while (*p && *p != ' ') p++;
    size_t len = (size_t)(p - b);
    if (len == 0 || len > Word::MAX) continue;
    char w[Word::MAX + 1];
    memcpy(w, b, len);
    w[len] = 0;
    if ((minRole(ci, w) > base) != elevated) continue;
    if (o) {
      o->print(n ? " " : "    ");
      o->print(w);
    }
    n++;
  }
  if (n && o) o->println();
  return n;
}

bool hasSubs(const CmdInfo& ci) { return ci.subs[0] && strcmp(ci.subs, "@commands") != 0; }

}  // namespace

void shx::cmdUsage(OutSink& o, const char* name) {
  const CmdInfo* ci = cmdFind(name);
  o.println(trf("Uso: %s", ci ? ci->usage : name));
}

void shx::cmdHelpList(OutSink& o, int role) {
  o.println(tr("Comandi del tuo ruolo (help <comando> e man <comando> spiegano il singolo comando):"));
  for (size_t i = 0; i < cmdCount(); i++) {
    const CmdInfo& ci = cmdAt(i);
    if (ci.flags & CMD_HIDDEN) continue;
    if (!roleAllows(role, Word(ci.name, strlen(ci.name)), Word())) continue;
    o.printf("  %-22s %s\r\n", ci.name, tr(ci.brief));
  }
  if (role >= ROLE_OPER) {                                           // comandi dei driver (scritti nelle funzioni VesevOS, nei loro file)
    char words[8][13];
    for (int i = 0, n = drvCmdWords(words, 8); i < n; i++) o.printf("  %-22s %s\r\n", words[i], tr("comando di un driver"));
  }
  o.println(tr("Tab completa comandi, opzioni e percorsi; due Tab di fila elencano le possibilita."));
}

bool shx::cmdHelpOne(OutSink& o, const char* name, int role, bool full) {
  Word w(name, strlen(name));
  w.lower();
  canonicalName(w);
  const CmdInfo* ci = cmdFind(w.c_str());
  if (!ci) return drvCmdHelp(o, w.c_str(), role);                    // forse e il comando di un driver
  if (!roleAllows(role, Word(ci->name, strlen(ci->name)), Word())) return false;   // stessa risposta per "non esiste" e "non per te"
  int base = minRole(*ci, "");

  if (!full) {                                                       // help <comando>: la scheda breve
    o.println(ci->usage);
    o.print("    ");
    o.println(tr(ci->brief));
    if (ci->alias[0]) o.println(trf("Alias: %s", ci->alias));
    o.println(trf("Ruolo minimo: %s", roleName(base)));
    if (manVoce(ci->name, nullptr, false) > MAN_NO) o.println(trf("Per i dettagli: man %s", ci->name));
    return true;
  }

  o.println(tr("NOME"));
  o.print("    ");
  o.print(ci->name);
  o.print(" - ");
  o.println(tr(ci->brief));
  o.println(tr("USO"));
  o.print("    ");
  o.println(ci->usage);
  manVoce(ci->name, &o, false);
  if (hasSubs(*ci)) {
    o.println(tr("OPZIONI E SOTTOCOMANDI"));
    printSubs(&o, *ci, false);
    if (printSubs(nullptr, *ci, true)) {
      o.print("  ");
      o.println(tr("Richiedono un ruolo piu alto:"));
      printSubs(&o, *ci, true);
    }
  }
  if (ci->alias[0]) {
    o.println(tr("ALIAS (vecchi nomi, ancora validi)"));
    o.print("    ");
    o.println(ci->alias);
  }
  o.println(tr("RUOLO MINIMO"));
  o.print("    ");
  o.println(roleName(base));
  manVoce(ci->name, &o, true);
  return true;
}
