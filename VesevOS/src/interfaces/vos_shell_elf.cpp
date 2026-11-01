// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_elf.cpp
// Comando "vesevos-elf" (solo con VOS_WITH_ELF): moduli ELF (.vae), codice nativo.
//
//   vesevos-elf [status]                                  stato, moduli in corso e in quarantena
//   vesevos-elf run [-t sec] [-u gruppi] <nome|file> [argomenti]   avvia in sottofondo (Admin); senza -u concede i gruppi che il modulo chiede
//   vesevos-elf ps                                        moduli in corso e ultimi finiti
//   vesevos-elf kill <numero|nome>                        ferma un modulo (Admin)
//   vesevos-elf which <nome|file>                         dove sta, se e valido, se e in corso o in quarantena
//   vesevos-elf list                                      moduli installati in /bin
//   vesevos-elf install <file>                            controlla e mette un .vae in /bin (Admin)
//   vesevos-elf quarantine | unquarantine <nome>          elenco / toglie dalla quarantena (Admin)
//   vesevos-elf driver list|info|install|remove           driver in ELF (.vde), vedi vos_shell_vde.cpp
//   vesevos-elf test [-t sec] [-u gruppi] <file> [argomenti]       prova in primo piano, con misure di tempo e memoria (Admin)
//
// -t = tetto di tempo in secondi (test: 1-30, di solito 5; run: nessun tetto se manca). -u = gruppi di primitive concessi, separati da
// virgola (di solito "log"; il gruppo base e sempre concesso). Un nome semplice ("sensore") e il file /bin/sensore.vae; un percorso
// puo stare in /bin, /apps, /devs o sulla microSD. Lettura (status, ps, which, list, quarantine): Operatore. Il resto: Admin.
// Niente conferme: l'Admin che scrive il comando e responsabile del modulo (a servizio "elf" acceso).
#include "vos_shell_int.h"

#if VOS_WITH_ELF
#include "../core/vos_files.h"
#include "../core/vos_layout.h"
#include "../core/vos_ext.h"
#include "../packages/vos_elf.h"
#include "../packages/vos_elf_file.h"
#include "../packages/vos_progext.h"
#include "../packages/vos_prog_data.h"
#include "../packages/vos_elf_prova.h"
#include "../packages/vos_elf_quar.h"
#include "../packages/vos_elf_run.h"

