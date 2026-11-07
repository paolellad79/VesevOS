// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_vde.cpp
// Parte "vde" del comando "vesevos-driver" (e del vecchio alias "vesevos-elf driver"), solo con VOS_WITH_ELF: driver in ELF (.vde).
// Formato: docs/progetto/formato-vde.md. Lo chiama vos_shell_driver.cpp, che sceglie tra .vdb e .vde dal nome o dall'estensione.
//   list                              driver .vde in /devs: bus, segnali, se hanno i pin
//   info <nome>                       dettagli: segnali, pin dati, comandi
//   install <file|nome> [pin,pin]     (Admin) mette il file in /devs, se serve, e da' i pin: quelli scelti da te o, senza, i proposti dal firmware
//   pins <nome> pin,pin               (Admin) da' altri pin a un driver installato (se non vanno restano i vecchi)
//   remove <nome>                     (Admin) toglie i pin e il file
// "base" = posizione della parola del sottocomando nella riga (1 per vesevos-driver, 2 per vesevos-elf driver).
// I pin li sceglie e li applica chi scrive il comando; il controllo (vietati, gia usati, doppi) e quello unico del registro dei pin.
#include "vos_shell_int.h"

#if VOS_WITH_ELF
#include <stdlib.h>
#include "../core/vos_files.h"
#include "../core/vos_pacchetto.h"
#include "../packages/vos_pacchetto_nome.h"
#include "../devices/vos_drvres.h"
#include "../packages/vos_elf.h"
#include "../packages/vos_elf_file.h"
#include "../packages/vos_vde.h"

