// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_driver.cpp
// Comando "vesevos-driver": UNA sola famiglia di comandi per i driver delle periferiche, sia .vdb (Berry) sia .vde (ELF).
// Il tipo si capisce dall'estensione del file o dal nome (un nome vale per un solo driver). Le regole dei driver sono le stesse:
// i pin li sceglie e li applica chi scrive il comando, il controllo (vietati, gia usati, doppi) e quello unico del registro dei pin.
//
//   vesevos-driver [list]                          elenco dei driver in /devs: nome, tipo (vdb o vde), classe o bus, titolo
//   vesevos-driver info <nome>                     dettagli
//   vesevos-driver install <file|nome> [prefisso] [pin,pin]   (Admin) mette il file in /devs, se serve, e crea le periferiche sui pin scelti o proposti
//   vesevos-driver pins <nome> pin,pin             (Admin) da' altri pin a un driver vde installato (le periferiche di un vdb si cambiano da Periferiche)
//   vesevos-driver remove <nome>                   (Admin) toglie il driver (un vdb lascia le periferiche gia create; un vde libera i pin)
// Il vecchio comando "mod" (solo vdb) e "vesevos-elf driver" (solo vde) restano come alias.
#include "vos_shell_int.h"
#include <stdlib.h>
#include "vos_mod_app.h"
#include "../core/vos_ext.h"
#include "../core/vos_files.h"
#include "../devices/vos_mod.h"
#include "../drivers/vos_drv_fs.h"
#if VOS_WITH_ELF
#include "../packages/vos_vde.h"
#endif

