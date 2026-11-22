// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_recupd.cpp
#include "vos_recupd.h"
#include "vos_imgcheck.h"
#include "vos_log.h"
#include "vos_util.h"
#include "vos_parse.h"
#include "vos_i18n.h"
#include "vos_audit.h"
#include "vos_files.h"
#include "vos_paths.h"
#include "../drivers/vos_drv_ota.h"
#include "../drivers/vos_drv_fs.h"
#include "mbedtls/sha256.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include "../boards/vos_board.h"

static uint8_t* g_buf = nullptr;
static uint32_t g_len = 0, g_cap = 0, g_last = 0;
static bool g_over = false;
static char g_ver[16] = ""; static bool g_verRead = false;

// Percorso a blocchi, compilato solo se la scheda non ha PSRAM (VOS_HAS_PSRAM 0).
#if !VOS_HAS_PSRAM
// Percorso a blocchi (schede senza PSRAM): il file non si tiene in memoria, ogni settore da 4 KB si scrive subito nel recovery.
// Il primo settore (con l'intestazione) resta in RAM e si scrive per ultimo; prima di scrivere il settore 1 si cancella il settore 0,
// cosi un recovery a meta non sembra valido. Da quel momento lo stato e "da rifare" finche non finisce bene.
static const uint32_t SEC = 4096;
static bool g_stream = false;                    // caricamento in corso col percorso a blocchi
static uint8_t* g_first = nullptr;               // settore 0 (tenuto fino alla fine)
static uint8_t* g_cur = nullptr;                 // settore in arrivo (dal secondo in poi)
static uint32_t g_fill = 0, g_sec = 0;           // byte nel settore in arrivo, numero del settore in arrivo
static bool g_hdrOk = false, g_erased = false, g_redo = false;
static int g_serr = -1;                          // errore del percorso a blocchi: -1 nessuno, 100+IMG_* file sbagliato, altrimenti REC_W_*
static char g_from[16] = "";
static mbedtls_sha256_context g_sha;
static bool g_shaOn = false;
#endif

// true se i 32 byte sono scritti in want come 64 cifre esadecimali (maiuscole o minuscole)
static bool hexSame(const uint8_t* h, std::string_view want) {
  static const char* hx = "0123456789abcdef";
  if (want.size() != 64) return false;
  for (int i = 0; i < 32; i++) {
    if (tolower((unsigned char)want[i * 2]) != hx[h[i] >> 4] || tolower((unsigned char)want[i * 2 + 1]) != hx[h[i] & 15]) return false;
  }
  return true;
}

#if !VOS_HAS_PSRAM
static void shaStop() { if (g_shaOn) mbedtls_sha256_free(&g_sha); g_shaOn = false; }
#endif

void recUpAbort() {
  if (g_buf) free(g_buf);
  g_buf = nullptr; g_len = g_cap = 0; g_over = false;
#if !VOS_HAS_PSRAM
  if (g_stream && g_erased) {                                       // caricamento interrotto dopo la cancellazione
    g_redo = true;
    vlog("ERRORE: caricamento del recovery interrotto: il recovery va riscritto");
    auditEvent(AUD_YELLOW, "", tr("caricamento del recovery interrotto: da rifare"));
  }
  if (g_first) free(g_first);
  if (g_cur) free(g_cur);
  g_first = g_cur = nullptr;
  shaStop();
  g_stream = false; g_fill = g_sec = 0; g_hdrOk = g_erased = false; g_serr = -1;
#endif
}

bool recUpDaRifare() {
#if !VOS_HAS_PSRAM
  return g_redo;
#else
  return false;
#endif
}

#if !VOS_HAS_PSRAM
static bool streamBegin(uint32_t cap, ErrBuf& err) {
  g_first = (uint8_t*)malloc(SEC);
  g_cur = (uint8_t*)malloc(SEC);
  if (!g_first || !g_cur) { recUpAbort(); err = tr("Memoria insufficiente"); return false; }
  mbedtls_sha256_init(&g_sha); mbedtls_sha256_starts(&g_sha, 0); g_shaOn = true;
  snprintf(g_from, sizeof g_from, "%s", recVersion());
  g_stream = true; g_cap = cap; g_len = 0; g_fill = 0; g_sec = 0; g_hdrOk = g_erased = g_over = false; g_serr = -1;
  g_last = drvMillis();
  return true;
}
#endif

