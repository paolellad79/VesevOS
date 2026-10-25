// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_mem.cpp: memoria (tetto, PSRAM, blocchi vivi) e uscita (print) di Berry (vedi vos_berry.cpp).
#include "vos_berry.h"
#include "../devices/vos_io.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <time.h>
#include <ctype.h>
#include "esp_heap_caps.h"
#include "vos_rules.h"
#include "vos_app_rt.h"
#include "../core/vos_log.h"
#include "../core/vos_time.h"
#include "../core/vos_sys.h"
#include "../core/vos_i18n.h"
#include "../core/vos_files.h"
#include "../core/vos_util.h"
#include "../devices/vos_led.h"
#include "vos_btn.h"
#include "vos_mqtt.h"
#include "vos_berry_int.h"

// ---------- memoria di Berry: tetto + PSRAM + elenco dei blocchi vivi ----------
// Ogni blocco ha un'intestazione (dimensione + elenco doppio). Finita l'esecuzione berryFreeAll() libera TUTTO cio che e rimasto
// (Berry dopo un errore di memoria puo lasciare piccoli blocchi): niente perdite nella PSRAM.
struct BHdr { uint32_t size; uint32_t magic; BHdr* prev; BHdr* next; };
#define BHDR_SZ  ((sizeof(BHdr) + 7) & ~(size_t)7)
#define BMAGIC   0xB3E77CA5UL
static BHdr* g_live = NULL;

extern "C" void* vos_berry_malloc(size_t n) {
  if (n == 0) n = 1;
  if (g_used + n > BERRY_MEM_LIMIT) return NULL;
  uint8_t* p = (uint8_t*)heap_caps_malloc(n + BHDR_SZ, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = (uint8_t*)heap_caps_malloc(n + BHDR_SZ, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   // scheda senza PSRAM
  if (!p) return NULL;
  BHdr* h = (BHdr*)p;
  h->size = (uint32_t)n; h->magic = BMAGIC; h->prev = NULL; h->next = g_live;
  if (g_live) g_live->prev = h;
  g_live = h;
  g_used += n; if (g_used > g_peak) g_peak = g_used;
  return p + BHDR_SZ;
}
extern "C" void vos_berry_free(void* q) {
  if (!q) return;
  BHdr* h = (BHdr*)((uint8_t*)q - BHDR_SZ);
  if (h->magic != BMAGIC) return;                          // non e un nostro blocco (o gia liberato): non toccare
  if (h->prev) h->prev->next = h->next; else g_live = h->next;
  if (h->next) h->next->prev = h->prev;
  g_used = g_used >= h->size ? g_used - h->size : 0;
  h->magic = 0;
  heap_caps_free(h);
}
extern "C" void* vos_berry_realloc(void* q, size_t n) {
  if (!q) return vos_berry_malloc(n);
  if (n == 0) { vos_berry_free(q); return NULL; }
  size_t old = ((BHdr*)((uint8_t*)q - BHDR_SZ))->size;
  if (n <= old) return q;                                  // piu piccolo: il blocco resta com'e
  if (g_used - old + n > BERRY_MEM_LIMIT) return NULL;
  void* r = vos_berry_malloc(n);
  if (!r) return NULL;
  memcpy(r, q, old);
  vos_berry_free(q);
  return r;
}
// libera tutto cio che e rimasto; ritorna i byte che Berry non aveva liberato
size_t vosberry::berryFreeAll() {
  size_t left = 0;
  while (g_live) { BHdr* h = g_live; left += h->size; g_live = h->next; h->magic = 0; heap_caps_free(h); }
  g_used = 0;
  return left;
}

// ---------- uscita (print) ----------
// Il lettore (pagina web, shell) copia i buffer sotto g_mux: qui si scrive sotto lo stesso blocco (solo memcpy/memmove, nessuna chiamata
// che possa saltare con longjmp).
extern "C" void vos_berry_write(const char* b, size_t n) {
  portENTER_CRITICAL(&g_mux);
  if (g_toErr) {
    size_t room = sizeof(g_ebuf) - 1 - g_elen; if (n > room) n = room;
    memcpy(g_ebuf + g_elen, b, n); g_elen += n; g_ebuf[g_elen] = 0;
  } else {
    size_t room = BERRY_OUT_MAX - g_olen;
    if (g_bg) {                                            // script in /scripts: si tiene la parte piu recente
      if (n > BERRY_OUT_MAX) { b += n - BERRY_OUT_MAX; n = BERRY_OUT_MAX; g_otrunc = true; }
      if (n > room) { size_t drop = n - room; memmove(g_obuf, g_obuf + drop, g_olen - drop); g_olen -= drop; g_otrunc = true; }
    } else if (n > room) { n = room; g_otrunc = true; }
    memcpy(g_obuf + g_olen, b, n); g_olen += n; g_obuf[g_olen] = 0;
  }
  portEXIT_CRITICAL(&g_mux);
}
#endif
