// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_check.cpp
// STRATO 6 (packages), LIVELLO A dei moduli ELF: validazione dell'ELF PRIMA di caricarlo. Controlla magic, classe, macchina, tipo,
// tetti, lunghezza esatta, allineamento e somma delle sezioni ALLOC. Legge il buffer solo con controlli di limite e non alloca memoria.
// E un primo filtro nostro (codici nostri, indipendente dal caricatore): il caricatore ne fa un secondo, piu fitto.
#include "vos_elf_int.h"

#if VOS_WITH_ELF
#include <string.h>

#define EH_SIZE 52
#define SH_SIZE 40
#define SHT_STRTAB 3
#define SHT_NOBITS 8
#define SHF_ALLOC  2

static uint16_t rd16(const uint8_t* p, size_t o) { uint16_t v; memcpy(&v, p + o, 2); return v; }
static uint32_t rd32(const uint8_t* p, size_t o) { uint32_t v; memcpy(&v, p + o, 4); return v; }

struct sez_t { uint32_t name, type, flags, addr, offset, size, addralign; };

// Le sezioni che ci interessano per i controlli successivi.
struct Trovate {
  sez_t text, rod;
  bool  haText, haRod;
};

static void leggiSez(const uint8_t* p, uint32_t shoff, uint32_t i, sez_t* s) {
  size_t o = (size_t)shoff + (size_t)i * SH_SIZE;
  s->name = rd32(p, o);
  s->type = rd32(p, o + 4);
  s->flags = rd32(p, o + 8);
  s->addr = rd32(p, o + 12);
  s->offset = rd32(p, o + 16);
  s->size = rd32(p, o + 20);
  s->addralign = rd32(p, o + 32);
}

static int nelFile(size_t len, uint32_t off, uint32_t size) {
  return off <= len && size <= len - off;
}

// Intestazione: magic, classe, macchina, tipo, flag, tabella sezioni.
static int controllaIntestazione(const uint8_t* p, size_t len, const vos_elf_caps_t* c) {
  if (memcmp(p, "\x7f" "ELF", 4) != 0) return VOS_ELF_E_MAGIC;
  if (p[4] != 1 || p[5] != (c->little_endian ? 1 : 2) || p[6] != 1 || rd32(p, 20) != 1) return VOS_ELF_E_CLASSE;
  if (rd16(p, 18) != c->machine) return VOS_ELF_E_MACCHINA;
  uint16_t tipo = rd16(p, 16);
  if (tipo != 2 && tipo != 3) return VOS_ELF_E_TIPO;
  if (!vos_drv_elf_flags_ok(rd32(p, 36))) return VOS_ELF_E_ABI;
  uint16_t shnum = rd16(p, 48);
  if (rd16(p, 40) != EH_SIZE || rd16(p, 46) != SH_SIZE || shnum == 0 || shnum > VOS_ELF_SEZ_MAX) return VOS_ELF_E_STRUTTURA;
  uint32_t shoff = rd32(p, 32);
  if (shoff & 3u) return VOS_ELF_E_STRUTTURA;
  if (shoff > len || (size_t)shnum * SH_SIZE > len - shoff) return VOS_ELF_E_CORTO;   // tabella oltre la fine: file troncato
  if (rd16(p, 50) == 0 || rd16(p, 50) >= shnum) return VOS_ELF_E_STRUTTURA;
  return 0;
}

// Tabella dei nomi: dentro il file e terminata da NUL (cosi' i confronti non escono mai dal buffer).
static int tabellaNomi(const uint8_t* p, size_t len, uint32_t shoff, uint16_t idx, sez_t* st) {
  leggiSez(p, shoff, idx, st);
  if (st->type != SHT_STRTAB || st->size == 0 || !nelFile(len, st->offset, st->size)) return VOS_ELF_E_STRUTTURA;
  return p[st->offset + st->size - 1] == 0 ? 0 : VOS_ELF_E_STRUTTURA;
}

static int nomeE(const uint8_t* p, const sez_t* st, uint32_t nome, const char* atteso) {
  return nome < st->size && strcmp((const char *)p + st->offset + nome, atteso) == 0;
}

