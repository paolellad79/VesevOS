// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_recupd.cpp
#include "vos_recupd.h"
#include "vos_imgcheck.h"
#include "vos_log.h"
#include "vos_util.h"
#include "vos_i18n.h"
#include "vos_audit.h"
#include "vos_files.h"
#include "../drivers/vos_drv_ota.h"
#include "../drivers/vos_drv_fs.h"
#include "mbedtls/sha256.h"

static uint8_t* g_buf = nullptr;
static uint32_t g_len = 0, g_cap = 0, g_last = 0;
static bool g_over = false;
static String g_ver; static bool g_verRead = false;

void recUpAbort() { if (g_buf) free(g_buf); g_buf = nullptr; g_len = g_cap = 0; g_over = false; }

bool recUpBegin(String& err) {
  if (g_buf && millis() - g_last < 30000UL) { err = tr("Un altro caricamento del recovery e in corso"); return false; }
  recUpAbort();                                                     // un caricamento interrotto da piu di 30 s si butta
  uint32_t cap = drvRecoverySize();
  if (!cap || !drvRecoveryPresent()) { err = tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia."); return false; }
  if (!psramFound() || !(g_buf = (uint8_t*)ps_malloc(cap))) { err = tr("Memoria PSRAM insufficiente per il file"); return false; }
  g_cap = cap; g_last = millis();
  return true;
}

bool recUpAdd(const uint8_t* d, size_t n) {
  if (!g_buf) return false;
  g_last = millis();
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
    case IMG_ARDUINO: return tr("Non e un programma Arduino-ESP32 valido");
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

bool recUpFinish(const String& shaIn, const String& who, String& from, String& to, String& err) {
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
    String want = shaIn; want.trim(); want.toLowerCase();
    if (want.length()) {
      if (want.length() != 64) { err = tr("L'impronta SHA-256 deve avere 64 cifre esadecimali"); break; }
      sha256(g_buf, g_len, h);
      if (hexOf(h, 32) != want) { err = tr("Impronta SHA-256 diversa: il file non e quello atteso"); break; }
    }
    from = recVersion(); to = ver;
    vlog("SISTEMA: scrivo il recovery %s (%lu byte, %s)", ver, (unsigned long)g_len, who.c_str());
    int w = drvRecoveryWrite(g_buf, g_len);
    g_verRead = false;                                              // la versione si rilegge dalla partizione
    if (w != REC_W_OK) { err = writeErr(w); vlog("ERRORE: aggiornamento del recovery non riuscito (codice %d)", w); break; }
    auditEvent(AUD_INFO, "", trf("recovery aggiornato %s -> %s (%s)", from.length() ? from.c_str() : "?", ver, who.c_str()));
    ok = true;
  } while (false);
  recUpAbort();
  return ok;
}

bool recUpFromFile(const String& path, const String& sha, const String& who, String& from, String& to, String& err) {
  String p = fsClean(path);
  if (!p.length() || fsProtected(p) || !drvFsExists(p)) { err = tr("File non trovato"); return false; }
  File f = drvFsOpen(p, "r");
  if (!f) { err = tr("File non trovato"); return false; }
  if (!recUpBegin(err)) { f.close(); return false; }
  uint8_t b[1024]; int n;
  while ((n = f.read(b, sizeof b)) > 0) if (!recUpAdd(b, (size_t)n)) break;
  f.close();
  return recUpFinish(sha, who, from, to, err);
}

String recVersion() {
  if (g_verRead) return g_ver;
  g_ver = ""; g_verRead = true;
  uint32_t size = drvRecoverySize();
  if (!size || !drvRecoveryPresent()) return g_ver;
  ImgScan s; imgScanInit(s);
  uint8_t b[1024];
  for (uint32_t o = 0; o + sizeof(b) <= size && !s.done; o += sizeof(b)) {   // il marchio del recovery nuovo viene prima dei resti del vecchio
    if (!drvRecoveryRead(o, b, sizeof b)) break;
    imgScanFeed(s, b, sizeof b);
  }
  if (s.done) g_ver = s.ver;
  return g_ver;
}
