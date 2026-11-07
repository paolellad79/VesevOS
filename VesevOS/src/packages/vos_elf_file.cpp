// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_file.cpp (vedi vos_elf_file.h)
#include "vos_elf_file.h"
#include "../core/vos_ext.h"
#include "../core/vos_paths.h"
#include "vos_progext.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vos_elf.h"
#include "vos_elf_quar.h"
#include "../core/vos_files.h"
#include "../core/vos_pacchetto.h"
#include "../drivers/vos_drv_fs.h"
#include "../core/vos_layout.h"
#include "../devices/vos_sd.h"

namespace {

const char* const kSdDir = "/media/sd/";
bool terminaCon(const char* s, const char* ext) {
  size_t n = strlen(s), e = strlen(ext);
  return n >= e && strcmp(s + n - e, ext) == 0;
}

// Finisce per .vae, per il vecchio .vel o per .elf?
bool estModulo(const char* s) { return progHaExt(s, PEXT_APP_E) || terminaCon(s, ".elf"); }

// Cartelle da cui si accetta un modulo: la cartella delle app (di solito /bin), quelle vecchie /apps e /devs, un livello solo.
bool cartellaOk(const char* path) {
  const char* const dirs[] = { layoutPath(LAY_APPS), "/apps", "/devs" };
  for (const char* d : dirs) {
    size_t n = strlen(d);
    if (!strncmp(path, d, n) && path[n] == '/' && path[n + 1] && !strchr(path + n + 1, '/')) return true;
  }
  return false;
}

// Dal percorso al nome del modulo: "/bin/sensore.vae" (o il vecchio .vel, o .elf) -> "sensore".
bool nomeDaPercorso(const char* path, char* nome) {
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;
  size_t n = strlen(base);
  if (estModulo(base)) n -= 4;                                // tutte le estensioni dei moduli hanno 4 caratteri
  if (pacNomeDaFile(std::string_view(base, n), nome, VOS_ELF_NOME_MAX + 1)) return vos_elf_nome_valido(nome);   // sensore-S3-1.2.vae -> sensore
  if (n == 0 || n > VOS_ELF_NOME_MAX) return false;
  memcpy(nome, base, n);
  nome[n] = 0;
  return vos_elf_nome_valido(nome);
}

int leggiFs(const char* path, size_t tetto, VosElfImg& im) {
  if (fsProtected(path) || pathSdRel(path)) return VOS_ELF_E_PERCORSO;       // i moduli si leggono solo dalla memoria interna (la microSD ha la sua via: bus SD)
  VosFile f = drvFsOpen(path);
  if (!f || f.isDirectory()) return VOS_ELF_E_FILE;
  size_t len = f.size();
  if (len == 0) return VOS_ELF_E_FILE;
  if (len > tetto) return VOS_ELF_E_GRANDE;
  im.p = (uint8_t*)malloc(len);
  if (!im.p) return VOS_ELF_E_MEM;
  im.n = f.read(im.p, len);
  return im.n == len ? VOS_ELF_OK : VOS_ELF_E_FILE;
}

#if VOS_WITH_SD
// Dalla microSD, a blocchi (sdReadAt rispetta il controllo del percorso e conta come operazione in corso).
int leggiSd(const char* rel, size_t tetto, VosElfImg& im) {
  char err[SD_ERR_MAX];
  im.p = (uint8_t*)malloc(tetto + 1);                        // un byte in piu: se si riempie, il file e oltre il tetto
  if (!im.p) return VOS_ELF_E_MEM;
  while (im.n <= tetto) {
    size_t want = tetto + 1 - im.n;
    if (want > SD_IO_MAX) want = SD_IO_MAX;
    int got = sdReadAt(rel, (uint32_t)im.n, im.p + im.n, want, err, sizeof err);
    if (got < 0) return VOS_ELF_E_FILE;
    if (got == 0) break;
    im.n += (size_t)got;
  }
  if (im.n > tetto) return VOS_ELF_E_GRANDE;
  return im.n == 0 ? VOS_ELF_E_FILE : VOS_ELF_OK;
}
#endif

}  // namespace

VosElfImg::~VosElfImg() { free(p); }

int vos_elf_risolvi(const char* in, char* out, size_t cap, char* nome) {
  if (!in || !in[0]) return VOS_ELF_E_NOME;
  if (!strchr(in, '/') && !estModulo(in)) {                                              // un nome semplice: sta in /bin
    if (!vos_elf_nome_valido(in)) return VOS_ELF_E_NOME;
    progPercorsoAvvio(out, cap, layoutPath(LAY_APPS), in, PEXT_APP_E);                         // .vae, o il vecchio .vel se c'e solo quello
    snprintf(nome, VOS_ELF_NOME_MAX + 1, "%s", in);
    return VOS_ELF_OK;
  }
  if (!fsCleanTo(in, out, cap)) return VOS_ELF_E_PERCORSO;
  if (!estModulo(out)) return VOS_ELF_E_PERCORSO;
  bool sd = !strncmp(out, kSdDir, strlen(kSdDir));
  if (!sd && !cartellaOk(out)) return VOS_ELF_E_PERCORSO;
  return nomeDaPercorso(out, nome) ? VOS_ELF_OK : VOS_ELF_E_NOME;
}

int vos_elf_img_leggi(const char* percorso, size_t tetto, VosElfImg& im) {
  if (!strncmp(percorso, kSdDir, strlen(kSdDir))) {
#if VOS_WITH_SD
    return leggiSd(percorso + strlen("/media/sd"), tetto, im);
#else
    return VOS_ELF_E_PERCORSO;
#endif
  }
  return leggiFs(percorso, tetto, im);
}

int vos_elf_pkg_controlla(const char* percorso, size_t* byte, uint32_t* ram_stima) {
  VosElfImg im;
  int e = vos_elf_img_leggi(percorso, vos_elf_caps()->file_max, im);
  if (e != VOS_ELF_OK) return e;
  if (byte) *byte = im.n;
  vos_elf_param_t p;
  memset(&p, 0, sizeof p);
  p.ruolo = ROLE_ADMIN;
  p.use = "log sd dev";
  vos_elf_info_t info;
  memset(&info, 0, sizeof info);
  e = vos_elf_valida(im.p, im.n, &p, &info);
  if (ram_stima) *ram_stima = info.ram_stima;
  return e;
}

int vos_elf_pkg_gruppi(const char* percorso, char* out, size_t cap, bool* dati) {
  if (dati) *dati = false;
  if (out && cap) out[0] = 0;
  VosElfImg im;
  int e = vos_elf_img_leggi(percorso, vos_elf_caps()->file_max, im);
  return e != VOS_ELF_OK ? e : vos_elf_gruppi_richiesti(im.p, im.n, out, cap, dati);
}

int vos_elf_pkg_elenco(VosElfPkgCb cb, void* ctx) {
  VosFile dir = drvFsOpen(layoutPath(LAY_APPS));
  if (!dir || !dir.isDirectory()) return 0;
  int n = 0;
  for (VosFile f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const char* nm = f.name();
    if (f.isDirectory() || !progHaExt(nm, PEXT_APP_E)) continue;
    char nome[VOS_ELF_NOME_MAX + 1];
    if (!nomeDaPercorso(nm, nome)) continue;                 // nome non valido: non e un modulo installabile
    if (terminaCon(nm, PEXT_APP_E.vecchio) && progVecchioCoperto(layoutPath(LAY_APPS), nome, PEXT_APP_E)) continue;   // c'e anche il .vae: vale quello
    cb(nome, f.size(), ctx);
    n++;
  }
  return n;
}
#endif
