// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pacchetto_nome.cpp
// Pacchetti caricati: nome corto, controllo prima di sostituire quello installato, avviso di versione piu bassa (regole in core/vos_pacchetto.h).
// .vab, .vdb e .vde dichiarano il nome nel file; .vsb e .vae non lo dichiarano e il nome si ricava dal file.
#include "vos_pacchetto_nome.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/vos_ext.h"
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_pacchetto.h"
#include "../core/vos_util.h"
#include "../devices/vos_mod.h"
#include "../drivers/vos_drv_fs.h"
#include "vos_app.h"
#include "vos_vde.h"
#if VOS_WITH_ELF
#include "vos_elf.h"
#include "vos_elf_file.h"
#endif

namespace {

const size_t kTesta = 8192;     // la dichiarazione sta in cima, anche nei .vde (parte di testo)

struct Parti { char dir[8]; std::string_view radice; const char* ext; };

bool cartellaPacchetti(std::string_view dir) { return dir == "/bin" || dir == "/sbin" || dir == "/devs"; }

bool estensioneDichiarata(const char* ext) {
  return !strcmp(ext, VOS_EXT_APP_B) || !strcmp(ext, VOS_EXT_DRV_B) || !strcmp(ext, VOS_EXT_DRV_E);
}

bool estensioneDaFile(const char* ext) { return !strcmp(ext, VOS_EXT_SCRIPT) || !strcmp(ext, VOS_EXT_APP_E); }

// false = non e un pacchetto in una delle cartelle dei pacchetti.
bool dividi(const char* percorso, Parti& p) {
  const char* barra = strrchr(percorso, '/');
  const char* punto = strrchr(percorso, '.');
  if (!barra || !punto || punto < barra) return false;
  size_t ld = (size_t)(barra - percorso);
  if (ld >= sizeof p.dir) return false;
  memcpy(p.dir, percorso, ld); p.dir[ld] = 0;
  p.radice = std::string_view(barra + 1, (size_t)(punto - barra - 1));
  p.ext = punto;
  return cartellaPacchetti(p.dir) && (estensioneDichiarata(punto) || estensioneDaFile(punto));
}

}  // namespace

void pacchettoLeggiDati(const char* percorso, PacDati& d) {
  memset(&d, 0, sizeof d);
  VosFile f = drvFsOpen(percorso);
  if (!f) return;
  char* b = (char*)malloc(kTesta);
  if (!b) return;
  pacLeggi(std::string_view(b, f.read((uint8_t*)b, kTesta)), d);
  free(b);
}

