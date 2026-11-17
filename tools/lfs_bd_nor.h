// SPDX-License-Identifier: GPL-3.0-or-later
// Blocco di memoria in RAM che si comporta come una flash NOR, per i programmi sul computer che usano il lfs.c vero.
// - cancellare un blocco lo riempie di 0xFF (la rambd di littlefs non cancella: lascia i dati vecchi);
// - programmare puo solo portare i bit da 1 a 0: se un byte dovrebbe tornare a 1 e un errore (la flash vera lo ignorerebbe).
// Usato da tools/lfs_immagine.c (generatore dell'immagine) e dalle prove in build/tests/risorse-immagine/.
#ifndef VOS_LFS_BD_NOR_H
#define VOS_LFS_BD_NOR_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lfs.h"

#define NOR_BLOCCO 4096

typedef struct {
  uint8_t* dati;
  uint32_t blocchi;
  uint32_t scritture_vietate;      // quante volte si e provato a riportare un bit a 1 senza cancellare
} NorBd;

static int nor_leggi(const struct lfs_config* c, lfs_block_t b, lfs_off_t o, void* buf, lfs_size_t n) {
  NorBd* bd = (NorBd*)c->context;
  if (b >= bd->blocchi || o + n > NOR_BLOCCO) return LFS_ERR_IO;
  memcpy(buf, bd->dati + (size_t)b * NOR_BLOCCO + o, n);
  return 0;
}

static int nor_scrivi(const struct lfs_config* c, lfs_block_t b, lfs_off_t o, const void* buf, lfs_size_t n) {
  NorBd* bd = (NorBd*)c->context;
  if (b >= bd->blocchi || o + n > NOR_BLOCCO) return LFS_ERR_IO;
  uint8_t* dove = bd->dati + (size_t)b * NOR_BLOCCO + o;
  const uint8_t* da = (const uint8_t*)buf;
  for (lfs_size_t i = 0; i < n; i++) {
    if ((uint8_t)(dove[i] & da[i]) != da[i]) {
      bd->scritture_vietate++;
      return LFS_ERR_IO;
    }
    dove[i] &= da[i];
  }
  return 0;
}

static int nor_cancella(const struct lfs_config* c, lfs_block_t b) {
  NorBd* bd = (NorBd*)c->context;
  if (b >= bd->blocchi) return LFS_ERR_IO;
  memset(bd->dati + (size_t)b * NOR_BLOCCO, 0xFF, NOR_BLOCCO);
  return 0;
}

static int nor_sync(const struct lfs_config* c) {
  (void)c;
  return 0;
}

// Parametri di LittleFS uguali a quelli del firmware (idf/managed_components/joltwallet__littlefs/src/esp_littlefs.c:1360-1369
// e sdkconfig CONFIG_LITTLEFS_*): blocco 4096, lettura e scrittura 128, cache 512, lookahead 128, block_cycles 512.
static void nor_config(struct lfs_config* cfg, NorBd* bd, uint32_t block_count) {
  memset(cfg, 0, sizeof *cfg);
  cfg->context = bd;
  cfg->read = nor_leggi;
  cfg->prog = nor_scrivi;
  cfg->erase = nor_cancella;
  cfg->sync = nor_sync;
  cfg->read_size = 128;
  cfg->prog_size = 128;
  cfg->block_size = NOR_BLOCCO;
  cfg->block_count = block_count;
  cfg->cache_size = 512;
  cfg->lookahead_size = 128;
  cfg->block_cycles = 512;
}

#endif