namespace {

const uint32_t kTempoDefaultMs = 5000;
const uint32_t kTempoMaxS = 30;
const int      kArgMax = 8;

bool endsWith(const char* s, const char* ext) {
  size_t n = strlen(s), e = strlen(ext);
  return n >= e && strcmp(s + n - e, ext) == 0;
}

// Gruppi "log,sd" -> "log sd" (il formato di use=). Ritorna false se ci sono caratteri strani o e troppo lungo.
bool useDaElenco(const char* in, char* out, size_t cap) {
  size_t n = strlen(in);
  if (n == 0 || n >= cap) return false;
  for (size_t i = 0; i < n; i++) {
    char c = in[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == ',';
    if (!ok) return false;
    out[i] = c == ',' ? ' ' : c;
  }
  out[n] = 0;
  return true;
}

struct Richiesta {
  char     path[FS_PATH_MAX + 1];
  char     nome[VOS_ELF_NOME_MAX + 1];
  char     use[VOS_ELF_USE_MAX];
  bool     useEsplicito = false;                // c'e -u: si usa quello e basta
  uint32_t tempoMs = kTempoDefaultMs;
  char     argBuf[200];
  char*    argv[kArgMax + 2];
  int      argc = 0;
};

// Dal testo scritto (nome o percorso) al file del modulo. false = errore gia scritto.
bool risolvi(const char* in, const ShellSess& ss, char* path, size_t cap, char* nome, OutSink& o) {
  bool comePercorso = strchr(in, '/') || endsWith(in, VOS_EXT_APP_E) || endsWith(in, VOS_EXT_OLD_APP_E) || endsWith(in, ".elf");
  char tmp[FS_PATH_MAX + 1];
  if (comePercorso && !shellResolvePath(ss.cwd, in, tmp, sizeof tmp)) { o.println(tr("Serve il nome di un file valido")); return false; }
  int e = vos_elf_risolvi(comePercorso ? tmp : in, path, cap, nome);
  if (e != VOS_ELF_OK) { o.println(trf("%s: %s", in, vos_elf_err_testo(e))); return false; }
  return true;
}

// Le opzioni, il file e gli argomenti dalla riga. idx = posizione della parola dopo il sottocomando. false = errore gia scritto.
// senzaTetto: per "run" il tetto di tempo manca se non c'e -t.
bool leggiRiga(const char* line, int idx, const ShellSess& ss, Richiesta& r, OutSink& o, bool senzaTetto) {
  strcpy(r.use, "log");
  if (senzaTetto) r.tempoMs = 0;                                          // di solito basta il gruppo log (il base c'e sempre)
  for (;; idx++) {
    Word w = argAt(line, idx);
    if (w == "-t") {
      long s = argAt(line, ++idx).toInt();
      if (s < 1 || s > (long)kTempoMaxS) { o.println(trf("-t vuole un numero di secondi da 1 a %d", (int)kTempoMaxS)); return false; }
      r.tempoMs = (uint32_t)s * 1000;
    } else if (w == "-u") {
      Word g = argAt(line, ++idx);
      if (g.cut() || !useDaElenco(g.c_str(), r.use, sizeof r.use)) { o.println(tr("-u vuole i gruppi separati da virgola (per esempio log)")); return false; }
      r.useEsplicito = true;
    } else {
      break;
    }
  }
  Word nome = argAt(line, idx);
  if (nome.empty() || nome.cut()) { o.println(tr("Serve il nome di un modulo o di un file")); return false; }
  if (!risolvi(nome.c_str(), ss, r.path, sizeof r.path, r.nome, o)) return false;
  const char* rest = restFrom(line, idx + 1);
  if (strlen(rest) >= sizeof r.argBuf) { o.println(tr("Argomenti troppo lunghi")); return false; }
  strcpy(r.argBuf, rest);
  const char* base = strrchr(r.path, '/');
  r.argv[r.argc++] = (char*)(base ? base + 1 : r.path);          // argv[0] = nome del file, come in ogni programma
  for (char* t = strtok(r.argBuf, " "); t; t = strtok(nullptr, " ")) {
    if (r.argc > kArgMax) { o.println(trf("Troppi argomenti (massimo %d)", kArgMax)); return false; }
    r.argv[r.argc++] = t;
  }
  r.argv[r.argc] = nullptr;
  return true;
}

const char* nomeFase(int fase) {
  switch (fase) {
    case VOS_ELF_FASE_VALIDA: return tr("controllo del file");
    case VOS_ELF_FASE_CARICA: return tr("caricamento");
    default:                  return tr("esecuzione");
  }
}

void scriviErrore(OutSink& o, const vos_elf_prova_ris_t& ris) {
  o.println(trf("Prova fermata durante il %s: %s (codice %d)", nomeFase(ris.fase), vos_elf_err_testo(ris.err), ris.err));
  if (ris.err == VOS_ELF_E_SPENTO) o.println(tr("Per accenderlo: vesevos-services vde start"));
  bool perSimbolo = ris.err == VOS_ELF_E_SIMBOLO || ris.err == VOS_ELF_E_RUOLO || ris.err == VOS_ELF_E_USE || ris.err == VOS_ELF_E_API;
  if (perSimbolo && ris.simbolo[0]) o.println(trf("Funzione chiesta dal modulo e rifiutata: %s", ris.simbolo));
  if (ris.err == VOS_ELF_E_USE) o.println(tr("Concedi il gruppo con -u (per esempio -u log)"));
  o.println(trf("Memoria libera dopo la prova: interna %u byte (prima %u)", (unsigned)ris.ram_dopo, (unsigned)ris.ram_prima));
}

void scriviEsito(OutSink& o, const vos_elf_prova_ris_t& ris) {
  if (ris.scaduto) o.println(tr("Tempo scaduto: task fermato e modulo scaricato a forza. Il modulo potrebbe aver lasciato risorse prese e non rilasciate: se la scheda si comporta in modo strano, riavviala."));
  else o.println(trf("Il modulo e finito, valore restituito: %d", ris.ritorno));
  o.println(trf("Tempo: caricamento %u ms, esecuzione %u ms", (unsigned)ris.ms_carica, (unsigned)ris.ms_esegui));
  o.println(trf("RAM stimata del modulo: %u byte", (unsigned)ris.ram_stima));
  o.println(trf("Memoria interna libera: prima %u, caricato %u, dopo %u byte (dopo - prima: %d)",
                (unsigned)ris.ram_prima, (unsigned)ris.ram_caricato, (unsigned)ris.ram_dopo, (int)ris.ram_dopo - (int)ris.ram_prima));
  if (ris.psram_prima)
    o.println(trf("PSRAM libera: prima %u, caricato %u, dopo %u byte (dopo - prima: %d)",
                  (unsigned)ris.psram_prima, (unsigned)ris.psram_caricato, (unsigned)ris.psram_dopo, (int)ris.psram_dopo - (int)ris.psram_prima));
}

void cmdTest(const char* line, OutSink& o, int role, uint32_t sess) {
  if (!vos_elf_abilitato()) { o.println(tr("Il sottosistema vde e spento. Per accenderlo: vesevos-services vde start")); return; }
  ShellSess ss;
  shellSessLoad(sess, ss);
  Richiesta r;
  if (!leggiRiga(line, 2, ss, r, o, false)) return;
  VosElfImg im;
  int e = vos_elf_img_leggi(r.path, vos_elf_caps()->file_max, im);
  if (e != VOS_ELF_OK) { o.println(trf("%s: %s", r.path, vos_elf_err_testo(e))); return; }
  o.println(trf("Prova del modulo %s (%u byte), tetto di tempo %u s", r.path, (unsigned)im.n, (unsigned)(r.tempoMs / 1000)));
  vos_elf_prova_par_t par = { role, r.use[0] ? r.use : nullptr, r.argc, r.argv, r.tempoMs };
  vos_elf_prova_ris_t ris;
  if (vos_elf_prova(im.p, im.n, par, ris) != 0) scriviErrore(o, ris);
  else scriviEsito(o, ris);
}

void cmdRun(const char* line, OutSink& o, int role, uint32_t sess) {
  if (!vos_elf_abilitato()) { o.println(tr("Il sottosistema vde e spento. Per accenderlo: vesevos-services vde start")); return; }
  ShellSess ss;
  shellSessLoad(sess, ss);
  Richiesta r;
  if (!leggiRiga(line, 2, ss, r, o, true)) return;
  VosElfImg im;
  int e = vos_elf_img_leggi(r.path, vos_elf_caps()->file_max, im);
  if (e != VOS_ELF_OK) { o.println(trf("%s: %s", r.path, vos_elf_err_testo(e))); return; }
  if (!r.useEsplicito) vos_elf_gruppi_richiesti(im.p, im.n, r.use, sizeof r.use);     // come l'avvio dalla pagina: i gruppi che il modulo chiede
  ErrBuf dati;
  if (!progDataReady(PK_APP_E, r.nome, dati)) { o.println(trf("%s non avviato: %s", r.nome, dati.c_str())); return; }   // dati sulla microSD scelta e la scheda non c'e
  vos_elf_run_par_t par = { role, r.use[0] ? r.use : nullptr, r.argc, r.argv, r.tempoMs, nullptr, 0 };
  int id = 0;
  e = vos_elf_run(r.nome, im.p, im.n, par, &id);
  if (e == VOS_ELF_OK) { im.cedi(); o.println(trf("%s avviato (numero %d)", r.nome, id)); return; }
  o.println(trf("%s non avviato: %s (codice %d)", r.nome, vos_elf_err_testo(e), e));
  if (e == VOS_ELF_E_QUARANTENA) o.println(tr("Per toglierlo dalla quarantena: vesevos-elf unquarantine <nome>"));
  if (e == VOS_ELF_E_USE) o.println(tr("Concedi il gruppo con -u (per esempio -u log,sd)"));
  if (e == VOS_ELF_E_FUNZ) o.println(tr("La funzionalita e spenta: si riaccende da Sistema > Funzionalita"));
}

void cmdPs(OutSink& o) {
  vos_elf_ps_t v[VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI];
  int n = vos_elf_ps(v, VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI);
  if (n == 0) { o.println(tr("Nessun modulo in esecuzione")); return; }
  o.println(tr("Num  Nome                      Stato      Secondi  RAM(B)   Ritorno"));
  for (int i = 0; i < n; i++) {
    const char* st = v[i].attivo ? tr("in corso") : v[i].uscita == VOS_ELF_USCITA_FERMATO ? tr("fermato") : v[i].uscita == VOS_ELF_USCITA_SCADUTO ? tr("scaduto") : tr("finito");
    char rit[16] = "-";
    if (!v[i].attivo) snprintf(rit, sizeof rit, "%d", v[i].ritorno);
    o.println(trf("%-4d %-25s %-10s %-8u %-8u %s", v[i].id, v[i].nome, st, (unsigned)v[i].secondi, (unsigned)v[i].ram_stima, rit));
  }
}

void cmdKill(const char* line, OutSink& o) {
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-elf kill <numero|nome>")); return; }
  int e = vos_elf_ferma(w.c_str());
  if (e == VOS_ELF_OK) o.println(trf("%s fermato", w.c_str()));
  else o.println(trf("%s: %s", w.c_str(), e == VOS_ELF_E_NOME ? tr("nessun modulo in esecuzione con questo nome o numero") : vos_elf_err_testo(e)));
}

int controlla(const char* path, uint32_t& ram, size_t& byte) { return vos_elf_pkg_controlla(path, &byte, &ram); }

void cmdWhich(const char* line, OutSink& o, uint32_t sess) {
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-elf which <nome|file>")); return; }
  ShellSess ss;
  shellSessLoad(sess, ss);
  char path[FS_PATH_MAX + 1], nome[VOS_ELF_NOME_MAX + 1];
  if (!risolvi(w.c_str(), ss, path, sizeof path, nome, o)) return;
  uint32_t ram = 0;
  size_t byte = 0;
  int e = controlla(path, ram, byte);
  o.println(path);
  if (e == VOS_ELF_E_FILE || e == VOS_ELF_E_PERCORSO) { o.println(trf("%s: %s", nome, vos_elf_err_testo(e))); return; }
  o.println(trf("Dimensione: %u byte, RAM stimata: %u byte", (unsigned)byte, (unsigned)ram));
  o.println(e == VOS_ELF_OK ? tr("Controllo del file: ok") : trf("Controllo del file: %s (codice %d)", vos_elf_err_testo(e), e));
  if (vos_elf_quar_dentro(nome)) o.println(tr("In quarantena"));
  vos_elf_ps_t v[VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI];
  int n = vos_elf_ps(v, VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI);
  for (int i = 0; i < n; i++) if (v[i].attivo && !strcmp(v[i].nome, nome)) o.println(trf("In corso (numero %d)", v[i].id));
}

struct ElencoCtx { OutSink* o; int n; };

void riga(const char* nome, size_t byte, void* ctx) {
  ElencoCtx* c = (ElencoCtx*)ctx;
  char path[FS_PATH_MAX + 1];
  progPercorso(path, sizeof path, layoutPath(LAY_APPS), nome, PEXT_APP_E);
  uint32_t ram = 0;
  size_t b = 0;
  int e = controlla(path, ram, b);
  const char* st = e != VOS_ELF_OK ? tr("non valido") : vos_elf_quar_dentro(nome) ? tr("in quarantena") : tr("ok");
  c->o->println(trf("%-25s %8u byte  %s", nome, (unsigned)byte, st));
  c->n++;
}

void cmdList(OutSink& o) {
  ElencoCtx c = { &o, 0 };
  vos_elf_pkg_elenco(riga, &c);
  if (c.n == 0) o.println(trf("Nessun modulo .vae in %s", layoutPath(LAY_APPS)));
}

void cmdInstall(const char* line, OutSink& o, uint32_t sess) {
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-elf install <file>")); return; }
  ShellSess ss;
  shellSessLoad(sess, ss);
  char path[FS_PATH_MAX + 1], nome[VOS_ELF_NOME_MAX + 1];
  if (!risolvi(w.c_str(), ss, path, sizeof path, nome, o)) return;
  uint32_t ram = 0;
  size_t byte = 0;
  int e = controlla(path, ram, byte);
  if (e != VOS_ELF_OK) { o.println(trf("%s non installato: %s (codice %d)", nome, vos_elf_err_testo(e), e)); return; }
  char dest[FS_PATH_MAX + 1];
  snprintf(dest, sizeof dest, "%s/%s%s", layoutPath(LAY_APPS), nome, VOS_EXT_APP_E);           // si scrive sempre il nome nuovo
  if (strcmp(path, dest) != 0) {
    ErrBuf err;
    if (!fsCopy(path, dest, err)) { o.println(trf("%s non installato: %s", nome, err.c_str())); return; }
    progToglieVecchio(layoutPath(LAY_APPS), nome, PEXT_APP_E);                                   // il vecchio .vel con lo stesso nome non serve piu
  }
  o.println(trf("%s installato in %s (%u byte, RAM stimata %u byte)", nome, dest, (unsigned)byte, (unsigned)ram));
}

void cmdQuarantena(const char* line, OutSink& o, bool togli) {
  if (!togli) {
    char q[8][VOS_ELF_NOME_MAX + 1];
    int n = vos_elf_quar_elenco(q, 8);
    if (n == 0) o.println(tr("Nessun modulo in quarantena"));
    for (int i = 0; i < n; i++) o.println(q[i]);
    return;
  }
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-elf unquarantine <nome>")); return; }
  o.println(vos_elf_quar_togli(w.c_str()) ? trf("%s tolto dalla quarantena", w.c_str()) : trf("%s non era in quarantena", w.c_str()));
}

void cmdStato(OutSink& o) {
  bool on = vos_elf_abilitato();
  o.println(trf("Sottosistema vde: %s", on ? tr("acceso") : tr("spento")));
  if (!on) { o.println(tr("Per accenderlo: vesevos-services vde start")); return; }
  o.println(trf("In esecuzione: %d (massimo %d)", vos_elf_tab_attivi(), VOS_ELF_RUN_MAX));
  char q[8][VOS_ELF_NOME_MAX + 1];
  int nq = vos_elf_quar_elenco(q, 8);
  if (nq) o.println(trf("In quarantena: %d (vesevos-elf quarantine)", nq));
  o.println(tr("Uso: vesevos-elf run|ps|kill|which|list|install|quarantine|unquarantine|data|driver|test"));
}

}  // namespace

void shx::shElf(const char* line, OutSink& o, int role, uint32_t sess) {
  Word sub = argAt(line, 1);
  sub.lower();
  if (sub == "driver") { shVde(line, o, role, sess, 2); return; }          // alias: ora si usa vesevos-driver
  bool lettura = sub.empty() || sub == "status" || sub == "ps" || sub == "which" || sub == "list" || sub == "quarantine";
  if (role < (lettura ? ROLE_OPER : ROLE_ADMIN)) o.println(tr("Serve il livello Admin"));
  else if (sub.empty() || sub == "status") cmdStato(o);
  else if (sub == "run") cmdRun(line, o, role, sess);
  else if (sub == "ps") cmdPs(o);
  else if (sub == "kill") cmdKill(line, o);
  else if (sub == "which") cmdWhich(line, o, sess);
  else if (sub == "list") cmdList(o);
  else if (sub == "install") cmdInstall(line, o, sess);
  else if (sub == "quarantine") cmdQuarantena(line, o, false);
  else if (sub == "unquarantine") cmdQuarantena(line, o, true);
  else if (sub == "data" && !argAt(line, 2).empty()) shProgData(PK_APP_E, argAt(line, 2).c_str(), argAt(line, 3).c_str(), o);   // dove l'app salva i dati
  else if (sub == "test") cmdTest(line, o, role, sess);
  else o.println(tr("Uso: vesevos-elf run|ps|kill|which|list|install|quarantine|unquarantine|data|driver|test"));
}
#endif
