// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_elf.cpp (strato 1: driver dei moduli ELF, LIVELLO B; vedi vos_drv_elf.h)
// Il caricatore (third_party/elf_loader, con le nostre due patch) e la memoria eseguibile: qui e l'UNICO posto con esp_elf.h e FreeRTOS per gli ELF.
// Cio che cambia da chip a chip sta qui o in boards/: macchina, ABI, PSRAM, tetti.
#include "vos_drv_elf.h"

#if VOS_WITH_ELF
#include <errno.h>
#include <string.h>
#include "esp_elf.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "vos_drv_heap.h"
#include "../boards/vos_board.h"

struct vos_drv_elf { esp_elf_t elf; };

// Blocco STATICO: nessuna allocazione (un blocco creato alla prima chiamata cadrebbe dentro la misura della memoria dei test).
static StaticSemaphore_t s_mtxBuf;
static SemaphoreHandle_t s_mtx;

int vos_drv_elf_init(vos_drv_elf_resolver_t risolutore) {
  if (!risolutore) return -1;
  if (!s_mtx) s_mtx = xSemaphoreCreateMutexStatic(&s_mtxBuf);
  if (!s_mtx) return -1;
  elf_set_symbol_resolver(risolutore);
  return 0;
}

int vos_drv_elf_lock(uint32_t ms) {
  if (!s_mtx) return -1;
  return xSemaphoreTake(s_mtx, pdMS_TO_TICKS(ms)) == pdTRUE ? 0 : -1;
}

void vos_drv_elf_unlock() {
  if (s_mtx) xSemaphoreGive(s_mtx);
}

void vos_drv_elf_caps(vos_elf_caps_t* c) {
  memset(c, 0, sizeof(*c));
  c->arch = VOS_ELF_ARCH;
  c->little_endian = 1;
  c->abi = VOS_ELF_ABI;
  c->machine = VOS_ELF_MACHINE;
  c->ha_psram = (VOS_ELF_HAS_PSRAM && drvPsramTotal() > 0) ? 1 : 0;
  c->esegue_da_psram = (c->ha_psram && VOS_ELF_EXEC_PSRAM) ? 1 : 0;
  c->text_esatto = 1;                      // il caricatore nostro ha la patch 1 (third_party/elf_loader/patch/0001-text-esatto.patch)
  c->ram_max_modulo = VOS_ELF_RAM_MAX_MODULE;
  c->file_max = VOS_ELF_FILE_MAX;
}

int vos_drv_elf_flags_ok(uint32_t e_flags) {
#if CONFIG_IDF_TARGET_ARCH_RISCV
  return (e_flags & 0x6u) == 0;            // EF_RISCV_FLOAT_ABI: solo "soft" (i chip RISC-V di Espressif non hanno la FPU)
#else
  (void)e_flags;                           // Xtensa: nessun controllo sulle flag
  return 1;
#endif
}

#if CONFIG_IDF_TARGET_ARCH_XTENSA
extern "C" float __divsf3(float a, float b);   // aiuto del compilatore: questo chip non divide in hardware
#endif

uintptr_t vos_drv_elf_aiuto(const char* nome) {
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  if (!strcmp(nome, "__divsf3")) return (uintptr_t)__divsf3;
#else
  (void)nome;                              // RISC-V: da riempire con gli aiuti veri (moltiplicazione e divisione software) dopo la prova sul chip
#endif
  return 0;
}

static int mappaErrore(int r) {
  if (r == -ENOSYS) return VOS_DRV_ELF_SIMBOLO;
  if (r == -ENOMEM) return VOS_DRV_ELF_MEM;
  return VOS_DRV_ELF_ALTRO;
}

int vos_drv_elf_carica(const uint8_t* p, size_t n, vos_drv_elf_t** h, vos_drv_elf_entry_t* entry) {
  *h = nullptr;
  *entry = nullptr;
  vos_drv_elf_t* x = (vos_drv_elf_t*)heap_caps_calloc(1, sizeof(*x), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!x) return VOS_DRV_ELF_MEM;
  int r = esp_elf_init(&x->elf);
  if (r == 0) r = esp_elf_relocate(&x->elf, p, n);
  if (r == 0 && !x->elf.entry) r = -EINVAL;
  if (r != 0) {                            // scarico sempre garantito: deinit libera cio che c'e ed e innocuo se non c'e nulla
    esp_elf_deinit(&x->elf);
    heap_caps_free(x);
    return mappaErrore(r);
  }
  *h = x;
  *entry = (vos_drv_elf_entry_t)x->elf.entry;
  return VOS_DRV_ELF_OK;
}

void vos_drv_elf_scarica(vos_drv_elf_t* h) {
  if (!h) return;
  esp_elf_deinit(&h->elf);
  heap_caps_free(h);
}

void vos_drv_elf_info(const vos_drv_elf_t* h, vos_drv_elf_info_t* i) {
  memset(i, 0, sizeof(*i));
  if (!h) return;
  i->entry = (uintptr_t)h->elf.entry;
  i->text_addr = (uintptr_t)h->elf.sec[ELF_SEC_TEXT].addr;
  i->text_size = (uint32_t)h->elf.sec[ELF_SEC_TEXT].size;
  for (int k = 0; k < 5 && k < ELF_SECS; k++) i->sec_size[k] = (uint32_t)h->elf.sec[k].size;
}
#endif