bool recUpBegin(ErrBuf& err) {
#if !VOS_HAS_PSRAM
  bool busy = g_buf || g_stream;
#else
  bool busy = g_buf != nullptr;
#endif
  if (busy && drvMillis() - g_last < 30000UL) { err = tr("Un altro caricamento del recovery e in corso"); return false; }
  recUpAbort();                                                     // un caricamento interrotto da piu di 30 s si butta
  uint32_t cap = drvRecoverySize();
#if !VOS_HAS_PSRAM
  if (cap && !drvPsramPresent()) return streamBegin(cap, err);      // senza PSRAM: a blocchi
#endif
  if (!cap || !drvRecoveryPresent()) { err = tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia."); return false; }
  if (!(g_buf = (uint8_t*)drvPsramMalloc(cap))) { err = tr("Memoria PSRAM insufficiente per il file"); return false; }
  g_cap = cap; g_last = drvMillis();
  return true;
}

#if !VOS_HAS_PSRAM
// Scrive il settore pieno (o l'ultimo parziale) nel recovery. Prima del primo settore dopo lo 0 cancella lo 0.
static int streamPut(uint32_t sec, const uint8_t* d, uint32_t n) {
  if (!g_erased) {
    int w = drvRecoveryBlkStart();
    if (w != REC_W_OK) return w;
    g_erased = true; g_redo = true; g_verRead = false;                                // da qui il vecchio recovery non c'e piu
    vlog("SISTEMA: scrivo il recovery a blocchi (scheda senza PSRAM)");
    w = drvRecoveryBlkPut(0, nullptr, 0);
    if (w != REC_W_OK) return w;
  }
  int w = drvRecoveryBlkPut(sec * SEC, d, n);
  if ((sec & 15) == 0) drvDelayMs(1);                               // lascia respirare gli altri task
  return w;
}

static bool streamAdd(const uint8_t* d, size_t n) {
  if (g_serr >= 0 || g_over) return false;
  if (n > g_cap - g_len) { g_over = true; return false; }
  mbedtls_sha256_update(&g_sha, d, n);
  while (n) {
    uint8_t* b = g_sec == 0 ? g_first : g_cur;
    size_t k = SEC - g_fill; if (k > n) k = n;
    memcpy(b + g_fill, d, k);
    g_fill += (uint32_t)k; g_len += (uint32_t)k; d += k; n -= k;
    if (!g_hdrOk && g_sec == 0 && g_fill >= IMG_HDR) {              // intestazione controllata PRIMA di cancellare
      int e = imgHeadCheck(g_first, g_fill, drvImgChipId());
      if (e != IMG_OK) { g_serr = 100 + e; return false; }
      g_hdrOk = true;
    }
    if (g_fill == SEC && n > 0) {                                   // settore pieno e c'e ancora altro: si scrive (il settore 0 resta)
      if (g_sec > 0) {
        int w = streamPut(g_sec, g_cur, SEC);
        if (w != REC_W_OK) { g_serr = w; return false; }
      }
      g_sec++; g_fill = 0;
    }
  }
  return true;
}
#endif

bool recUpAdd(const uint8_t* d, size_t n) {
  g_last = drvMillis();
#if !VOS_HAS_PSRAM
  if (g_stream) return streamAdd(d, n);
#endif
  if (!g_buf) return false;
  if (g_over || n > g_cap - g_len) { g_over = true; return false; }
  memcpy(g_buf + g_len, d, n); g_len += (uint32_t)n;
  return true;
}

static void sha256(const uint8_t* d, size_t n, uint8_t out[32]) {
  mbedtls_sha256_context c; mbedtls_sha256_init(&c); mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, d, n); mbedtls_sha256_finish(&c, out); mbedtls_sha256_free(&c);
}

static const char* imgErr(int e) {
  switch (e) {
    case IMG_SMALL: return tr("File troppo piccolo: non e un programma");
    case IMG_MAGIC: return tr("Non e un programma ESP32 (intestazione errata)");
    case IMG_CHIP: return tr("Programma per un altro tipo di chip (serve ESP32-S3)");
    case IMG_APPDESC: return tr("Non e un programma VesevOS valido");
    case IMG_BIG: return tr("File troppo grande per lo spazio del recovery");
    case IMG_SEGS: return tr("File rovinato (segmenti)");
    case IMG_SUM: return tr("File rovinato (somma di controllo)");
    case IMG_TAIL: return tr("File incompleto o con dati in piu");
    default: return tr("File incompleto");
  }
}

