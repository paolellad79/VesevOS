// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_file.cpp
// File singoli di /usr/share/vesevos (dati comuni, licenze minori, manuale, tabella radio, pagina) controllati con l'elenco SHA256SUMS.
// Le regole sono in vos_risorse_elenco_rule.h (provate sul computer); qui sono collegate al file system interno e a mbedTLS. Vedi vos_risorse_file.h.
// Lo stato di ogni file (OK / diverso / mancante) sta in RAM in un posto solo: lo scrive il controllo dopo l'avvio (vos_risorse_avviso.cpp) o la
// prima richiesta del file (risFileApri), con lo stesso confronto (RisConfronto).
#include "vos_risorse_file.h"
#include <atomic>
#include <stdio.h>
#include "vos_risorse_sha.h"
#include "../drivers/vos_drv_fs.h"

namespace {

const char* const kNomiFile[RF_N] = {"comuni.json.gz", "licenza-apache2.txt.gz", "licenza-bsd3.txt.gz", "licenza-mit.txt.gz", "manuale.txt", "radio.txt", "html.vrs"};
const size_t FILE_TETTO = 64 * 1024;         // un file singolo al massimo (il piu grande oggi: 13 KB)
std::atomic<uint8_t>  g_stato[RF_N];         // RisStato di ogni file
std::atomic<uint8_t>  g_elenco{0xFF};        // ultimo esito dell'elenco SHA256SUMS (0xFF = non ancora controllato)
std::atomic<uint32_t> g_gen{0};              // cambia a ogni azzeramento

void ricordaElenco(RisEsito e) { g_elenco.store((uint8_t)e); }

// Elenco valido (formato e nomi conosciuti), poi lo SHA-256 dell'intero file aperto in vf (letto a pezzi con lo stesso buffer dell'elenco).
RisEsito controllaFile(RisFile rf, VosFile& vf) {
  char buf[RIS_ELENCO_MAX];
  uint8_t atteso[32];
  RisEsito e = risFileAtteso(rf, atteso);
  if (e != RIS_OK) return e;
  mbedtls_sha256_context ctx;
  RisConfronto cf(risShaMbed(&ctx));
  cf.avvia(atteso);
  for (size_t m; (m = vf.read((uint8_t*)buf, sizeof buf)) > 0;) cf.dai((const uint8_t*)buf, m);
  return cf.fine() ? RIS_OK : RIS_SHA;
}

}  // namespace

bool risNomeNotoFw(const char* nome, size_t lunghezza) {
  if (!nome || lunghezza == 0 || lunghezza > RIS_FILE_NOME_MAX) return false;
  char copia[RIS_FILE_NOME_MAX + 1];
  memcpy(copia, nome, lunghezza);
  copia[lunghezza] = 0;
  RisFile f;
  return risFileDaNome(copia, f);
}

const char* risFileNome(RisFile f) { return (unsigned)f < RF_N ? kNomiFile[f] : ""; }

bool risFilePercorso(RisFile f, char* out, size_t cap) {
  if ((unsigned)f >= RF_N || !out) return false;
  int n = snprintf(out, cap, RIS_DIR "/%s", kNomiFile[f]);
  return n > 0 && (size_t)n < cap;
}

bool risFileDaNome(const char* nome, RisFile& f) {
  if (!nome) return false;
  for (int i = 0; i < RF_N; i++) {
    if (strcmp(nome, kNomiFile[i]) != 0) continue;
    f = (RisFile)i;
    return true;
  }
  return false;
}

size_t risFileTetto(RisFile f) { return f == RF_HTML ? (size_t)RIS_TETTO_BYTE + RIS_INTEST : FILE_TETTO; }

RisEsito risElencoLeggi(char* buf, size_t& n) {
  n = 0;
  VosFile f = drvFsOpen(RIS_DIR "/SHA256SUMS", "r");
  RisEsito e = RIS_OK;
  if (!f || f.isDirectory()) {
    e = RIS_MANCA;
  } else {
    size_t grande = f.size();
    if (grande == 0 || grande > RIS_ELENCO_MAX) {
      e = RIS_GRANDE;
    } else {
      n = f.read((uint8_t*)buf, grande);
      if (n != grande) e = RIS_LETTURA;
    }
  }
  f.close();
  if (e == RIS_OK) {
    e = risElencoControlla(buf, n, risNomeNotoFw);
  }
  ricordaElenco(e);
  return e;
}

RisEsito risFileAtteso(RisFile rf, uint8_t hash[32]) {
  char buf[RIS_ELENCO_MAX];
  size_t n = 0;
  RisEsito e = risElencoLeggi(buf, n);
  if (e != RIS_OK) return e;
  if ((unsigned)rf >= RF_N || !risElencoCerca(buf, n, kNomiFile[rf], hash)) return RIS_FORMATO;   // il file non e nell'elenco
  return RIS_OK;
}

RisEsito risElencoAvvio() {
  char buf[RIS_ELENCO_MAX];
  size_t n = 0;
  return risElencoLeggi(buf, n);
}

RisEsito risElencoStato() {
  uint8_t e = g_elenco.load();
  return e == 0xFF ? RIS_LETTURA : (RisEsito)e;
}

bool risElencoNoto() { return g_elenco.load() != 0xFF; }

RisStato risFileStato(RisFile f) { return (unsigned)f < RF_N ? (RisStato)g_stato[f].load() : RS_NON_CONTROLLATO; }

void risFileImposta(RisFile f, RisStato s) {
  if ((unsigned)f < RF_N) g_stato[f].store((uint8_t)s);
}

RisEsito risFileApriGrezzo(RisFile rf, VosFile& vf, size_t& len) {
  vf.close();
  char path[RIS_PERCORSO_MAX];
  if (!risFilePercorso(rf, path, sizeof path)) return RIS_FORMATO;
  vf = drvFsOpen(path, "r");
  if (!vf || vf.isDirectory()) {
    vf.close();
    return RIS_MANCA;
  }
  len = vf.size();
  if (len == 0 || len > risFileTetto(rf)) {
    vf.close();
    return RIS_GRANDE;
  }
  return RIS_OK;
}

RisEsito risFileApri(RisFile rf, VosFile& vf, size_t& len) {
  RisEsito e = risFileApriGrezzo(rf, vf, len);
  if (e != RIS_OK) {
    if (e == RIS_MANCA) risFileImposta(rf, RS_MANCANTE);
    else risFileImposta(rf, RS_DIVERSO);
    return e;
  }
  if (risFileStato(rf) == RS_OK) return RIS_OK;                // gia controllato per intero (dopo l'avvio o alla prima richiesta)
  const uint32_t gen = g_gen.load();
  e = controllaFile(rf, vf);                                   // si controlla il file gia aperto: quello che si legge dopo e quello controllato
  if (e == RIS_OK && !vf.seek(0)) e = RIS_LETTURA;
  bool daElenco = e != RIS_OK && risElencoStato() != RIS_OK;   // elenco non buono: non e colpa del file, il suo stato resta com'e
  if (!daElenco && gen == g_gen.load()) risFileImposta(rf, risStatoDaEsito(e));
  if (e != RIS_OK) vf.close();
  return e;
}

void risFileAzzera() {
  for (int i = 0; i < RF_N; i++) g_stato[i].store(RS_NON_CONTROLLATO);
  g_elenco.store(0xFF);
  g_gen.fetch_add(1);
}

void risFileAzzeraUno(RisFile f) {
  if ((unsigned)f < RF_N) g_stato[f].store(RS_NON_CONTROLLATO);
  g_gen.fetch_add(1);
}

uint32_t risFileGen() { return g_gen.load(); }
