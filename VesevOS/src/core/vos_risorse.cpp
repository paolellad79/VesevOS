// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse.cpp
// Collega le regole di vos_risorse_rule.h al file system (VosFile, FsAtomicWriter) e a mbedTLS. Vedi vos_risorse.h.
#include "vos_risorse.h"
#include <new>
#include <stdio.h>
#include "vos_risorse_sha.h"
#include "vos_files_stream.h"
#include "vos_risorse_tocco.h"
#include "vos_i18n.h"
#include "../drivers/vos_drv_fs.h"

namespace {

// ---- destinazione: file temporaneo + scambio (FsAtomicWriter) ----
class SinkFs : public RisSink {
 public:
  bool apri(const RisInfo& info) override {
    char path[FS_PATH_MAX + 1];
    ErrBuf err;
    if (!risPercorso(info.nome, path, sizeof path)) return false;
    RisToccoNostro nostro;                                     // la nostra rotta di caricamento non e una manomissione
    if (!fsMkdirs(RIS_DIR, err)) return false;
    return w_.begin(path, true, (size_t)RIS_INTEST + info.lunghezza, err);
  }
  bool scrivi(const uint8_t* d, size_t n) override { return w_.write(d, n); }
  bool conferma() override {
    ErrBuf err;
    RisToccoNostro nostro;
    bool ok = w_.commit(err);
    if (ok) risToccoCaricato("html.vrs");
    return ok;
  }
  void annulla() override { w_.abort(); }

 private:
  FsAtomicWriter w_;
};

// ---- destinazione di un file singolo: stesso schema, nome preso dall'elenco ----
class SinkFileFs : public RisFileSink {
 public:
  bool apri(const char* nome, size_t tetto) override {
    char path[FS_PATH_MAX + 1];
    ErrBuf err;
    snprintf(path, sizeof path, RIS_DIR "/%s", nome);
    RisToccoNostro nostro;
    snprintf(nome_, sizeof nome_, "%s", nome);
    if (!fsMkdirs(RIS_DIR, err)) return false;
    bool ok = w_.begin(path, true, tetto, err);
    spazio_ = !ok && strcmp(err.c_str(), tr("Spazio esaurito")) == 0;
    return ok;
  }
  bool scrivi(const uint8_t* d, size_t n) override { return w_.write(d, n); }
  bool conferma() override {
    ErrBuf err;
    RisToccoNostro nostro;
    bool ok = w_.commit(err);
    if (ok) risToccoCaricato(nome_);
    return ok;
  }
  void annulla() override { w_.abort(); }
  bool pieno() const override { return spazio_ || w_.full(); }

 private:
  FsAtomicWriter w_;
  bool           spazio_ = false;
  char           nome_[RIS_FILE_NOME_MAX + 1] = "";
};

}  // namespace

struct RisFileCarico {
  mbedtls_sha256_context ctx;
  SinkFileFs             sink;
  RisFileScrittore       scr;
  bool                   elenco = false;
  RisFile                file = RF_N;
  RisFileCarico() : scr(risShaMbed(&ctx), sink) { memset(&ctx, 0, sizeof ctx); }
};

struct RisCarico {
  mbedtls_sha256_context ctx;
  SinkFs                 sink;
  RisScrittore           scr;
  RisCarico() : scr(risShaMbed(&ctx), sink) { memset(&ctx, 0, sizeof ctx); }
};

bool risPercorso(const char* nome, char* out, size_t cap) {
  if (!nome || !out || !risNomeOk(nome, strlen(nome))) return false;
  int n = snprintf(out, cap, RIS_DIR "/%s.vrs", nome);
  return n > 0 && (size_t)n < cap;
}

RisEsito risApri(const char* nome, VosFile& f, RisInfo& info) {
  char path[FS_PATH_MAX + 1];
  if (!risPercorso(nome, path, sizeof path)) return RIS_FORMATO;
  f = drvFsOpen(path, "r");
  if (!f || f.isDirectory()) { f.close(); return RIS_MANCA; }
  uint8_t h[RIS_INTEST];
  size_t got = f.read(h, sizeof h);
  RisEsito e = got == sizeof h ? risIntestazione(h, got, RIS_TETTO_BYTE, info) : RIS_TRONCATA;
  if (e == RIS_OK && strcmp(info.nome, nome) != 0) e = RIS_FORMATO;       // il nome dentro il file deve essere quello del file
  if (e == RIS_OK) {
    size_t atteso = (size_t)RIS_INTEST + info.lunghezza;
    size_t vero = f.size();
    if (vero < atteso) e = RIS_TRONCATA;
    else if (vero > atteso) e = RIS_ECCESSO;
  }
  if (e != RIS_OK) f.close();
  return e;
}