namespace {

// Il nome corto del pacchetto: quello dichiarato, altrimenti quello ricavato dal nome del file. Vuoto se non si sa.
void nomeDelPacchetto(const char* caricato, const Parti& p, char* nome, size_t cap) {
  nome[0] = 0;
  if (estensioneDichiarata(p.ext)) {
    PacDati d;
    pacchettoLeggiDati(caricato, d);
    snprintf(nome, cap, "%s", d.nome);
  }
  if (!nome[0]) pacNomeDaFile(p.radice, nome, cap);
}

// Un driver .vdb e uno .vde con lo stesso nome non stanno insieme.
bool doppioneDriver(const char* dir, const char* nome, const char* ext) {
  if (strcmp(dir, "/devs")) return false;
  char p[FS_PATH_MAX + 1];
  if (!strcmp(ext, VOS_EXT_DRV_E)) {
    snprintf(p, sizeof p, "%s/%s%s", dir, nome, VOS_EXT_DRV_B);
    if (drvFsExists(p)) return true;
    snprintf(p, sizeof p, "%s/%s.vdp", dir, nome);
    return drvFsExists(p);
  }
  snprintf(p, sizeof p, "%s/%s%s", dir, nome, VOS_EXT_DRV_E);
  return drvFsExists(p);
}

// Tutto il file in memoria, chiuso con 0. nullptr = err dice perche. Si libera con free().
char* leggiTutto(const char* percorso, size_t tetto, const char* troppoGrande, size_t& n, ErrBuf& err) {
  VosFile f = drvFsOpen(percorso);
  if (!f) { err = tr("File non trovato"); return nullptr; }
  n = f.size();
  if (n == 0) { err = tr("File vuoto o non di testo"); return nullptr; }
  if (n > tetto) { err = troppoGrande; return nullptr; }
  char* b = (char*)malloc(n + 1);
  if (!b) { err = tr("Memoria insufficiente"); return nullptr; }
  if (f.read((uint8_t*)b, n) != n) { free(b); err = tr("Impossibile leggere"); return nullptr; }
  b[n] = 0;
  return b;
}

// Il controllo vero dei file Berry si fa all'avvio dello script: qui si esclude un file vuoto o binario.
bool scriptValido(const char* percorso, ErrBuf& err) {
  size_t n = 0;
  char* b = leggiTutto(percorso, fsTextMax(), tr("Testo troppo lungo (max 32 KB)"), n, err);
  if (!b) return false;
  bool ok = strlen(b) == n;                 // un byte a zero in mezzo: non e un testo
  if (!ok) err = tr("File vuoto o non di testo");
  free(b);
  return ok;
}

bool driverBerryValido(const char* percorso, const char* nome, ErrBuf& err) {
  size_t n = 0;
  char* b = leggiTutto(percorso, MOD_FILE_MAX, tr("File troppo grande (massimo 16 KB)"), n, err);
  if (!b) return false;
  ModInfo* m = new ModInfo();
  bool ok = modParse(std::string_view(b, n), nome, *m, err);
  delete m;
  free(b);
  return ok;
}

#if VOS_WITH_BERRY
bool appBerryValida(const char* percorso, const char* nome, ErrBuf& err) {
  size_t n = 0;
  char* b = leggiTutto(percorso, APP_FILE_MAX, tr("File troppo grande (massimo 16 KB)"), n, err);
  if (!b) return false;
  AppInfo* a = new AppInfo();
  bool ok = appParse(std::string_view(b, n), nome, *a, err);
  delete a;
  free(b);
  return ok;
}
#endif

#if VOS_WITH_ELF
bool driverElfValido(const char* percorso, const char* nome, ErrBuf& err) {
  VosElfImg im;
  int r = vos_elf_img_leggi(percorso, VDE_TEXT_MAX + 64 + vos_elf_caps()->file_max, im);
  if (r != VOS_ELF_OK) { err = vos_elf_err_testo(r); return false; }
  VdeInfo* v = new VdeInfo();
  bool ok = vdeParse(im.p, im.n, nome, *v, nullptr, err);
  delete v;
  return ok;
}

bool appElfValida(const char* percorso, ErrBuf& err) {
  int r = vos_elf_pkg_controlla(percorso, nullptr, nullptr);
  if (r != VOS_ELF_OK) { err = vos_elf_err_testo(r); return false; }
  return true;
}
#endif

// Una funzione spenta a compilazione non ha il suo controllo: quei file non si possono comunque usare.
bool pacchettoValido(const char* percorso, const char* nome, const char* ext, ErrBuf& err) {
  if (!strcmp(ext, VOS_EXT_SCRIPT)) return scriptValido(percorso, err);
  if (!strcmp(ext, VOS_EXT_DRV_B)) return driverBerryValido(percorso, nome, err);
#if VOS_WITH_BERRY
  if (!strcmp(ext, VOS_EXT_APP_B)) return appBerryValida(percorso, nome, err);
#endif
#if VOS_WITH_ELF
  if (!strcmp(ext, VOS_EXT_DRV_E)) return driverElfValido(percorso, nome, err);
  if (!strcmp(ext, VOS_EXT_APP_E)) return appElfValida(percorso, err);
#endif
  return true;
}

void confrontaVersione(const PacDati& nuovo, const char* installato, ErrBuf& avviso) {
  PacDati vecchio;
  pacchettoLeggiDati(installato, vecchio);
  if (pacVersionePiuBassa(nuovo.versione, vecchio.versione)) {
    avviso.setf(tr("Versione %s piu vecchia di quella installata (%s)"), nuovo.versione, vecchio.versione);
  }
}

}  // namespace

bool pacchettoDaProteggere(const char* percorso) {
  Parti p;
  return dividi(percorso, p) && drvFsExists(percorso);
}

void pacchettoAvviso(const char* installato, std::string_view nuovo, ErrBuf& avviso) {
  PacDati d;
  pacLeggi(nuovo.substr(0, kTesta), d);
  confrontaVersione(d, installato, avviso);
}

bool pacchettoCaricato(const char* caricato, const char* nomeFile, ErrBuf& err, ErrBuf& avviso) {
  Parti p;
  if (!dividi(nomeFile, p)) return true;

  char nome[25];
  nomeDelPacchetto(caricato, p, nome, sizeof nome);
  bool nomeLungo = p.radice != nome && utilNameOk(nome, strlen(nome));     // altrimenti il nome resta quello del file, da mostrare come non valido

  if (nomeLungo && doppioneDriver(p.dir, nome, p.ext)) {
    drvFsRemove(caricato);
    err = tr("Esiste gia un driver con questo nome");
    return false;
  }
  char finale[FS_PATH_MAX + 1];
  if (nomeLungo) snprintf(finale, sizeof finale, "%s/%s%s", p.dir, nome, p.ext);
  else snprintf(finale, sizeof finale, "%s", nomeFile);
  if (!strcmp(caricato, finale)) return true;                              // nessun pacchetto installato da sostituire

  if (drvFsExists(finale)) {
    char atteso[MOD_NAME_BUF];
    snprintf(atteso, sizeof atteso, "%.*s", (int)p.radice.size(), p.radice.data());
    if (!pacchettoValido(caricato, nomeLungo ? nome : atteso, p.ext, err)) {
      drvFsRemove(caricato);
      return false;
    }
    PacDati d;
    pacchettoLeggiDati(caricato, d);
    confrontaVersione(d, finale, avviso);
  }
  // Un passo solo: LittleFS sostituisce in modo atomico (lfs_rename), se manca la corrente resta il vecchio o il nuovo, mai nessuno dei due.
  if (!drvFsRename(caricato, finale)) {
    drvFsRemove(caricato);
    avviso.clear();
    err = tr("Impossibile scrivere");
    return false;
  }
  return true;
}
