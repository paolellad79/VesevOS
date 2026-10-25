// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cleaner.cpp
// Comando "vesevos-cleaner" (solo Admin): pulizia SICURA, con prova a secco di fabbrica.
//
//   vesevos-cleaner [categoria ...]       mostra COSA si potrebbe pulire e QUANTO spazio si libera. Non cancella niente.
//   vesevos-cleaner -y [categoria ...]    pulisce dopo conferma (y/n da seriale, " yes" dalla pagina). Senza categorie: tutte.
//
// Categorie (e solo queste, scelte una a una perche si sa con certezza che sono pulibili):
//   tmp      i "nome.tmp~" lasciati da un salvataggio interrotto, SOLO se esiste anche "nome" (la logica e quella di fsTmpClean)
//   crash    i rapporti dei crash (/crash.txt), con crashClear: lo stesso del comando "crash clear"
//   log      il registro degli eventi in RAM, con logClear: lo stesso di "log clear" (non libera flash: e un buffer fisso)
//   reboots  il diario dei riavvii, con diaryClear: lo stesso di "reboots clear"
//   cache    /selftest.tmp, il file di prova che l'autodiagnosi scrive e subito toglie (resta solo se la scheda si e spenta nel mezzo)
//
// MAI toccati, perche non c'e nessun percorso scelto dall'utente: ogni categoria ha il suo file o la sua funzione scritti qui sopra.
// Quindi non si puo arrivare a /vesevos.conf, /io.cfg (e i suoi .bak/.tmp), script, app, lingue, certificati, chiavi, /mqtt-ca.pem, utenti,
// regole, registro di AUDIT (/audit.log: si svuota solo con "audit") ne a cartelle intere. Niente ricorsione, niente jolly.
// Lasciati fuori di proposito: /rules.tmp e io.cfg.tmp (nel momento dello scambio possono essere l'unica copia valida) e i file di prova
// della microSD (sono sul volume della scheda, non nella memoria interna).
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "vos_shell_rep.h"

