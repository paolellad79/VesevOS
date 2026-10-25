// VesevOS Recovery - lettura "a pezzi" di un modulo multipart/form-data (il file .bin caricato dalla pagina).
// Parte pura (nessun hardware): provata sul computer da tools/hosttest/test_recmulti.cpp.
// Si danno i byte man mano che arrivano (feed). Del PRIMO campo con "filename=" si chiamano: fileStart(), fileData() per ogni pezzo,
// fileEnd() quando arriva il confine. Gli altri campi si ignorano. Nessun file intero in memoria: tiene solo gli ultimi byte del confine.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>

class RecMultipart {
 public:
  struct Sink {
    virtual void fileStart() = 0;
    virtual void fileData(const uint8_t* d, size_t n) = 0;
    virtual void fileEnd() = 0;
  };

  // boundary = il valore di "boundary=" nell'intestazione Content-Type. false = vuoto o troppo lungo.
  bool begin(const std::string& boundary, Sink* sink) {
    if (boundary.empty() || boundary.size() > 100) return false;
    delim_ = "\r\n--" + boundary;
    sink_ = sink;
    buf_ = "\r\n";                      // il primo confine non ha il "a capo" davanti: lo aggiungiamo noi
    state_ = PREAMBLE;
    fileSeen_ = false;
    return true;
  }

  // false = il modulo e rovinato (l'intestazione di un campo e troppo lunga, ecc.).
  bool feed(const uint8_t* d, size_t n) {
    if (state_ == DONE) return true;
    buf_.append((const char*)d, n);
    for (;;) {
      if (state_ == PREAMBLE) {
        size_t p = buf_.find(delim_);
        if (p == std::string::npos) { keepTail(delim_.size() - 1); return true; }
        buf_.erase(0, p + delim_.size());
        state_ = LINE;
      } else if (state_ == LINE) {          // resto della riga del confine: "--" = fine del modulo, altrimenti si va alle intestazioni
        size_t p = buf_.find("\r\n");
        if (p == std::string::npos) return buf_.size() < 256;
        bool last = buf_.compare(0, 2, "--") == 0;
        buf_.erase(0, p + 2);
        if (last) { state_ = DONE; buf_.clear(); return true; }
        state_ = HEADERS;
      } else if (state_ == HEADERS) {
        size_t p = buf_.find("\r\n\r\n");
        if (p == std::string::npos) return buf_.size() < 2048;
        bool isFile = buf_.find("filename=", 0) < p;
        buf_.erase(0, p + 4);
        inFile_ = isFile && !fileSeen_;
        if (inFile_) { fileSeen_ = true; sink_->fileStart(); }
        state_ = BODY;
      } else {                              // BODY
        size_t p = buf_.find(delim_);
        if (p != std::string::npos) {
          if (inFile_) { emit(p); sink_->fileEnd(); inFile_ = false; }
          buf_.erase(0, p + delim_.size());
          state_ = LINE;
          continue;
        }
        size_t keep = delim_.size() - 1;     // questi byte potrebbero essere l'inizio del confine
        if (buf_.size() > keep) {
          if (inFile_) emit(buf_.size() - keep);
          buf_.erase(0, buf_.size() - keep);
        }
        return true;
      }
    }
  }

  bool finished() const { return state_ == DONE || (fileSeen_ && !inFile_); }   // il file e arrivato intero

 private:
  enum State { PREAMBLE, LINE, HEADERS, BODY, DONE };
  void keepTail(size_t n) { if (buf_.size() > n) buf_.erase(0, buf_.size() - n); }
  void emit(size_t n) { if (n) sink_->fileData((const uint8_t*)buf_.data(), n); }

  std::string buf_, delim_;
  Sink* sink_ = nullptr;
  State state_ = DONE;
  bool fileSeen_ = false, inFile_ = false;
};
