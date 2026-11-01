// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_file.h
// STRATO 6 (packages): dove stanno i moduli ELF (.vae) e come si leggono. Un modulo installato e un file NOME.vae nella cartella delle
// app (di solito /bin); per le prove e per "run" con un percorso si accettano anche /apps, /devs (un livello) e la microSD.
// Lo usano la shell (vesevos-elf) e la pagina (/api/elf). Non carica niente: legge e controlla.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "vos_elf_types.h"

#if VOS_WITH_ELF
// Il file letto per intero in un blocco di memoria allineato a 4 (come lo da malloc). Si libera da solo, a meno che lo si ceda.
struct VosElfImg {
  uint8_t* p = nullptr;
  size_t   n = 0;
  VosElfImg() = default;
  VosElfImg(const VosElfImg&) = delete;
  VosElfImg& operator=(const VosElfImg&) = delete;
  ~VosElfImg();
  uint8_t* cedi() { uint8_t* x = p; p = nullptr; n = 0; return x; }   // chi lo riceve lo libera con free()
};

// Un nome semplice ("sensore") o un percorso ("/bin/sensore.vae", "/media/sd/x.vae"). Scrive in out il percorso pulito.
// 0 oppure VOS_ELF_E_NOME / VOS_ELF_E_PERCORSO. Il nome del modulo (senza cartelle e senza estensione) va in nome (VOS_ELF_NOME_MAX + 1 byte).
int  vos_elf_risolvi(const char* in, char* out, size_t cap, char* nome);

// Legge il file. 0 oppure VOS_ELF_E_PERCORSO / E_FILE / E_GRANDE / E_MEM.
int  vos_elf_img_leggi(const char* percorso, size_t tetto, VosElfImg& im);

// Controllo del file senza caricarlo e senza eseguirlo (vos_elf_valida con tutti i gruppi di primitive: qui si guarda il file, non i diritti).
// 0 oppure il VOS_ELF_E_*; byte e ram_stima sono scritti se il file si e potuto leggere.
int  vos_elf_pkg_controlla(const char* percorso, size_t* byte, uint32_t* ram_stima);

// I gruppi di primitive che il modulo chiede (dai simboli che importa), separati da spazio, senza il gruppo base ("log sd"). Vuoto = nessuno.
// 0 oppure VOS_ELF_E_MAGIC / E_CORTO / E_ARG. Il file non e controllato a fondo: serve solo a mostrare e concedere i permessi.
// dati (facoltativo): true se il modulo importa le primitive vos_data_* e quindi salva dati (la pagina App allora lascia scegliere flash o microSD).
int  vos_elf_gruppi_richiesti(const uint8_t* p, size_t len, char* out, size_t cap, bool* dati = nullptr);
int  vos_elf_pkg_gruppi(const char* percorso, char* out, size_t cap, bool* dati = nullptr);   // lo stesso, leggendo il file (0 oppure l'errore di lettura)

// Moduli installati (NOME.vae nella cartella delle app). cb e chiamata per ogni file: nome senza estensione e byte.
typedef void (*VosElfPkgCb)(const char* nome, size_t byte, void* ctx);
int  vos_elf_pkg_elenco(VosElfPkgCb cb, void* ctx);       // quanti ne ha trovati
#endif
