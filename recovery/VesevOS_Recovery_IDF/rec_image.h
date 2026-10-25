// VesevOS Recovery - scrittura di un'immagine firmware in app0 con tutti i controlli (usata dal caricamento dalla pagina e dallo scaricamento).
// Controlli: intestazione ESP32-S3 (PRIMA di cancellare), marchio VesevOS, dimensione (anche quella attesa), SHA-256 (se dato), esp_ota_end.
// Se l'errore arriva dopo aver cominciato a scrivere, il primo settore di app0 viene cancellato: niente immagini a meta.
#pragma once
#include "rec_multipart.h"
#include "recovery_check.h"
#include "rec_order.h"
#include <string>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

class RecImage : public RecMultipart::Sink {
 public:
  // wantSha = SHA-256 atteso in esadecimale minuscolo ("" = non dato); wantSize 0 = non data.
  RecImage(const std::string& wantSha, uint32_t wantSize) : want_(wantSha), wantSize_(wantSize) {}
  ~RecImage() { shaStop(); }

  void fileStart() override;
  void fileData(const uint8_t* d, size_t n) override;
  void fileEnd() override;

  bool failed() const { return failed_; }
  bool done() const { return started_ && !failed_; }     // il file e arrivato intero e ha superato tutto
  void fail(int code, const std::string& why);
  const std::string& err() const { return err_; }
  int code() const { return code_; }                      // RES_* dell'errore
  const std::string& sha() const { return sha_; }

 private:
  bool beginWrite();
  void shaStop() { if (shaOn_) { mbedtls_sha256_free(&sha256_); shaOn_ = false; } }

  bool failed_ = false, started_ = false, begun_ = false, shaOn_ = false;
  int code_ = RES_WRITE;
  std::string err_ = "Caricamento non riuscito", sha_, want_, pending_;
  uint32_t wantSize_;
  RecCheck chk_;
  mbedtls_sha256_context sha256_;
  esp_ota_handle_t oh_ = 0;
};