static const char* writeErr(int w) {
  switch (w) {
    case REC_W_NOPART: return tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia.");
    case REC_W_SIZE: return tr("File troppo grande per lo spazio del recovery");
    case REC_W_RUNNING: return tr("VesevOS non sta partendo dal suo spazio (app0): aggiornamento annullato");
    case REC_W_BOOT: return tr("Non riesco a impostare l'avvio di VesevOS: recovery non toccato");
    case REC_W_ERASE: return tr("Errore nel cancellare il recovery: ricaricalo");
    case REC_W_WRITE: return tr("Errore nello scrivere il recovery: ricaricalo");
    default: return tr("Il recovery scritto non corrisponde al file: ricaricalo");
  }
}

#if !VOS_HAS_PSRAM
// Fine del percorso a blocchi: ultimi settori, impronta attesa, poi controllo dell'immagine scritta (senza caricarla in RAM).
static void streamBroken(ErrBuf& err, const char* why) {
  err = tr("Il recovery e rovinato: riprova l'aggiornamento prima di riavviare");
  vlog("ERRORE: recovery scritto a blocchi non valido (%s)", why);
  auditEvent(AUD_RED, "", trf("recovery rovinato dopo la scrittura a blocchi (%s)", why).c_str());
}

static bool streamCheck(const uint8_t want[32], char* ver, size_t vn) {
  uint32_t il = 0;
  if (!drvRecoveryBlkVerify(&il) || il != g_len) return false;
  mbedtls_sha256_context c; mbedtls_sha256_init(&c); mbedtls_sha256_starts(&c, 0);
  ImgScan sc; imgScanInit(sc);
  uint8_t b[1024], h[32];
  for (uint32_t o = 0; o < g_len; o += sizeof b) {
    uint32_t k = (g_len - o < sizeof b) ? g_len - o : (uint32_t)sizeof b;
    if (!drvRecoveryRead(o, b, k)) { mbedtls_sha256_free(&c); return false; }
    mbedtls_sha256_update(&c, b, k);
    if (!sc.done) imgScanFeed(sc, b, k);
  }
  mbedtls_sha256_finish(&c, h); mbedtls_sha256_free(&c);
  if (memcmp(h, want, 32) != 0 || !sc.done) return false;
  snprintf(ver, vn, "%s", sc.ver);
  return true;
}

static bool streamFinish(const char* shaIn, const char* who, char* from, char* to, size_t verLen, ErrBuf& err) {
  if (g_serr >= 100) {
    int e = g_serr - 100;
    if (e == IMG_CHIP) err = trf("Programma per un altro tipo di chip (serve %s)", drvChipModel());
    else err = imgErr(e);
    return false;
  }
  if (g_serr >= 0) { err = writeErr(g_serr); vlog("ERRORE: aggiornamento del recovery non riuscito (codice %d)", g_serr); return false; }
  if (g_over) { err = tr("File troppo grande per lo spazio del recovery"); return false; }
  if (!g_hdrOk || g_len < SEC) { err = imgErr(IMG_SMALL); return false; }       // finora non si e toccato nulla
  uint8_t h[32];
  mbedtls_sha256_finish(&g_sha, h);
  std::string_view want = trimView(shaIn ? shaIn : "");
  if (want.size() != 64) { err = tr("L'impronta SHA-256 deve avere 64 cifre esadecimali"); return false; }   // obbligatoria (20/11/2026)
  if (!hexSame(h, want)) { err = tr("Impronta SHA-256 diversa: il file non e quello atteso"); return false; }
  if (g_sec > 0) {
    int w = streamPut(g_sec, g_cur, g_fill);
    if (w != REC_W_OK) { err = writeErr(w); vlog("ERRORE: aggiornamento del recovery non riuscito (codice %d)", w); return false; }
  }
  int w = streamPut(0, g_first, g_sec == 0 ? g_fill : SEC);                 // l'intestazione per ultima
  if (w != REC_W_OK) { err = writeErr(w); vlog("ERRORE: aggiornamento del recovery non riuscito (codice %d)", w); return false; }
  g_verRead = false;
  char ver[16];
  if (!streamCheck(h, ver, sizeof ver)) { streamBroken(err, "controllo finale"); return false; }
  snprintf(from, verLen, "%s", g_from); snprintf(to, verLen, "%s", ver);
  g_redo = false;
  auditEvent(AUD_INFO, "", trf("recovery aggiornato %s -> %s (%s, a blocchi)", from[0] ? from : "?", ver, who).c_str());
  return true;
}
#endif

