// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_deflate.cpp
// Deflate a flusso sopra la miniz della ROM dell'ESP32-S3 (vedi vos_drv_deflate.h). Nella ROM ci sono: tinfl_decompress, tdefl_init,
// tdefl_compress_buffer, tdefl_compress (esp32s3.rom.ld, gruppo miniz) e il CRC32. NON c'e tdefl_create_comp_flags_from_zip_params:
// il livello 1-9 diventa le opzioni di tdefl con la tabella qui sotto. Il file miniz.h si include solo qui, mai negli header.
#include "vos_drv_deflate.h"
#include "vos_drv_heap.h"
#include "vos_drv_task.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"
#include "freertos/semphr.h"
#include "miniz.h"
#include <string.h>

namespace {

// Passi di lavoro di tdefl in un colpo solo: piccoli, cosi tra uno e l'altro si cede la CPU e si controlla il tempo.
const size_t kCompressSlice = 4096;

// Livello gzip -> numero massimo di ricerche nel dizionario (piu alto = piu compressione e piu tempo). Il livello 0 non esiste: e "senza compressione".
// Valori nello stile di zlib; da 1 a 3 il parsing e "greedy" (piu veloce).
const uint16_t kProbes[10] = {0, 1, 6, 32, 16, 32, 128, 256, 512, 768};

int64_t nowMs() { return esp_timer_get_time() / 1000; }

}  // namespace

// ---------- una operazione alla volta ----------

namespace {
SemaphoreHandle_t busyMutex() {
  static StaticSemaphore_t buf;                                  // creato una sola volta (l'inizializzazione di una static locale e sicura tra task)
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&buf);
  return h;
}
}  // namespace

DrvDefLock::DrvDefLock() : ok_(xSemaphoreTake(busyMutex(), 0) == pdTRUE) {}

DrvDefLock::~DrvDefLock() {
  if (ok_) xSemaphoreGive(busyMutex());
}

// ---------- guardia: tempo, annullamento, CPU ----------

void DrvDefGuard::start(uint32_t maxMs, DrvDefCancelFn cancel, void* user) {
  startMs_ = (uint32_t)nowMs();
  maxMs_ = maxMs;
  pending_ = 0;
  cancel_ = cancel;
  user_ = user;
}

uint32_t DrvDefGuard::elapsedMs() const { return (uint32_t)nowMs() - startMs_; }

DrvDefErr DrvDefGuard::tick(size_t n) {
  pending_ += n;
  if (pending_ < DRV_DEF_YIELD_BYTES) return DDF_OK;
  pending_ = 0;
  drvTaskPause();                                                // un tick di pausa e il segnale al watchdog: i task di priorita bassa girano
  if (maxMs_ && elapsedMs() > maxMs_) return DDF_TIMEOUT;
  if (cancel_ && cancel_(user_)) return DDF_CANCEL;
  return DDF_OK;
}

// ---------- CRC32 e PSRAM ----------

// esp_rom_crc32_le mette da solo la "~" davanti e dietro (vedi esp_rom_crc.h): con crc = 0 al primo pezzo da' il CRC32 standard.
uint32_t drvCrc32(uint32_t crc, const uint8_t* data, size_t n) {
  return n ? esp_rom_crc32_le(crc, data, (uint32_t)n) : crc;
}

DrvDefErr drvDefCheckPsram(size_t need) {
  if (drvPsramTotal() == 0) return DDF_NO_PSRAM;
  if (drvPsramFree() < need + DRV_DEF_PSRAM_MARGIN) return DDF_NO_MEM;
  return DDF_OK;
}

// ---------- decompressione ----------

DrvDefErr DrvInflate::open() {
  close();
  DrvDefErr e = drvDefCheckPsram(sizeof(tinfl_decompressor) + DRV_DEF_WINDOW);
  if (e != DDF_OK) return e;
  state_ = drvPsramMalloc(sizeof(tinfl_decompressor));
  window_ = (uint8_t*)drvPsramMalloc(DRV_DEF_WINDOW);
  if (!state_ || !window_) { close(); return DDF_NO_MEM; }
  restart();
  return DDF_OK;
}

void DrvInflate::restart() {
  if (state_) tinfl_init((tinfl_decompressor*)state_);
  ofs_ = 0;
  crc_ = 0;
  out_ = 0;
}

void DrvInflate::close() {
  heap_caps_free(state_);
  heap_caps_free(window_);
  state_ = nullptr;
  window_ = nullptr;
}