namespace {

enum Tipo { T_NESSUNO, T_VDB, T_VDE };

bool finisceCon(const char* s, const char* ext) {
  size_t l = strlen(s), e = strlen(ext);
  return l > e && !strcmp(s + l - e, ext);
}

// Il driver di nome "nome" (senza estensione) e .vde, .vdb o non c'e.
Tipo tipoDi(const char* nome) {
#if VOS_WITH_ELF
  if (vdeEsiste(nome)) return T_VDE;
#endif
  char p[64];
  modFilePath(p, sizeof p, nome);
  return drvFsExists(p) ? T_VDB : T_NESSUNO;
}

// Il primo argomento e un file (con '/') o un nome. Se e un file mette in nome il nome senza estensione e in tipo il tipo dall'estensione.
bool daArgomento(const char* w, char* nome, size_t cap, Tipo& tipo, bool& fileDato) {
  fileDato = strchr(w, '/') != nullptr;
  const char* base = strrchr(w, '/');
  base = base ? base + 1 : w;
  size_t bl = strlen(base);
  tipo = T_NESSUNO;
  if (fileDato) {
    const char* ext = finisceCon(base, VOS_EXT_DRV_E) ? VOS_EXT_DRV_E : finisceCon(base, VOS_EXT_DRV_B) ? VOS_EXT_DRV_B : finisceCon(base, MOD_EXT_OLD) ? MOD_EXT_OLD : nullptr;
    if (!ext) return false;
    bl -= strlen(ext);
    tipo = !strcmp(ext, VOS_EXT_DRV_E) ? T_VDE : T_VDB;
  }
  if (bl == 0 || bl >= cap) return false;
  memcpy(nome, base, bl);
  nome[bl] = 0;
  return true;
}

void cmdList(OutSink& o, int role, uint32_t sess) {
  HeapBuf t(MOD_TEXT_MAX);
  modListText(t);
  o.print(t.c_str());
#if VOS_WITH_ELF
  shx::shVde("vesevos-driver list", o, role, sess, 1);
#else
  (void)role; (void)sess;
#endif
}

void cmdInfo(const char* line, OutSink& o, int role, uint32_t sess) {
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver info <nome>")); return; }
  Tipo t = tipoDi(w.c_str());
  if (t == T_VDB) { HeapBuf b(MOD_TEXT_MAX); modInfoText(w.c_str(), b); o.print(b.c_str()); }
#if VOS_WITH_ELF
  else if (t == T_VDE) shx::shVde(line, o, role, sess, 1);
#endif
  else o.println(trf("%s: driver non trovato in /devs", w.c_str()));
  (void)role; (void)sess;
}

// Legge un file di testo (driver vdb) e lo salva in /devs controllandolo.
bool salvaVdb(const char* path, const char* nome, ErrBuf& err) {
  VosFile f = drvFsOpen(path);
  if (!f || f.isDirectory()) { err = tr("File non trovato"); return false; }
  size_t n = f.size();
  if (n == 0 || n > MOD_FILE_MAX) { err = tr("File troppo grande (massimo 16 KB)"); return false; }
  char* b = (char*)malloc(n + 1);
  if (!b) { err = tr("Memoria insufficiente"); return false; }
  bool ok = f.read((uint8_t*)b, n) == n;
  if (ok) ok = modSave(nome, std::string_view(b, n), err);
  else err = tr("Impossibile leggere");
  free(b);
  return ok;
}

void cmdInstall(const char* line, OutSink& o, int role, uint32_t sess) {
  Word w = argAt(line, 2), a3 = argAt(line, 3), a4 = argAt(line, 4);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver install <file|nome> [prefisso] [pin,pin]")); return; }
  char nome[MOD_NAME_BUF];
  Tipo tipo; bool fileDato;
  if (!daArgomento(w.c_str(), nome, sizeof nome, tipo, fileDato)) { o.println(tr("Serve il nome di un driver o un file .vdb o .vde")); return; }
  ErrBuf err;
  if (!fileDato) tipo = tipoDi(nome);
  if (tipo == T_NESSUNO) { o.println(trf("%s: driver non trovato in /devs", nome)); return; }
  if (tipo == T_VDE) {
#if VOS_WITH_ELF
    shx::shVde(line, o, role, sess, 1);                       // i pin sono il terzo argomento: "install <file|nome> [pin,pin]"
#else
    o.println(tr("Questo firmware non carica driver vde"));
#endif
    return;
  }
  if (fileDato) {
    ShellSess ss; shellSessLoad(sess, ss);
    char path[FS_PATH_MAX + 1];
    if (!shellResolvePath(ss.cwd, w.c_str(), path, sizeof path)) { o.println(tr("Serve il nome di un file valido")); return; }
    if (!salvaVdb(path, nome, err)) { o.println(trf("%s non installato: %s", nome, err.c_str())); return; }
  }
  const char* pre = a3.c_str(); const char* pins = a4.c_str();
  if (strchr(pre, ',')) { pins = pre; pre = ""; }             // "install sd 10,11,12,13": il terzo valore con virgole sono i pin
  HeapBuf out(2048);
  if (!modInstallDriver(nome, pre, pins, out, err)) { o.println(trf("%s: %s", nome, err.c_str())); return; }
  o.println(out.c_str());
}

void cmdPins(const char* line, OutSink& o, int role, uint32_t sess) {
  Word w = argAt(line, 2);
  Tipo t = w.empty() ? T_NESSUNO : tipoDi(w.c_str());
#if VOS_WITH_ELF
  if (t == T_VDE) { shx::shVde(line, o, role, sess, 1); return; }
#endif
  (void)role; (void)sess;
  if (t == T_VDB) o.println(tr("I pin delle periferiche di un driver vdb si cambiano dalla pagina Periferiche"));
  else o.println(tr("Uso: vesevos-driver pins <nome> pin,pin"));
}

void cmdRemove(const char* line, OutSink& o, int role, uint32_t sess) {
  Word w = argAt(line, 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver remove <nome>")); return; }
  Tipo t = tipoDi(w.c_str());
#if VOS_WITH_ELF
  if (t == T_VDE) { shx::shVde(line, o, role, sess, 1); return; }
#endif
  (void)role; (void)sess;
  ErrBuf err;
  if (t != T_VDB) o.println(trf("%s: driver non trovato in /devs", w.c_str()));
  else if (!modDeleteDriver(w.c_str(), err)) o.println(err.c_str());
  else o.println(trf("%s tolto (le periferiche gia create restano: si tolgono da Periferiche)", w.c_str()));
}

}  // namespace

void shx::shDriver(const char* line, OutSink& o, int role, uint32_t sess) {
  Word sub = argAt(line, 1);
  sub.lower();
  bool lettura = sub.empty() || sub == "list" || sub == "info";
  if (role < (lettura ? ROLE_OPER : ROLE_ADMIN)) { o.println(tr("Serve il livello Admin")); return; }
  if (sub.empty() || sub == "list") cmdList(o, role, sess);
  else if (sub == "info") cmdInfo(line, o, role, sess);
  else if (sub == "install") cmdInstall(line, o, role, sess);
  else if (sub == "pins") cmdPins(line, o, role, sess);
  else if (sub == "remove") cmdRemove(line, o, role, sess);
  else o.println(tr("Uso: vesevos-driver list|info|install|pins|remove"));
}