bool recUpFinish(const char* shaIn, const char* who, char* from, char* to, size_t verLen, ErrBuf& err) {
#if !VOS_HAS_PSRAM
  if (g_stream) {
    bool r = streamFinish(shaIn, who, from, to, verLen, err);
    g_erased = false;                                               // la fine e gia stata gestita: niente avviso di interruzione
    if (!r && g_redo) vlog("ERRORE: il recovery va riscritto (da rifare)");
    recUpAbort();
    return r;
  }
#endif
  if (!g_buf) { err = tr("Caricamento non riuscito"); return false; }
  bool ok = false;
  do {
    if (g_over) { err = tr("File troppo grande per lo spazio del recovery"); break; }
    uint32_t hashOff = 0;
    int e = imgCheck(g_buf, g_len, g_cap, hashOff);
    if (e != IMG_OK) { err = imgErr(e); break; }
    uint8_t h[32];
    if (hashOff) { sha256(g_buf, hashOff, h); if (memcmp(h, g_buf + hashOff, 32) != 0) { err = tr("File rovinato (impronta interna sbagliata)"); break; } }
    char ver[16];
    if (!imgRecVersion(g_buf, g_len, ver, sizeof ver)) { err = tr("Non e un recovery VesevOS (manca il marchio): forse e il firmware?"); break; }
    std::string_view want = trimView(shaIn ? shaIn : "");
    if (want.size() != 64) { err = tr("L'impronta SHA-256 deve avere 64 cifre esadecimali"); break; }   // obbligatoria (20/11/2026)
    sha256(g_buf, g_len, h);
    if (!hexSame(h, want)) { err = tr("Impronta SHA-256 diversa: il file non e quello atteso"); break; }
    snprintf(from, verLen, "%s", recVersion()); snprintf(to, verLen, "%s", ver);
    vlog("SISTEMA: scrivo il recovery %s (%lu byte, %s)", ver, (unsigned long)g_len, who);
    int w = drvRecoveryWrite(g_buf, g_len);
    g_verRead = false;                                              // la versione si rilegge dalla partizione
    if (w != REC_W_OK) { err = writeErr(w); vlog("ERRORE: aggiornamento del recovery non riuscito (codice %d)", w); break; }
    auditEvent(AUD_INFO, "", trf("recovery aggiornato %s -> %s (%s)", from[0] ? from : "?", ver, who).c_str());
    ok = true;
  } while (false);
  recUpAbort();
  return ok;
}

bool recUpFromFile(const char* path, const char* sha, const char* who, char* from, char* to, size_t verLen, ErrBuf& err) {
  char p[FS_PATH_MAX + 1];
  if (!fsCleanTo(path ? path : "", p, sizeof p) || !p[0] || fsProtected(p) || !drvFsExists(p)) { err = tr("File non trovato"); return false; }
  VosFile f = drvFsOpen(p, "r");
  if (!f) { err = tr("File non trovato"); return false; }
  if (!recUpBegin(err)) { f.close(); return false; }
  uint8_t b[1024]; int n;
  while ((n = f.read(b, sizeof b)) > 0) if (!recUpAdd(b, (size_t)n)) break;
  f.close();
  return recUpFinish(sha, who, from, to, verLen, err);
}

const char* recVersion() {
  if (g_verRead) return g_ver;
  g_ver[0] = 0; g_verRead = true;
  uint32_t size = drvRecoverySize();
  if (!size || !drvRecoveryPresent()) return g_ver;
  ImgScan s; imgScanInit(s);
  uint8_t b[1024];
  for (uint32_t o = 0; o + sizeof(b) <= size && !s.done; o += sizeof(b)) {   // il marchio del recovery nuovo viene prima dei resti del vecchio
    if (!drvRecoveryRead(o, b, sizeof b)) break;
    imgScanFeed(s, b, sizeof b);
  }
  if (s.done) snprintf(g_ver, sizeof g_ver, "%s", s.ver);
  return g_ver;
}
