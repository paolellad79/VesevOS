// VesevOS Recovery - controlli sul file del firmware (parte pura, provata sul computer da tools/hosttest/test_recheck.cpp).
// Il marchio (vedi VesevOS.ino, VOS_FW_MARK) e scritto nel firmware. Qui e salvato "mascherato" (XOR 0x5A) per non
// comparire in chiaro nel recovery stesso: cosi il recovery non puo essere caricato per sbaglio al posto del firmware.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define RC_HDR 48
#define RC_MARK_LEN 12
static const uint8_t RC_MARK_X[RC_MARK_LEN] = {0x21, 0x1C, 0x0D, 0x60, 0x0C, 0x3F, 0x29, 0x3F, 0x2C, 0x15, 0x09, 0x27};

struct RecCheck {
  uint8_t hdr[RC_HDR];
  uint8_t nh;        // byte dell'intestazione gia raccolti
  uint8_t mi;        // posizione raggiunta nel marchio
  bool mark;         // marchio trovato
  uint32_t size;     // byte ricevuti
};

inline void rcInit(RecCheck& c) { c.nh = 0; c.mi = 0; c.mark = false; c.size = 0; }

// Il marchio inizia con '{' che non compare altrove al suo interno: basta ripartire da 0 (o da 1 se il byte e '{').
inline void rcFeed(RecCheck& c, const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n; i++) {
    uint8_t b = d[i];
    if (c.nh < RC_HDR) c.hdr[c.nh++] = b;
    if (!c.mark) {
      if (b == (uint8_t)(RC_MARK_X[c.mi] ^ 0x5A)) {
        if (++c.mi == RC_MARK_LEN) c.mark = true;
      } else c.mi = (b == (uint8_t)(RC_MARK_X[0] ^ 0x5A)) ? 1 : 0;
    }
  }
  c.size += (uint32_t)n;
}

// Controllo dell'intestazione (dopo i primi 48 byte). NULL = va bene, altrimenti il motivo (senza apici doppi: finisce in un JSON).
inline const char* rcHeaderErr(const RecCheck& c) {
  if (c.nh < RC_HDR) return "File troppo piccolo: non e un firmware";
  if (c.hdr[0] != 0xE9) return "Non e un firmware (intestazione errata)";
  uint16_t chip = (uint16_t)(c.hdr[12] | (c.hdr[13] << 8));
  if (chip != 0x0009) return "Firmware per un altro tipo di chip (serve ESP32-S3)";
  uint32_t dm = (uint32_t)c.hdr[32] | ((uint32_t)c.hdr[33] << 8) | ((uint32_t)c.hdr[34] << 16) | ((uint32_t)c.hdr[35] << 24);
  if (dm != 0xABCD5432UL) return "Non e un firmware Arduino-ESP32 valido";
  return nullptr;
}

// Controllo finale (dopo l'ultimo byte).
inline const char* rcFinalErr(const RecCheck& c, uint32_t maxSize) {
  const char* e = rcHeaderErr(c);
  if (e) return e;
  if (c.size > maxSize) return "File troppo grande per lo spazio disponibile";
  if (!c.mark) return "Non e un firmware VesevOS (manca il marchio)";
  return nullptr;
}
