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

// Nomi dei file sulla scheda. I quattro .vgz (contenitore VOSR con gzip) hanno ancora il vecchio nome .gz come ripiego, finche non sono stati ricaricati.
const char* const kNomiFile[RF_N] = {"comuni.vgz", "licenza-apache2.vgz", "licenza-bsd3.vgz", "licenza-mit.vgz", "manuale.txt", "radio.txt", "html.vrs"};
const char* const kNomiVecchi[RF_N] = {"comuni.json.gz", "licenza-apache2.txt.gz", "licenza-bsd3.txt.gz", "licenza-mit.txt.gz", nullptr, nullptr, nullptr};
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
  if (n <= 0 || (size_t)n >= cap) return false;
  if (kNomiVecchi[f] && !drvFsExists(out)) {                              // il nuovo non c'e: se c'e il vecchio .gz si usa quello
    char vecchio[RIS_PERCORSO_MAX];
    int m = snprintf(vecchio, sizeof vecchio, RIS_DIR "/%s", kNomiVecchi[f]);
    if (m > 0 && (size_t)m < sizeof vecchio && drvFsExists(vecchio)) {
      memcpy(out, vecchio, (size_t)m + 1);
      return (size_t)m < cap;
    }
  }
  return true;
}

bool risFileDaNome(const char* nome, RisFile& f) {
  if (!nome) return false;
  for (int i = 0; i < RF_N; i++) {
    if (strcmp(nome, kNomiFile[i]) != 0 && !(kNomiVecchi[i] && strcmp(nome, kNomiVecchi[i]) == 0)) continue;
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
  if ((unsigned)rf >= RF_N) return RIS_FORMATO;
  if (risElencoCerca(buf, n, kNomiFile[rf], hash)) return RIS_OK;
  if (kNomiVecchi[rf] && risElencoCerca(buf, n, kNomiVecchi[rf], hash)) return RIS_OK;       // elenco vecchio con il nome .gz
  return RIS_FORMATO;                                                       // il file non e nell'elenco
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

// Versione e firmware minimo scritti DENTRO il file: contenitore VOSR (byte 6-7 e 60-63) oppure righe "# versione=" e "# dipende=" in cima al testo.
// Si legge il file senza controllarlo (e un'informazione: il controllo e quello di SHA256SUMS). ver = "" se non dichiarata. false = file non leggibile.
bool risFileMeta(RisFile f, char* ver, size_t cap, uint32_t* fwMin) {
  if (ver && cap) ver[0] = 0;
  if (fwMin) *fwMin = 0;
  VosFile vf;
  size_t len = 0;
  if (risFileApriGrezzo(f, vf, len) != RIS_OK) return false;
  uint8_t buf[RIS_TESTO_SCAN];
  size_t n = vf.read(buf, sizeof buf);
  vf.close();
  if (n >= 4 && memcmp(buf, "VOSR", 4) == 0) {
    RisInfo i;
    if (risIntestazione(buf, n, 0xFFFFFFFFu, i) != RIS_OK) return false;
    if (ver) risVersioneTesto(i.verMag, i.verMin, ver, cap);
    if (fwMin) *fwMin = i.fwMin;
    return true;
  }
  RisTesto t = risTestoLeggi((const char*)buf, n);
  if (!t.ok) return false;
  if (ver) risVersioneTesto(t.verMag, t.verMin, ver, cap);
  if (fwMin) *fwMin = t.fwMin;
  return true;
}