DrvDefErr DrvInflate::push(const uint8_t* in, size_t len, size_t* used, DrvDefSink sink, void* user, DrvDefGuard* guard) {
  *used = 0;
  if (!state_ || !window_ || !sink) return DDF_BAD_ARG;
  tinfl_decompressor* r = (tinfl_decompressor*)state_;
  for (;;) {
    size_t inN = len - *used;
    size_t outN = DRV_DEF_WINDOW - ofs_;                         // la finestra e circolare: si scrive fino alla fine, poi si riparte da 0
    uint8_t* chunk = window_ + ofs_;
    // TINFL_FLAG_HAS_MORE_INPUT sempre acceso: e il chiamante a sapere se il file e finito (se manca input e il flusso non e chiuso e "troncato").
    tinfl_status st = tinfl_decompress(r, in + *used, &inN, window_, chunk, &outN, TINFL_FLAG_HAS_MORE_INPUT);
    *used += inN;
    if (outN) {
      crc_ = drvCrc32(crc_, chunk, outN);
      out_ += outN;
      ofs_ = (ofs_ + outN) & (DRV_DEF_WINDOW - 1);
      if (!sink(chunk, outN, user)) return DDF_SINK;
    }
    if (guard) {
      DrvDefErr g = guard->tick(inN + outN);
      if (g != DDF_OK) return g;
    }
    if (st == TINFL_STATUS_DONE) return DDF_DONE;
    if (st == TINFL_STATUS_NEEDS_MORE_INPUT) return DDF_OK;     // tutto l'input e stato consumato
    if (st != TINFL_STATUS_HAS_MORE_OUTPUT) return DDF_BAD_DATA; // negativi: flusso rotto
    if (inN == 0 && outN == 0) return DDF_BAD_DATA;              // nessun passo avanti: non si resta in questo giro per sempre
  }
}

// ---------- compressione ----------

DrvDefErr DrvDeflate::open(int level, DrvDefSink sink, void* user) {
  close();
  if (level < 1 || level > 9 || !sink) return DDF_BAD_ARG;
  DrvDefErr e = drvDefCheckPsram(sizeof(tdefl_compressor));
  if (e != DDF_OK) return e;
  state_ = drvPsramMalloc(sizeof(tdefl_compressor));
  if (!state_) return DDF_NO_MEM;
  sink_ = sink;
  sinkUser_ = user;
  sinkFailed_ = false;
  crc_ = 0;
  in_ = 0;
  out_ = 0;
  int flags = kProbes[level] | (level <= 3 ? TDEFL_GREEDY_PARSING_FLAG : 0);   // deflate grezzo: niente TDEFL_WRITE_ZLIB_HEADER
  if (tdefl_init((tdefl_compressor*)state_, &DrvDeflate::putCb, this, flags) != TDEFL_STATUS_OKAY) { close(); return DDF_BAD_ARG; }
  return DDF_OK;
}

void DrvDeflate::close() {
  heap_caps_free(state_);
  state_ = nullptr;
}

// miniz da' i byte compressi a blocchi (fino a circa 31 KB). false = errore: tdefl_compress_buffer ritorna TDEFL_STATUS_PUT_BUF_FAILED.
int DrvDeflate::putCb(const void* data, int len, void* self) {
  DrvDeflate* d = (DrvDeflate*)self;
  if (len <= 0) return 1;
  d->out_ += (size_t)len;
  if (!d->sink_((const uint8_t*)data, (size_t)len, d->sinkUser_)) { d->sinkFailed_ = true; return 0; }
  return 1;
}

DrvDefErr DrvDeflate::push(const uint8_t* in, size_t len, DrvDefGuard* guard) {
  if (!state_) return DDF_BAD_ARG;
  while (len) {
    size_t n = len < kCompressSlice ? len : kCompressSlice;
    crc_ = drvCrc32(crc_, in, n);
    in_ += n;
    tdefl_status st = tdefl_compress_buffer((tdefl_compressor*)state_, in, n, TDEFL_NO_FLUSH);
    if (st < 0) return sinkFailed_ ? DDF_SINK : DDF_BAD_ARG;
    in += n;
    len -= n;
    if (guard) {
      DrvDefErr g = guard->tick(n);
      if (g != DDF_OK) return g;
    }
  }
  return DDF_OK;
}

DrvDefErr DrvDeflate::finish(DrvDefGuard* guard) {
  if (!state_) return DDF_BAD_ARG;
  tdefl_status st = tdefl_compress_buffer((tdefl_compressor*)state_, nullptr, 0, TDEFL_FINISH);
  if (st < 0) return sinkFailed_ ? DDF_SINK : DDF_BAD_ARG;
  if (guard) return guard->tick(DRV_DEF_YIELD_BYTES);            // una pausa e un controllo finali (la chiusura puo essere lunga)
  return DDF_OK;
}
