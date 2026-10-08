// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ram.cpp
#include "vos_ram.h"
#include "vos_util.h"
#include "vos_i18n.h"
#include <esp_heap_caps.h>

// Dimensione dello stack con cui nascono i task di VesevOS (tenere uguale a xTaskCreate... nel modulo di ogni task).
struct StackSize { const char* name; uint16_t bytes; };
static const StackSize SZ[] = {
  {"led", 3072}, {"mesh", 6144}, {"mqtt", 6144}, {"net", 6144}, {"rules", 6144},
  {"monitor", 4608}, {"time", 4096}, {"wd", 3072}, {"selftest", 5120}, {"loopTask", 8192},
};

static uint16_t stackOf(const char* n) {
  for (const StackSize& s : SZ) if (strcmp(s.name, n) == 0) return s.bytes;
  return 0;
}

static void heapNow(uint32_t& free_, uint32_t& big, uint32_t& psFree) {
  multi_heap_info_t in, ps;
  heap_caps_get_info(&in, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heap_caps_get_info(&ps, MALLOC_CAP_SPIRAM);
  free_ = in.total_free_bytes; big = in.largest_free_block; psFree = ps.total_free_bytes;
}

String ramReport() {
  multi_heap_info_t in, ps;
  heap_caps_get_info(&in, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heap_caps_get_info(&ps, MALLOC_CAP_SPIRAM);
  String r;
  r += trf("RAM interna: libera %u KB, pezzo piu grande %u KB, minima %u KB", (unsigned)(in.total_free_bytes / 1024), (unsigned)(in.largest_free_block / 1024), (unsigned)(in.minimum_free_bytes / 1024)) + "\n";
  r += trf("PSRAM: libera %u KB, pezzo piu grande %u KB", (unsigned)(ps.total_free_bytes / 1024), (unsigned)(ps.largest_free_block / 1024)) + "\n";
#if (configUSE_TRACE_FACILITY == 1)
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t* a = (TaskStatus_t*)malloc((n + 2) * sizeof(TaskStatus_t));
  if (!a) { r += String(tr("Memoria insufficiente")) + "\n"; return r; }
  uint32_t tot = 0;
  n = uxTaskGetSystemState(a, n + 2, &tot);
  r += String(tr("Task          Stack libero minimo / totale (byte)  Si puo togliere")) + "\n";
  uint32_t sumTot = 0, sumSpare = 0; char line[96];
  for (UBaseType_t i = 0; i < n; i++) {
    const char* nm = a[i].pcTaskName;
    uint32_t freeMin = a[i].usStackHighWaterMark, total = stackOf(nm), spare = utilStackSpare(total, freeMin);
    if (total) { snprintf(line, sizeof(line), "%-12s %5u / %-5u  %5u", nm, (unsigned)freeMin, (unsigned)total, (unsigned)spare); sumTot += total; sumSpare += spare; }
    else snprintf(line, sizeof(line), "%-12s %5u / -", nm, (unsigned)freeMin);
    r += String(line) + "\n";
  }
  free(a);
  r += trf("Stack dei task VesevOS: %u byte in tutto, margine recuperabile (tenendo 25%% libero): %u byte", (unsigned)sumTot, (unsigned)sumSpare) + "\n";
#else
  r += String(tr("(dettagli non disponibili in questa build)")) + "\n";
#endif
  return r;
}

static struct { bool set; uint32_t free_, big, ps, ms; } g_mark;

void ramMark() { heapNow(g_mark.free_, g_mark.big, g_mark.ps); g_mark.ms = millis(); g_mark.set = true; }

String ramDiff() {
  if (!g_mark.set) return "";
  uint32_t f, b, p; heapNow(f, b, p);
  return trf("Dopo %lu s: RAM interna %+.1f KB, pezzo piu grande %+.1f KB, PSRAM %+.1f KB",
             (unsigned long)((millis() - g_mark.ms) / 1000UL),
             ((double)f - (double)g_mark.free_) / 1024.0, ((double)b - (double)g_mark.big) / 1024.0, ((double)p - (double)g_mark.ps) / 1024.0) + "\n";
}

// ---- TLS in PSRAM ----
// La libreria TLS (mbedTLS) chiede di suo la RAM interna: i due buffer da 16 KB per ogni collegamento HTTPS valgono ~34 dei ~40 KB.
// mbedtls_platform_set_calloc_free() le fa usare un nostro allocatore: blocchi >= 512 byte in PSRAM, quelli piccoli (chiavi, valori segreti) restano in RAM interna.
// Se la PSRAM e piena si ripiega sulla RAM interna. La funzione e "weak": se questo core non la offre, non c'e errore di collegamento, si resta come prima.
extern "C" int mbedtls_platform_set_calloc_free(void* (*calloc_func)(size_t, size_t), void (*free_func)(void*)) __attribute__((weak));
#define TLS_PSRAM_MIN 512
static int g_tlsState = 0;
static void* tlsCalloc(size_t n, size_t sz) {
  if (n && sz > (size_t)-1 / n) return nullptr;
  size_t t = n * sz;
  void* p = nullptr;
  if (t >= TLS_PSRAM_MIN) p = heap_caps_calloc(n, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_calloc(n, sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return p;
}
static void tlsFree(void* p) { heap_caps_free(p); }          // accetta puntatori di qualsiasi memoria
bool ramTlsToPsram() {
  if (!psramFound()) { g_tlsState = -1; return false; }
  if (!mbedtls_platform_set_calloc_free) { g_tlsState = -1; return false; }
  bool ok = mbedtls_platform_set_calloc_free(tlsCalloc, tlsFree) == 0;
  g_tlsState = ok ? 1 : -1;
  return ok;
}
int ramTlsState() { return g_tlsState; }