namespace {

enum Cat : uint8_t { C_TMP, C_CRASH, C_LOG, C_REBOOTS, C_CACHE, C_COUNT };

struct CatInfo {
  const char* name;
  const char* what;        // chiave tr
};

const CatInfo kCats[C_COUNT] = {
  {"tmp", TRK("residui .tmp~ di salvataggi interrotti (solo se il file completo c'e)")},
  {"crash", TRK("rapporti dei crash (/crash.txt)")},
  {"log", TRK("registro degli eventi in RAM")},
  {"reboots", TRK("diario degli ultimi riavvii")},
  {"cache", TRK("file di prova dell'autodiagnosi (/selftest.tmp)")},
};

const char* const kCacheFile = "/selftest.tmp";
const uint8_t     ALL_CATS = (1 << C_COUNT) - 1;

// Quanto c'e da pulire in una categoria.
struct Found {
  uint32_t items = 0;       // voci (file, righe, avvii)
  uint32_t bytes = 0;       // byte dei file che si libererebbero (0 per il registro in RAM)
  uint16_t orphans = 0;     // tmp: .tmp~ senza il file completo, lasciati
  bool     partial = false; // tmp: cartelle con troppe voci, guardate solo in parte
};

uint32_t fileBytes(const char* path) {
  VosFile f = drvFsOpen(path, "r");
  return (f && !f.isDirectory()) ? (uint32_t)f.size() : 0;
}

// Prova a secco: guarda soltanto, non cambia niente.
Found measure(Cat c) {
  Found f;
  switch (c) {
    case C_TMP: {
      FsTmpStat st;
      st.dryRun = true;
      fsTmpCleanKnown(st);
      f.items = st.removed;
      f.bytes = st.bytes;
      f.orphans = st.orphan;
      f.partial = st.more;
      break;
    }
    case C_CRASH:
      f.bytes = crashFileBytes();
      f.items = f.bytes ? 1 : 0;
      break;
    case C_LOG:
      f.items = (uint32_t)logCount();
      break;
    case C_REBOOTS:
      f.bytes = diaryFileBytes();
      f.items = (uint32_t)diaryCount();
      break;
    case C_CACHE:
      if (selftestRunning()) break;                 // l'autodiagnosi sta usando il file: non si tocca
      f.bytes = fileBytes(kCacheFile);
      f.items = drvFsExists(kCacheFile) ? 1 : 0;
      break;
    default:
      break;
  }
  return f;
}

// La tabella della prova a secco per le categorie scelte. Ritorna la somma dei byte (flash) e dice se c'e qualcosa da pulire.
uint32_t printTable(OutSink& o, uint8_t mask, bool& anything) {
  o.printf("%-9s %5s %10s  %s\r\n", tr("Categoria"), tr("Voci"), tr("Spazio"), tr("Cosa"));
  uint32_t total = 0;
  anything = false;
  bool partial = false;
  uint32_t orphans = 0;
  for (int i = 0; i < C_COUNT; i++) {
    if (!(mask & (1 << i))) continue;
    Found f = measure((Cat)i);
    char sz[16];
    if (i == C_LOG) snprintf(sz, sizeof sz, "RAM");
    else repBytes(f.bytes, sz, sizeof sz);
    o.printf("%-9s %5lu %10s  %s\r\n", kCats[i].name, (unsigned long)f.items, sz, tr(kCats[i].what));
    total += f.bytes;
    if (f.items) anything = true;
    partial = partial || f.partial;
    orphans += f.orphans;
  }
  char tot[16];
  repBytes(total, tot, sizeof tot);
  o.println(trf("Si liberano nella memoria interna: %s (byte dei file; lo spazio reale e a blocchi da 4 KB)", tot));
  if (orphans) o.println(trf("%u residui .tmp~ senza il file completo restano com'e: potrebbero essere l'unica copia", (unsigned)orphans));
  if (partial) o.println(tr("Alcune cartelle hanno troppe voci e sono state guardate solo in parte: ripeti il comando per il resto"));
  return total;
}

// Una riga di esito per categoria. Ritorna i byte liberati.
uint32_t report(OutSink& o, const char* name, bool ok, uint32_t bytes) {
  if (!ok) { o.println(trf("%s: errore, non cancellato", name)); return 0; }
  char sz[16];
  repBytes(bytes, sz, sizeof sz);
  o.println(trf("%s: fatto, liberati %s", name, sz));
  return bytes;
}

// Pulisce davvero una categoria (si arriva qui solo dopo la conferma). Ritorna i byte liberati.
uint32_t cleanOne(Cat c, OutSink& o) {
  const char* name = kCats[c].name;
  if (c == C_TMP) {
    FsTmpStat st;
    fsTmpCleanKnown(st);
    uint32_t freed = report(o, name, true, st.bytes);
    if (st.failed) o.println(trf("%s: %u non cancellati per un errore", name, (unsigned)st.failed));
    if (st.orphan) o.println(trf("%s: %u senza il file completo, lasciati com'erano", name, (unsigned)st.orphan));
    return freed;
  }
  Found before = measure(c);
  if (!before.items) { o.println(trf("%s: niente da pulire", name)); return 0; }
  bool ok = false;
  switch (c) {
    case C_CRASH:   ok = crashClear(); break;
    case C_LOG:     logClear(); ok = true; break;
    case C_REBOOTS: diaryClear(); ok = true; break;
    case C_CACHE: {
      ErrBuf err;
      ok = fsRemove(kCacheFile, err);
      break;
    }
    default: break;
  }
  if (c == C_LOG) {                                     // buffer fisso in RAM: niente spazio liberato, si dicono le righe
    o.println(trf("%s: fatto, %u righe cancellate (RAM, non libera flash)", name, (unsigned)before.items));
    return 0;
  }
  return report(o, name, ok, before.bytes);
}

// Le categorie scelte dalle parole della riga. Ritorna false (e lo dice) se una non esiste.
bool parseArgs(const char* line, OutSink& o, uint8_t& mask, bool& yes) {
  mask = 0;
  yes = false;
  bool confirmed = askConfirmed(line);                  // l'ultima parola e "yes": la conferma messa da askConfirm, non una categoria
  for (int i = 1; i < 16; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    w.lower();
    if (w == "-y") { yes = true; continue; }
    if (confirmed && w == "yes" && argAt(line, i + 1).empty()) continue;
    int found = -1;
    for (int k = 0; k < C_COUNT; k++)
      if (w == kCats[k].name) found = k;
    if (found < 0) {
      o.println(trf("Categoria sconosciuta: %s", w.c_str()));
      o.println(tr("Uso: vesevos-cleaner [-y] [tmp|crash|log|reboots|cache ...]"));
      return false;
    }
    mask |= (uint8_t)(1 << found);
  }
  if (!mask) mask = ALL_CATS;
  return true;
}

}  // namespace

void shx::shCleaner(const char* line, OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  uint8_t mask;
  bool yes;
  if (!parseArgs(line, o, mask, yes)) return;

  if (!yes || !askConfirmed(line)) {
    bool anything;
    if (!yes) o.println(tr("Prova a secco: non cancello niente"));
    printTable(o, mask, anything);
    if (!yes) {
      o.println(tr("Per pulire: vesevos-cleaner -y [categoria ...] (chiede conferma)"));
      o.println(tr("Non tocca mai: configurazione, periferiche, utenti, certificati, script, app, lingue, registro di audit (comando audit)"));
      return;
    }
    if (!anything) { o.println(tr("Niente da pulire")); return; }
    char names[64] = "";
    for (int i = 0; i < C_COUNT; i++) {
      if (!(mask & (1 << i))) continue;
      size_t n = strlen(names);
      snprintf(names + n, sizeof names - n, "%s%s", n ? ", " : "", kCats[i].name);
    }
    askConfirm(o, line, trf("Cancello: %s. Non si annulla. Procedo?", names));
    return;
  }

  uint32_t freed = 0;
  char names[64] = "";
  for (int i = 0; i < C_COUNT; i++) {                   // l'ordine e fisso: il registro per ultimo, cosi la nostra riga nel registro resta
    if (!(mask & (1 << i))) continue;
    freed += cleanOne((Cat)i, o);
    size_t n = strlen(names);
    snprintf(names + n, sizeof names - n, "%s%s", n ? "," : "", kCats[i].name);
  }
  char sz[16];
  repBytes(freed, sz, sizeof sz);
  o.println(trf("Totale liberato nella memoria interna: %s", sz));
  vlog("SISTEMA: pulizia dalla shell (%s), liberati %lu byte", names, (unsigned long)freed);
}
