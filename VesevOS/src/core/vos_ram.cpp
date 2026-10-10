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
  {"monitor", 4608}, {"time", 4096}, {"wd", 6144}, {"selftest", 5120}, {"loopTask", 8192},
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


// ---- Storico delle annotazioni (ram audit) ----
// Anello fisso di 14 voci: nessun heap. Chi annota: avvio finito, Bluetooth acceso/spento, accesso e uscita dalla pagina.
struct RamNote { uint32_t sec; char label[10]; uint32_t free_, big, ps; };
static RamNote g_notes[14];
static uint8_t g_noteN = 0;                  // quante voci valide (max 14)
static uint8_t g_noteHead = 0;               // dove scrivere la prossima
static portMUX_TYPE g_noteMux = portMUX_INITIALIZER_UNLOCKED;

void ramNote(const char* label) {
  uint32_t f, b, p; heapNow(f, b, p);
  portENTER_CRITICAL(&g_noteMux);
  RamNote& n = g_notes[g_noteHead];
  n.sec = millis() / 1000UL; n.free_ = f; n.big = b; n.ps = p;
  strncpy(n.label, label ? label : "", sizeof(n.label) - 1); n.label[sizeof(n.label) - 1] = 0;
  g_noteHead = (g_noteHead + 1) % 14; if (g_noteN < 14) g_noteN++;
  portEXIT_CRITICAL(&g_noteMux);
}

String ramAudit() {
  ramNote("audit");                     // ogni audit lascia un segno: cosi la variazione tra due audit e quella vera
  multi_heap_info_t in;
  heap_caps_get_info(&in, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint32_t tot = in.total_free_bytes + in.total_allocated_bytes;
  uint32_t pct = tot ? (uint32_t)((uint64_t)in.total_allocated_bytes * 100ULL / tot) : 0;
  String r;
  r += trf("RAM interna: totale %u KB, occupata %u KB (%u%%), libera %u KB, pezzo piu grande %u KB",
           (unsigned)(tot / 1024), (unsigned)(in.total_allocated_bytes / 1024), (unsigned)pct, (unsigned)(in.total_free_bytes / 1024), (unsigned)(in.largest_free_block / 1024)) + "\n";
  int ts = ramTlsState();
  r += trf("TLS in PSRAM: %s", ts == 1 ? tr("attivo") : ts < 0 ? tr("non disponibile") : tr("non provato")) + "\n";
  r += String(tr("Storico: secondi, evento, RAM libera, pezzo piu grande, PSRAM, variazione della RAM libera")) + "\n";
  RamNote c[14]; uint8_t n, head;
  portENTER_CRITICAL(&g_noteMux);
  memcpy(c, g_notes, sizeof(c)); n = g_noteN; head = g_noteHead;
  portEXIT_CRITICAL(&g_noteMux);
  if (!n) r += String(tr("Nessun evento")) + "\n";
  char line[96]; int32_t prev = 0; bool havePrev = false;
  for (uint8_t i = 0; i < n; i++) {
    const RamNote& x = c[(head + 14 - n + i) % 14];
    if (havePrev) snprintf(line, sizeof(line), "%6lu  %-9s %4u KB  %4u KB  %5u KB  %+d KB", (unsigned long)x.sec, x.label, (unsigned)(x.free_ / 1024), (unsigned)(x.big / 1024), (unsigned)(x.ps / 1024), (int)(((int32_t)x.free_ - prev) / 1024));
    else snprintf(line, sizeof(line), "%6lu  %-9s %4u KB  %4u KB  %5u KB", (unsigned long)x.sec, x.label, (unsigned)(x.free_ / 1024), (unsigned)(x.big / 1024), (unsigned)(x.ps / 1024));
    prev = (int32_t)x.free_; havePrev = true;
    r += String(line) + "\n";
  }
  return r + ramReport();
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
