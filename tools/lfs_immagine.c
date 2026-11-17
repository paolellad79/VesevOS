// SPDX-License-Identifier: GPL-3.0-or-later
// Crea un'immagine LittleFS con i file di /usr/share/vesevos, per l'installatore del sito (tools/mkimmaginerisorse.py).
// Si compila con il lfs.c del componente joltwallet/littlefs 1.22.3 (lo stesso del firmware) e i parametri di tools/lfs_bd_nor.h.
//
// Uso: lfs_immagine <block_count> <uscita.bin> <file> [<file> ...]
//   I file si scrivono in /usr/share/vesevos/<nome del file>, nell'ordine dato (chi chiama li ordina per nome).
//   Senza attributo di data: nessuna data inventata, la stessa immagine a ogni giro.
//   Scrive su <uscita.bin> tutti i block_count blocchi (cancellati a 0xFF dove non c'e niente) e stampa "usati <n>".
//   Esce con 1 se qualcosa non va (per esempio spazio finito: -28).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lfs_bd_nor.h"

static lfs_t lfs;

static int crea_cartelle(void) {
  const char* percorsi[] = { "/usr", "/usr/share", "/usr/share/vesevos" };
  for (int i = 0; i < 3; i++) {
    int r = lfs_mkdir(&lfs, percorsi[i]);
    if (r < 0) {
      fprintf(stderr, "mkdir %s: errore %d\n", percorsi[i], r);
      return r;
    }
  }
  return 0;
}

static int scrivi_file(const char* nome, const uint8_t* dati, size_t n) {
  char percorso[300];
  snprintf(percorso, sizeof percorso, "/usr/share/vesevos/%s", nome);
  lfs_file_t f;
  int r = lfs_file_open(&lfs, &f, percorso, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL);
  if (r < 0) {
    fprintf(stderr, "apertura %s: errore %d\n", percorso, r);
    return r;
  }
  size_t fatti = 0;
  while (fatti < n) {
    size_t pezzo = n - fatti > 4096 ? 4096 : n - fatti;
    int w = lfs_file_write(&lfs, &f, dati + fatti, pezzo);
    if (w < 0) {
      fprintf(stderr, "scrittura %s: errore %d\n", percorso, w);
      lfs_file_close(&lfs, &f);
      return w;
    }
    fatti += (size_t)w;
  }
  r = lfs_file_close(&lfs, &f);
  if (r < 0) fprintf(stderr, "chiusura %s: errore %d\n", percorso, r);
  return r;
}

static const char* nome_base(const char* percorso) {
  const char* s = strrchr(percorso, '/');
  return s ? s + 1 : percorso;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "uso: %s <block_count> <uscita.bin> <file>...\n", argv[0]);
    return 1;
  }
  uint32_t blocchi = (uint32_t)strtoul(argv[1], NULL, 10);
  if (blocchi < 8 || blocchi > 160) {
    fprintf(stderr, "block_count %u fuori da 8..160\n", blocchi);
    return 1;
  }
  NorBd bd;
  bd.blocchi = blocchi;
  bd.scritture_vietate = 0;
  bd.dati = (uint8_t*)malloc((size_t)blocchi * NOR_BLOCCO);
  if (!bd.dati) return 1;
  memset(bd.dati, 0xFF, (size_t)blocchi * NOR_BLOCCO);
  struct lfs_config cfg;
  nor_config(&cfg, &bd, blocchi);
  if (lfs_format(&lfs, &cfg) < 0 || lfs_mount(&lfs, &cfg) < 0) {
    fprintf(stderr, "formattazione o montaggio falliti\n");
    return 1;
  }
  if (crea_cartelle() < 0) return 1;
  for (int i = 3; i < argc; i++) {
    FILE* f = fopen(argv[i], "rb");
    if (!f) {
      fprintf(stderr, "non si apre %s\n", argv[i]);
      return 1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* dati = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
    if (!dati || (n > 0 && fread(dati, 1, (size_t)n, f) != (size_t)n)) {
      fprintf(stderr, "lettura di %s fallita\n", argv[i]);
      return 1;
    }
    fclose(f);
    int r = scrivi_file(nome_base(argv[i]), dati, (size_t)n);
    free(dati);
    if (r < 0) return 1;
  }
  int usati = (int)lfs_fs_size(&lfs);
  if (lfs_unmount(&lfs) < 0 || usati < 0) return 1;
  if (bd.scritture_vietate) {
    fprintf(stderr, "scritture vietate (bit da 0 a 1 senza cancellare): %u\n", bd.scritture_vietate);
    return 1;
  }
  FILE* u = fopen(argv[2], "wb");
  if (!u || fwrite(bd.dati, 1, (size_t)blocchi * NOR_BLOCCO, u) != (size_t)blocchi * NOR_BLOCCO) {
    fprintf(stderr, "scrittura di %s fallita\n", argv[2]);
    return 1;
  }
  fclose(u);
  printf("usati %d\n", usati);
  return 0;
}