// Sezioni: dentro il file, allineamenti sensati, somma della RAM; ricorda .text e .rodata.
static int controllaSezioni(const uint8_t* p, size_t len, uint32_t limite, vos_elf_info_t* info, Trovate* tv) {
  uint32_t shoff = rd32(p, 32);
  uint16_t shnum = rd16(p, 48);
  sez_t st;
  int e = tabellaNomi(p, len, shoff, rd16(p, 50), &st);
  if (e) return e;
  uint64_t ram = 0;
  for (uint32_t i = 0; i < shnum; i++) {
    sez_t s;
    leggiSez(p, shoff, i, &s);
    if (s.type != SHT_NOBITS && !nelFile(len, s.offset, s.size)) return VOS_ELF_E_CORTO;
    if (!(s.flags & SHF_ALLOC)) continue;
    uint32_t al = s.addralign > 1 ? s.addralign : 1;
    if ((al & (al - 1)) || al > 4096) return VOS_ELF_E_STRUTTURA;
    if ((uint64_t)s.addr + s.size > 0xFFFFFFFFull) return VOS_ELF_E_STRUTTURA;
    ram += (uint64_t)s.size + (al - 1);
    if (ram > limite) return VOS_ELF_E_RAM;
    if (nomeE(p, &st, s.name, ".text")) { tv->text = s; tv->haText = true; }
    else if (nomeE(p, &st, s.name, ".rodata")) { tv->rod = s; tv->haRod = true; }
  }
  info->ram_stima = (uint32_t)ram;
  info->n_sezioni = shnum;
  return 0;
}

// Caso pericoloso: .text non multiplo di 4 con .rodata subito dopo (con un caricatore senza la patch 1 il simbolo finiva nel posto sbagliato).
static int controllaSovrapposizione(const vos_elf_caps_t* c, const sez_t* text, const sez_t* rod, bool haRod, vos_elf_info_t* info) {
  uint32_t fine = text->addr + text->size;
  uint32_t fine4 = (fine + 3u) & ~3u;
  info->rodata_sovrapposto = (haRod && (fine & 3u) && rod->addr >= fine && rod->addr < fine4) ? 1 : 0;
  return (info->rodata_sovrapposto && !c->text_esatto) ? VOS_ELF_E_ALLINEA : 0;
}

static int controllaManifest(const vos_elf_caps_t* c, const vos_elf_param_t* par) {
  if (par->arch && par->arch != c->arch) return VOS_ELF_E_ARCH;
  if (par->serve_psram && !c->ha_psram) return VOS_ELF_E_CAPS;
  if (par->api > c->api_version) return VOS_ELF_E_API;
  return 0;
}

int vos_elf_ispeziona(const uint8_t* p, size_t len, const vos_elf_caps_t* c, const vos_elf_param_t* par, vos_elf_info_t* info) {
  if (!p || !c || !par || !info) return VOS_ELF_E_ARG;
  memset(info, 0, sizeof(*info));
  int e = controllaManifest(c, par);
  if (e) return e;
  uint32_t fmax = (par->file_max && par->file_max < c->file_max) ? par->file_max : c->file_max;
  uint32_t rmax = (par->ram_max && par->ram_max < c->ram_max_modulo) ? par->ram_max : c->ram_max_modulo;
  if (((uintptr_t)p & 3u) != 0) return VOS_ELF_E_ALLINEA;
  if (len > fmax) return VOS_ELF_E_GRANDE;
  if (len < EH_SIZE) return VOS_ELF_E_CORTO;
  e = controllaIntestazione(p, len, c);
  if (e) return e;
  Trovate tv = {};
  e = controllaSezioni(p, len, rmax, info, &tv);
  if (e) return e;
  uint32_t ent = rd32(p, 24);
  info->entry_vaddr = ent;
  if (!tv.haText) return VOS_ELF_E_STRUTTURA;
  if (ent < tv.text.addr || ent - tv.text.addr >= tv.text.size) return VOS_ELF_E_INGRESSO;
  return controllaSovrapposizione(c, &tv.text, &tv.rod, tv.haRod, info);
}
#endif