RisEsito risControlla(const char* nome, RisInfo* info) {
  VosFile f;
  RisInfo in;
  RisEsito e = risApri(nome, f, in);
  if (e != RIS_OK) return e;
  mbedtls_sha256_context ctx;
  RisVerifica ver(risShaMbed(&ctx));
  uint8_t h[RIS_INTEST];
  if (!f.seek(0) || f.read(h, sizeof h) != sizeof h) return RIS_LETTURA;
  e = ver.dai(h, sizeof h);
  uint8_t buf[1024];
  while (e == RIS_OK) {
    size_t n = f.read(buf, sizeof buf);
    if (n == 0) break;
    e = ver.dai(buf, n);
  }
  if (e == RIS_OK) e = ver.fine();
  if (e == RIS_OK && info) *info = in;
  return e;
}

RisCarico* risCaricoInizia() { return new (std::nothrow) RisCarico(); }

RisEsito risCaricoDai(RisCarico* c, const uint8_t* d, size_t n) {
  return c ? c->scr.dai(d, n) : RIS_SCRITTURA;
}

RisEsito risCaricoFine(RisCarico* c) {
  if (!c) return RIS_SCRITTURA;
  RisEsito e = c->scr.fine();
  delete c;
  return e;
}

void risCaricoAnnulla(RisCarico* c) {
  if (!c) return;
  c->scr.annulla();
  delete c;
}

// ---- file singoli ----
bool risFileCaricabile(const char* nomeInviato) {
  if (!nomeInviato) return false;
  if (strcmp(nomeInviato, "SHA256SUMS") == 0) return true;
  RisFile f;
  return risFileDaNome(nomeInviato, f) && f != RF_HTML;          // la pagina ha il suo caricamento (.vrs con intestazione)
}

RisFileCarico* risFileCaricoInizia(const char* nomeInviato, RisEsito& esito) {
  esito = RIS_SCRITTURA;
  if (!risFileCaricabile(nomeInviato)) { esito = RIS_MANCA; return nullptr; }
  RisFileCarico* c = new (std::nothrow) RisFileCarico();
  if (!c) return nullptr;
  if (strcmp(nomeInviato, "SHA256SUMS") == 0) {
    c->elenco = true;
    esito = c->scr.iniziaElenco(risNomeNotoFw);
  } else {
    risFileDaNome(nomeInviato, c->file);
    char elenco[RIS_ELENCO_MAX];
    size_t n = 0;
    esito = risElencoLeggi(elenco, n);
    if (esito == RIS_OK) esito = c->scr.iniziaFile(elenco, n, nomeInviato, risFileTetto(c->file), risNomeNotoFw);
  }
  if (esito != RIS_OK) {
    delete c;
    return nullptr;
  }
  return c;
}

RisEsito risFileCaricoDai(RisFileCarico* c, const uint8_t* d, size_t n) {
  return c ? c->scr.dai(d, n) : RIS_SCRITTURA;
}

RisEsito risFileCaricoFine(RisFileCarico* c) {
  if (!c) return RIS_SCRITTURA;
  RisEsito e = c->scr.fine();
  if (e == RIS_OK) {
    if (c->elenco) {
      risFileAzzera();                                         // elenco nuovo: ogni file va ricontrollato
      risElencoAvvio();
    } else {
      risFileAzzeraUno(c->file);
      VosFile f;
      size_t len = 0;
      risFileApri(c->file, f, len);                            // lo ricontrolla subito (file piccoli) e ricorda lo stato
    }
  }
  delete c;
  return e;
}

const char* risFileCaricoNome(const RisFileCarico* c) { return c ? c->scr.nome() : ""; }

void risFileCaricoAnnulla(RisFileCarico* c) {
  if (!c) return;
  c->scr.annulla();
  delete c;
}