namespace {

const char* nomeBus(uint8_t b) { return b == VB_I2C ? "i2c" : b == VB_SPI ? "spi" : b == VB_SD ? "sd" : "gpio"; }

void cmdList(OutSink& o) {
  char (*nomi)[MOD_NAME_BUF] = new char[VDE_LIST_MAX][MOD_NAME_BUF];
  int tot = vdeElenco(nomi, VDE_LIST_MAX), n = tot < VDE_LIST_MAX ? tot : VDE_LIST_MAX;
  VdeInfo* v = new VdeInfo();
  if (tot == 0) o.println(tr("Nessun driver vde in /devs"));
  for (int i = 0; i < n; i++) {
    ErrBuf e;
    if (!vdeLeggi(nomi[i], *v, nullptr, e)) { o.println(trf("%-24s non valido: %s", nomi[i], e.c_str())); continue; }
    o.printf("%-24s %-5s %-5s %s\r\n", v->name, "vde", nomeBus(v->bus), vdeInstallato(v->name) ? tr("pin dati") : tr("senza pin"));
  }
  delete v;
  delete[] nomi;
}

void cmdInfo(const char* line, OutSink& o, int base) {
  Word w = argAt(line, base + 1);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver info <nome>")); return; }
  VdeInfo* v = new VdeInfo();
  ErrBuf e;
  if (!vdeLeggi(w.c_str(), *v, nullptr, e)) { o.println(trf("%s: %s", w.c_str(), e.c_str())); delete v; return; }
  o.println(trf("%s - %s (vde, bus %s, %u byte)", v->name, v->title, nomeBus(v->bus), (unsigned)v->elfLen));
  o.print("  "); o.println(trf("Versione: %s", v->ver));
  if (v->author[0]) { o.print("  "); o.println(trf("Autore: %s", v->author)); }
  int pin[VDE_SIG_MAX], np = vdePinDati(v->name, pin, VDE_SIG_MAX);      // per la microSD i pin stanno nella periferica sd
  for (int i = 0; i < v->nsig; i++) {
    if (i < np) o.println(trf("  %-6s GPIO %d", v->sig[i], pin[i]));
    else o.println(trf("  %-6s (pin da dare)", v->sig[i]));
  }
  for (int i = 0; i < v->ncmd; i++) o.println(trf("  comando: %s %s (%s)", v->cmd[i].cmd, v->cmd[i].sub, v->cmd[i].admin ? "Admin" : tr("Operatore")));
  if (v->use[0]) o.println(trf("  permessi: %s", v->use));
  delete v;
}

void cmdInstall(const char* line, OutSink& o, uint32_t sess, int base) {
  Word w = argAt(line, base + 1);
  Word pins = argAt(line, base + 2);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver install <file|nome> [pin,pin]")); return; }
  char nome[MOD_NAME_BUF];
  ErrBuf err;
  if (strchr(w.c_str(), '/')) {                              // un file: lo si controlla e lo si mette in /devs
    ShellSess ss; shellSessLoad(sess, ss);
    char path[FS_PATH_MAX + 1];
    if (!shellResolvePath(ss.cwd, w.c_str(), path, sizeof path)) { o.println(tr("Serve il nome di un file valido")); return; }
    const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
    size_t bl = strlen(base);
    if (bl < 5 || strcmp(base + bl - 4, VDE_EXT) || bl - 4 >= sizeof nome) { o.println(tr("Il file deve chiamarsi nome.vde")); return; }
    memcpy(nome, base, bl - 4); nome[bl - 4] = 0;
    VosElfImg im;
    int r = vos_elf_img_leggi(path, VDE_TEXT_MAX + 64 + vos_elf_caps()->file_max, im);
    if (r != VOS_ELF_OK) { o.println(trf("%s: %s", w.c_str(), vos_elf_err_testo(r))); return; }
    PacDati d;
    pacLeggi(std::string_view((const char*)im.p, im.n < VDE_TEXT_MAX ? im.n : VDE_TEXT_MAX), d);
    if (d.nome[0]) snprintf(nome, sizeof nome, "%s", d.nome);      // il nome corto dichiarato vale piu del nome del file
    ErrBuf avviso;
    char installato[64];
    snprintf(installato, sizeof installato, "%s/%s%s", VDE_DIR, nome, VDE_EXT);
    pacchettoAvviso(installato, std::string_view((const char*)im.p, im.n < VDE_TEXT_MAX ? im.n : VDE_TEXT_MAX), avviso);
    if (!vdeSalva(nome, im.p, im.n, err)) { o.println(trf("%s non installato: %s", nome, err.c_str())); return; }
    if (!avviso.empty()) o.println(avviso.c_str());
  } else {
    if (strlen(w.c_str()) > 24) { o.println(trf("%s: nome troppo lungo", w.c_str())); return; }
    snprintf(nome, sizeof nome, "%.47s", w.c_str());
    if (!vdeEsiste(nome)) { o.println(trf("%s: file non trovato in /devs", nome)); return; }
  }
  if (!vdeInstalla(nome, pins.empty() ? "" : pins.c_str(), err)) { o.println(trf("%s: file salvato, pin non dati: %s", nome, err.c_str())); return; }
  o.println(trf("%s installato", nome));
}

void cmdPins(const char* line, OutSink& o, int base) {
  Word w = argAt(line, base + 1), pins = argAt(line, base + 2);
  if (w.empty() || w.cut() || pins.empty()) { o.println(tr("Uso: vesevos-driver pins <nome> pin,pin")); return; }
  ErrBuf err;
  o.println(vdeCambiaPin(w.c_str(), pins.c_str(), err) ? trf("%s: pin cambiati", w.c_str()) : trf("%s: %s", w.c_str(), err.c_str()));
}

void cmdRemove(const char* line, OutSink& o, int base) {
  Word w = argAt(line, base + 1);
  if (w.empty() || w.cut()) { o.println(tr("Uso: vesevos-driver remove <nome>")); return; }
  ErrBuf err;
  o.println(vdeRimuovi(w.c_str(), true, err) ? trf("%s tolto", w.c_str()) : trf("%s: %s", w.c_str(), err.c_str()));
}

}  // namespace

void shx::shVde(const char* line, OutSink& o, int role, uint32_t sess, int base) {
  Word sub = argAt(line, base);
  sub.lower();
  bool lettura = sub.empty() || sub == "list" || sub == "info";
  if (role < (lettura ? ROLE_OPER : ROLE_ADMIN)) o.println(tr("Serve il livello Admin"));
  else if (sub.empty() || sub == "list") cmdList(o);
  else if (sub == "info") cmdInfo(line, o, base);
  else if (sub == "install") cmdInstall(line, o, sess, base);
  else if (sub == "pins") cmdPins(line, o, base);
  else if (sub == "remove") cmdRemove(line, o, base);
  else o.println(tr("Uso: vesevos-driver list|info|install|pins|remove"));
}
#endif
