// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_deflate.h
// STRATO 1 (driver): compressione e decompressione deflate a flusso, sopra la libreria miniz che sta nella ROM dell'ESP32-S3
// (esp_rom: tinfl_decompress, tdefl_init, tdefl_compress_buffer; costo di flash quasi zero). Il CRC32 e quello della ROM (esp_rom_crc32_le).
// Qui si usa solo la parte "a basso livello" di miniz: nessuna allocazione nascosta, la memoria la decidiamo noi.
//
// MEMORIA (tutta nella PSRAM, presa all'apertura e rilasciata alla chiusura o al distruttore, anche dopo un errore):
//   decompressione: finestra da 32 KB + stato di tinfl (circa 11 KB)
//   compressione  : lo stato tdefl_compressor, circa 168 KB (dizionario 32 KB, tabelle 73 KB, buffer dei codici 24 KB, buffer di uscita 31 KB)
// Senza PSRAM (drvPsramTotal() == 0) si rifiuta con DDF_NO_PSRAM: non si ripiega sulla RAM interna, che e scarsa.
//
// CHIAMANTE: una sola operazione alla volta (DrvDefLock), e un DrvDefGuard che cede la CPU e controlla tempo e annullamento.
// I dati entrano a pezzi (push) e escono a pezzi (sink): nessun file intero in memoria.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum DrvDefErr : uint8_t {
  DDF_OK = 0,        // tutto bene (decompressione: serve altro input)
  DDF_DONE,          // il flusso deflate e finito (decompressione)
  DDF_NO_PSRAM,      // la scheda non ha la PSRAM
  DDF_NO_MEM,        // la PSRAM libera non basta
  DDF_BAD_DATA,      // flusso compresso non valido
  DDF_SINK,          // chi riceve i dati ha detto basta (spazio finito, tetto superato...)
  DDF_CANCEL,        // fermato da chi chiama (Ctrl+C)
  DDF_TIMEOUT,       // tetto di tempo superato
  DDF_BAD_ARG,       // uso sbagliato (non aperto, livello fuori misura)
};

const size_t   DRV_DEF_WINDOW = 32768;          // finestra di decompressione (la massima del formato deflate)
const size_t   DRV_DEF_YIELD_BYTES = 4096;      // ogni quanti byte trattati si cede la CPU
const uint32_t DRV_DEF_PSRAM_MARGIN = 262144;   // PSRAM da lasciare libera oltre a quella che serve (la usano anche altri servizi)

// Chi riceve i dati: true = continua, false = fermati (l'operazione finisce con DDF_SINK)
typedef bool (*DrvDefSink)(const uint8_t* data, size_t n, void* user);
// Chiamata ogni tanto da DrvDefGuard: true = ferma l'operazione
typedef bool (*DrvDefCancelFn)(void* user);

// Una sola compressione o decompressione alla volta. Si crea sullo stack: ok() dice se il turno e tuo; il turno finisce col distruttore.
class DrvDefLock {
 public:
  DrvDefLock();
  ~DrvDefLock();
  DrvDefLock(const DrvDefLock&) = delete;
  DrvDefLock& operator=(const DrvDefLock&) = delete;
  bool ok() const { return ok_; }
 private:
  bool ok_;
};

// Tetto di tempo e annullamento. tick(n) si chiama per ogni pezzo trattato (n byte): ogni DRV_DEF_YIELD_BYTES cede la CPU
// (drvTaskPause: il watchdog dei task non scatta) e controlla scadenza e annullamento.
class DrvDefGuard {
 public:
  void start(uint32_t maxMs, DrvDefCancelFn cancel, void* user);
  DrvDefErr tick(size_t n);                 // DDF_OK, DDF_TIMEOUT o DDF_CANCEL
  uint32_t elapsedMs() const;
 private:
  uint32_t startMs_ = 0;
  uint32_t maxMs_ = 0;
  size_t   pending_ = 0;
  DrvDefCancelFn cancel_ = nullptr;
  void*    user_ = nullptr;
};

// CRC32 standard (quello di gzip, zip e zlib). Il primo pezzo parte da crc = 0; i successivi continuano dal valore ritornato.
uint32_t drvCrc32(uint32_t crc, const uint8_t* data, size_t n);

// PSRAM: c'e, e ce n'e abbastanza per need byte piu il margine? DDF_OK, DDF_NO_PSRAM o DDF_NO_MEM.
DrvDefErr drvDefCheckPsram(size_t need);

// ---- decompressione (flusso deflate "grezzo", senza intestazione zlib: quello di gzip e di zip) ----
class DrvInflate {
 public:
  DrvInflate() = default;
  ~DrvInflate() { close(); }
  DrvInflate(const DrvInflate&) = delete;
  DrvInflate& operator=(const DrvInflate&) = delete;

  DrvDefErr open();                          // prende la PSRAM; DDF_NO_PSRAM / DDF_NO_MEM se non si puo
  void close();                              // rilascia la PSRAM (si puo chiamare sempre)
  void restart();                            // riparte con un flusso nuovo senza liberare e riprendere la PSRAM (per piu voci di un archivio)
  // Da' in pasto len byte compressi. I dati decompressi escono dal sink, a blocchi fino a 32 KB. guard puo essere nullptr.
  // Ritorna DDF_OK (tutto consumato, serve altro input), DDF_DONE (il flusso e finito: *used dice quanti byte erano suoi),
  // oppure un errore. Dopo DDF_DONE o un errore non si chiama piu push.
  DrvDefErr push(const uint8_t* in, size_t len, size_t* used, DrvDefSink sink, void* user, DrvDefGuard* guard);
  uint32_t crc() const { return crc_; }      // CRC32 dei byte decompressi finora
  size_t   outBytes() const { return out_; }
 private:
  void*    state_ = nullptr;                 // tinfl_decompressor (nella PSRAM)
  uint8_t* window_ = nullptr;                // finestra da 32 KB (nella PSRAM)
  size_t   ofs_ = 0;
  uint32_t crc_ = 0;
  size_t   out_ = 0;
};

// ---- compressione (flusso deflate grezzo) ----
class DrvDeflate {
 public:
  DrvDeflate() = default;
  ~DrvDeflate() { close(); }
  DrvDeflate(const DrvDeflate&) = delete;
  DrvDeflate& operator=(const DrvDeflate&) = delete;

  // level da 1 (veloce) a 9 (piu compressa); come in gzip il 6 e quello di norma. I byte compressi escono dal sink.
  DrvDefErr open(int level, DrvDefSink sink, void* user);
  void close();
  DrvDefErr push(const uint8_t* in, size_t len, DrvDefGuard* guard);   // dati da comprimere (qualsiasi lunghezza)
  DrvDefErr finish(DrvDefGuard* guard);                                // chiude il flusso: dopo non si chiama piu push
  uint32_t crc() const { return crc_; }      // CRC32 dei byte entrati
  size_t   inBytes() const { return in_; }
  size_t   outBytes() const { return out_; } // byte compressi usciti dal sink
 private:
  static int putCb(const void* data, int len, void* self);   // chiamata di miniz (mz_bool = int)
  void*    state_ = nullptr;                 // tdefl_compressor (nella PSRAM)
  DrvDefSink sink_ = nullptr;
  void*    sinkUser_ = nullptr;
  bool     sinkFailed_ = false;
  uint32_t crc_ = 0;
  size_t   in_ = 0;
  size_t   out_ = 0;
};
