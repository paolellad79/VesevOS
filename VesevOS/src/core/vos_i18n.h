// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.h
// Traduzioni. Nel codice si scrive tr("testo italiano") e il testo italiano fa da chiave.
// Solo l'italiano e DENTRO il firmware (e la lingua del codice). Le altre lingue, inglese compreso, sono file /lang/<codice>.json (dalla 1.7.59).
// Le altre lingue sono file nella memoria interna: /lang/<codice>.json, un oggetto JSON piatto
// {"testo italiano": "traduzione", ...}. Chiavi speciali: "_name" nome della lingua, "_locale" (es. "es-ES"),
// "_flag" bandiera in SVG (piccola, disegnata a mano). Lo stesso file serve anche alla pagina web.
#pragma once
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <string_view>
#include "vos_errbuf.h"
#include "vos_heapbuf.h"

void        langInit();                          // dopo LittleFS e cfgLoad: carica la lingua scelta
bool        langSet(const char* code, ErrBuf& err);   // cambia lingua, salva, (ri)carica il file
bool        langCodeValid(std::string_view c);     // 2-8 lettere minuscole o numeri
#define LANG_JSON_MAX 8192   // tetto del JSON delle lingue (una bandiera pesa fino a 1 KB)
bool        langListJson(HeapBuf& out);          // aggiunge in coda [{"code":"it","name":"Italiano","loc":"it-IT","flag":"<svg..>"},...]
#define LANG_LIST_MAX 12   // al massimo 12 lingue nell'elenco pubblico (it + 11 file)
#define LANG_CODE_BUF 9    // codice lingua (8 caratteri + zero)
#define LANG_NAME_BUF 32   // nome della lingua, tagliato se piu lungo
int         langList(char codes[][LANG_CODE_BUF], char names[][LANG_NAME_BUF], int max);   // lingue disponibili (per il menu della seriale)
const char* tr(const char* it);                  // testo nella lingua corrente

// Testo formattato da trf(): fino a 95 caratteri sta dentro l'oggetto (stack), oltre in un blocco di heap che si libera da solo.
// Si usa come una String per leggere: c_str(), length(), e passa dove serve un const char*. Vive fino alla fine dell'istruzione:
// per tenerlo si copia in un buffer. Non si copia (si crea solo dal valore restituito da trf).
class TrText {
 public:
  TrText() { small_[0] = 0; }
  ~TrText() { free(big_); }
  TrText(TrText&& o) : big_(o.big_) { memcpy(small_, o.small_, sizeof small_); o.big_ = nullptr; }
  TrText(const TrText&) = delete;
  TrText& operator=(const TrText&) = delete;
  const char* c_str() const { return big_ ? big_ : small_; }
  operator const char*() const { return c_str(); }
  size_t length() const { return strlen(c_str()); }
 private:
  friend TrText trf(const char* it, ...);
  char  small_[96];
  char* big_ = nullptr;
};
TrText      trf(const char* it, ...) __attribute__((format(printf, 1, 2)));   // come printf, con testo tradotto
