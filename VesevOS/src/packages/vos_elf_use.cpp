// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_use.cpp
// STRATO 6 (packages): quali GRUPPI di primitive chiede un modulo ELF. Un'app ELF non ha un manifest: i permessi che chiede si leggono dai
// simboli che importa ("vos_sd_status" = gruppo sd). Cosi l'utente li vede accanto al nome e l'avvio concede proprio quelli (poi la
// pagina Funzionalita e il ruolo possono solo toglierne). Legge il buffer solo con controlli di limite; non alloca memoria.
#include "vos_elf_file.h"

#if VOS_WITH_ELF
#include <string.h>
#include "vos_prim.h"

namespace {

const uint32_t SHT_SYMTAB = 2, SHT_DYNSYM = 11;
const size_t   SH_SIZE = 40, SYM_SIZE = 16;

uint16_t rd16(const uint8_t* p, size_t o) { uint16_t v; memcpy(&v, p + o, 2); return v; }
uint32_t rd32(const uint8_t* p, size_t o) { uint32_t v; memcpy(&v, p + o, 4); return v; }
bool dentro(size_t len, uint32_t off, uint32_t size) { return off <= len && size <= len - off; }

// Aggiunge il gruppo a out se non c'e gia. false = non entra.
bool aggiungi(char* out, size_t cap, const char* gruppo) {
  size_t gl = strlen(gruppo), n = strlen(out);
  for (const char* q = out; *q;) {
    while (*q == ' ') q++;
    const char* s = q;
    while (*q && *q != ' ') q++;
    if ((size_t)(q - s) == gl && !strncmp(s, gruppo, gl)) return true;
  }
  if (n + gl + (n ? 1 : 0) + 1 > cap) return false;
  if (n) out[n++] = ' ';
  memcpy(out + n, gruppo, gl + 1);
  return true;
}

// Una tabella dei simboli: i simboli senza definizione (shndx 0) che sono primitive, nei gruppi di out.
// dati (se non nullo) diventa true se il modulo importa una primitiva vos_data_*: il modulo salva dati (dove lo sceglie l'utente, vedi vos_prog_data.h).
void guardaTabella(const uint8_t* p, size_t len, uint32_t shoff, uint32_t i, char* out, size_t cap, bool* dati) {
  size_t o = (size_t)shoff + (size_t)i * SH_SIZE;
  uint32_t off = rd32(p, o + 16), size = rd32(p, o + 20), link = rd32(p, o + 24);
  if (!dentro(len, off, size)) return;
  uint16_t shnum = rd16(p, 48);
  if (link >= shnum) return;
  size_t ol = (size_t)shoff + (size_t)link * SH_SIZE;
  uint32_t soff = rd32(p, ol + 16), ssize = rd32(p, ol + 20);
  if (!dentro(len, soff, ssize) || ssize == 0 || p[soff + ssize - 1] != 0) return;      // stringhe chiuse da NUL: i confronti restano dentro
  for (uint32_t k = 0; k + SYM_SIZE <= size; k += SYM_SIZE) {
    uint32_t nome = rd32(p, off + k);
    if (rd16(p, off + k + 14) != 0 || nome >= ssize) continue;                          // definito dal modulo stesso, o nome fuori dalla tabella
    const char* gruppo = nullptr;
    if (vos_prim_trova_elf((const char*)p + soff + nome, &gruppo) && strcmp(gruppo, VOS_PRIM_GRUPPO_BASE)) {
      aggiungi(out, cap, gruppo);
      if (dati && !strncmp((const char*)p + soff + nome, "vos_data_", 9)) *dati = true;
    }
  }
}

}  // namespace

int vos_elf_gruppi_richiesti(const uint8_t* p, size_t len, char* out, size_t cap, bool* dati) {
  if (!out || cap == 0) return VOS_ELF_E_ARG;
  out[0] = 0;
  if (dati) *dati = false;
  if (!p || len < 52 || memcmp(p, "\x7f" "ELF", 4) != 0) return VOS_ELF_E_MAGIC;
  uint32_t shoff = rd32(p, 32);
  uint16_t shnum = rd16(p, 48);
  if (rd16(p, 46) != SH_SIZE || shnum == 0 || shoff > len || (size_t)shnum * SH_SIZE > len - shoff) return VOS_ELF_E_CORTO;
  for (uint32_t i = 0; i < shnum; i++) {
    uint32_t tipo = rd32(p, (size_t)shoff + (size_t)i * SH_SIZE + 4);
    if (tipo == SHT_SYMTAB || tipo == SHT_DYNSYM) guardaTabella(p, len, shoff, i, out, cap, dati);
  }
  return VOS_ELF_OK;
}
#endif
