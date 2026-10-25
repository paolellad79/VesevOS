// VesevOS Recovery - scrittura dell'immagine in app0 (vedi rec_image.h). Messaggi uguali alla 0.4.4.
#include "rec_image.h"
#include "rec_boot.h"
#include "rec_led.h"
#include "rec_util.h"
#include <stdio.h>

void RecImage::fail(int code, const std::string& why) {
  failed_ = true; code_ = code; err_ = why;
  recLedSet(LS_ERR);
  if (begun_) {
    esp_ota_abort(oh_);
    const esp_partition_t* p = recApp0();
    if (p) esp_partition_erase_range(p, 0, 4096);
    begun_ = false;
  }
  shaStop();
}

void RecImage::fileStart() {
  started_ = true;
  app0FwForget();
  recLedSet(LS_WRITE);
  rcInit(chk_);
  mbedtls_sha256_init(&sha256_); mbedtls_sha256_starts(&sha256_, 0); shaOn_ = true;
}

// Primi 48 byte raccolti: si controlla l'intestazione PRIMA di cancellare qualcosa, poi si apre la scrittura.
bool RecImage::beginWrite() {
  const char* e = rcHeaderErr(chk_);
  if (e) { fail(RES_SIZE, e); printf("[recovery] file rifiutato: %s\n", e); return false; }
  const esp_partition_t* p = recApp0();
  if (!p) { fail(RES_WRITE, "Partizione del firmware non trovata"); return false; }
  if (esp_ota_begin(p, OTA_SIZE_UNKNOWN, &oh_) != ESP_OK) { fail(RES_WRITE, "Non posso scrivere nella partizione"); return false; }
  begun_ = true;
  return true;
}

void RecImage::fileData(const uint8_t* d, size_t n) {
  if (failed_) return;
  rcFeed(chk_, d, n);
  mbedtls_sha256_update(&sha256_, d, n);
  if (!begun_) {
    pending_.append((const char*)d, n);
    if (chk_.nh < RC_HDR) return;                       // intestazione ancora incompleta: aspetto altri byte
    if (!beginWrite()) return;
    d = (const uint8_t*)pending_.data(); n = pending_.size();
  }
  if (esp_ota_write(oh_, d, n) != ESP_OK) fail(RES_WRITE, "Scrittura fallita (file troppo grande?)");
  pending_.clear();
}

void RecImage::fileEnd() {
  if (failed_) return;
  uint8_t dg[32];
  mbedtls_sha256_finish(&sha256_, dg);
  sha_ = hexOf(dg, 32);
  const esp_partition_t* p = recApp0();
  const char* e = rcFinalErr(chk_, p ? p->size : 0);
  if (e) { fail(RES_SIZE, e); printf("[recovery] file rifiutato: %s\n", e); return; }
  if (wantSize_ && chk_.size != wantSize_) { fail(RES_SIZE, "Dimensione diversa da quella attesa"); return; }
  if (want_.size() && want_ != sha_) {
    fail(RES_SHA, "SHA-256 diverso da quello atteso: file danneggiato o sbagliato");
    printf("[recovery] SHA-256 ricevuto %s\n", sha_.c_str());
    return;
  }
  if (!begun_) { fail(RES_WRITE, "Caricamento non riuscito"); return; }
  if (esp_ota_end(oh_) != ESP_OK) {
    begun_ = false;
    if (p) esp_partition_erase_range(p, 0, 4096);
    fail(RES_WRITE, "Immagine incompleta o non valida");
    return;
  }
  begun_ = false;
  if (esp_ota_set_boot_partition(p) != ESP_OK) { fail(RES_WRITE, "Non riesco a impostare l'avvio"); return; }
  shaStop();
  app0FwSet(true);                                      // il marchio c'era (rcFinalErr)
  printf("[recovery] controlli superati, SHA-256 %s\n", sha_.c_str());
}
