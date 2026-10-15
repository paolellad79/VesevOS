// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_imgcheck.h
// Controlli puri su un'immagine ESP32-S3 gia tutta in memoria (aggiornamento del recovery dalla pagina).
// Niente hardware: provati sul computer da tools/hosttest/test_imgcheck.cpp. L'impronta SHA-256 finale la controlla chi chiama.
// Il marchio del recovery (testo fisso + versione + graffa chiusa, vedi REC_MARK in recovery/VesevOS_Recovery) e salvato "mascherato" (XOR 0x5A):
// cosi non compare in chiaro nel firmware e il firmware non puo essere caricato al posto del recovery.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define IMG_HDR 48
#define IMG_RMARK_LEN 13
static const uint8_t IMG_RMARK_X[IMG_RMARK_LEN] = {0x21, 0x08, 0x1F, 0x19, 0x60, 0x0C, 0x3F, 0x29, 0x3F, 0x2C, 0x15, 0x09, 0x60};

static inline uint32_t imgLe32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

// Struttura dell'immagine: intestazione ESP32-S3 / Arduino, segmenti, somma di controllo. IMG_OK = va bene, altrimenti il motivo
// (il testo tradotto lo sceglie chi chiama). hashOff = dove inizia l'impronta SHA-256 aggiunta (0 = nessuna impronta).
enum { IMG_OK = 0, IMG_SMALL, IMG_MAGIC, IMG_CHIP, IMG_ARDUINO, IMG_BIG, IMG_SEGS, IMG_SHORT, IMG_SUM, IMG_TAIL };
static inline int imgCheck(const uint8_t* d, uint32_t n, uint32_t maxSize, uint32_t& hashOff) {
  hashOff = 0;
  if (n < IMG_HDR) return IMG_SMALL;
  if (d[0] != 0xE9) return IMG_MAGIC;
  if ((uint16_t)(d[12] | (d[13] << 8)) != 0x0009) return IMG_CHIP;
  if (imgLe32(d + 32) != 0xABCD5432UL) return IMG_ARDUINO;
  if (n > maxSize) return IMG_BIG;
  uint8_t segs = d[1];
  if (segs == 0 || segs > 16) return IMG_SEGS;
  uint32_t off = 24; uint8_t sum = 0xEF;
  for (uint8_t s = 0; s < segs; s++) {
    if (off + 8 > n) return IMG_SHORT;
    uint32_t len = imgLe32(d + off + 4); off += 8;
    if (len > n - off) return IMG_SHORT;
    for (uint32_t i = 0; i < len; i++) sum ^= d[off + i];
    off += len;
  }
  uint32_t cs = off + (15 - (off % 16));          // la somma sta nell'ultimo byte del blocco da 16
  if (cs >= n) return IMG_SHORT;
  if (d[cs] != sum) return IMG_SUM;
  uint32_t end = cs + 1;
  if (d[23] == 1) { if (end + 32 != n) return IMG_TAIL; hashOff = end; }
  else if (end != n) return IMG_TAIL;
  return IMG_OK;
}

// Cerca il marchio del recovery e ne copia la versione (fino a '}', solo caratteri stampabili). true = trovato.
// Il marchio inizia con '{' che non compare altrove al suo interno: basta ripartire da 0 (o da 1 se il byte e '{').
static inline bool imgRecVersion(const uint8_t* d, uint32_t n, char* ver, size_t vn) {
  if (vn) ver[0] = 0;
  uint8_t mi = 0;
  for (uint32_t i = 0; i < n; i++) {
    uint8_t b = d[i];
    if (b == (uint8_t)(IMG_RMARK_X[mi] ^ 0x5A)) {
      if (++mi < IMG_RMARK_LEN) continue;
      size_t k = 0;
      for (uint32_t j = i + 1; j < n && d[j] != '}' && k + 1 < vn; j++) {
        if (d[j] < 0x21 || d[j] > 0x7E) { k = 0; break; }
        ver[k++] = (char)d[j];
      }
      if (vn) ver[k] = 0;
      if (k) return true;
      mi = 0;
    } else mi = (b == (uint8_t)(IMG_RMARK_X[0] ^ 0x5A)) ? 1 : 0;
  }
  return false;
}

// Come sopra ma a pezzi (marchio a cavallo di due pezzi compreso): per leggere la versione dalla partizione senza tenerla tutta in RAM.
struct ImgScan { uint8_t mi; bool in; char ver[16]; uint8_t k; bool done; };
static inline void imgScanInit(ImgScan& s) { memset(&s, 0, sizeof(s)); }
static inline void imgScanFeed(ImgScan& s, const uint8_t* d, size_t n) {
  for (size_t i = 0; i < n && !s.done; i++) {
    uint8_t b = d[i];
    if (s.in) {
      if (b == '}' && s.k) { s.ver[s.k] = 0; s.done = true; }
      else if (b < 0x21 || b > 0x7E || s.k + 1 >= sizeof(s.ver)) { s.in = false; s.k = 0; s.mi = 0; }
      else s.ver[s.k++] = (char)b;
      continue;
    }
    if (b == (uint8_t)(IMG_RMARK_X[s.mi] ^ 0x5A)) { if (++s.mi == IMG_RMARK_LEN) { s.in = true; s.k = 0; } }
    else s.mi = (b == (uint8_t)(IMG_RMARK_X[0] ^ 0x5A)) ? 1 : 0;
  }
}
