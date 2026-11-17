// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.h
// Traduzioni. Nel codice si scrive tr("testo italiano") e il testo italiano fa da chiave.
// Solo l'italiano e DENTRO il firmware (e la lingua del codice). Le altre lingue, inglese compreso, sono file nella memoria interna.
// Ogni lingua ha DUE file, nessuno compresso sulla scheda e nessuno caricato in RAM:
//   /lang/<codice>.vld  dizionario del firmware (frasi cercate a salti sul file, formato in vos_i18n_calc.h). Una lingua e
//                       "installata" se esiste questo file.
//   /lang/<codice>.vlp  frasi della pagina web (JSON piatto, contenuto gzip: lo decomprime il browser). Facoltativo:
//                       senza, la pagina resta in italiano.
// Il nome, il codice locale e la bandiera stanno nella testata del .vld. Li scrive tools/mklang.py.
#pragma once
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <string_view>
#include "vos_errbuf.h"
#include "vos_heapbuf.h"

void        langInit();                          // dopo LittleFS e cfgLoad: mette in uso la lingua scelta (controlla il .vld, non lo carica)
bool        langSet(const char* code, ErrBuf& err);   // cambia lingua, salva, controlla il file
bool        langCodeValid(std::string_view c);     // 2-8 lettere minuscole o numeri
#define LANG_VLD_MAX 196608  // tetto del .vld (oggi circa 105-120 KB); il file non si carica in RAM, il tetto protegge dai file sbagliati
#define LANG_VLP_MAX 131072  // tetto del .vlp gzip (oggi circa 54-60 KB)
#define LANG_JSON_MAX 8192   // tetto del JSON delle lingue (una bandiera pesa fino a 1 KB)
bool        langListJson(HeapBuf& out);          // aggiunge in coda [{"code":"it","name":"Italiano","loc":"it-IT","flag":"<svg..>"},...]
#define LANG_LIST_MAX 12   // al massimo 12 lingue nell'elenco pubblico (it + 11 file)
#define LANG_CODE_BUF 9    // codice lingua (8 caratteri + zero)
#define LANG_NAME_BUF 32   // nome della lingua, tagliato se piu lungo
int         langList(char codes[][LANG_CODE_BUF], char names[][LANG_NAME_BUF], int max);   // lingue disponibili (per il menu della seriale)
const char* tr(const char* it);                  // testo nella lingua corrente

// Lingua in errore: il file in uso non e piu quello caricato (sostituito di nascosto, rovinato, cancellato, errori di lettura
// ripetuti). Da quel momento tr() usa l'italiano (nessun testo sbagliato, nessun accesso al file) finche langSet() non riesce
// (anche per "it") o un file valido sostituisce quello in uso. "it" non puo essere in errore. Pochi byte di RAM.
bool        langInErrore();
bool        langErrore(char* code, size_t cap, int& motivo);   // true = in errore: codice della lingua e motivo (LangGuasto, 1-5)
bool        langErroreJson(HeapBuf& j);          // {"k":0} oppure {"k":1,"c":"<codice>","m":<motivo>}, per /api/status
void        langTick();                          // dal ciclo principale: da l'avviso (registro e allarme giallo) e toglie l'allarme
// Il puntatore di tr() punta in una memoria circolare: resta valido finche dopo la chiamata non si leggono dal file
// LANG_GARANZIA byte di testi nuovi (circa 30 frasi medie, vedi vos_i18n_calc.h). Chi lo tiene piu a lungo (membri, static,
// code, callback) deve COPIARE il testo. Con l'italiano il puntatore e la frase stessa e vale per sempre.

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
