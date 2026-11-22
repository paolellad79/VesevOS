// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_elenco_rule.h
// Regole PURE dell'elenco SHA256SUMS (file singoli in /usr/share/vesevos): formato delle righe, ricerca dello SHA-256 di un file e id
// dell'elenco. Nessun file, nessun hardware: provato sul computer (build/tests/risorse-file). Lo SHA-256 dei file si passa da fuori (RisSha).
// Formato (quello di sha256sum): una riga per file, "<64 cifre minuscole>  <nome>\n", nomi con lettere, cifre, . _ - (massimo 40 caratteri).
// Niente ancora al firmware (decisione dell'utente del 16/11/2026): l'elenco si controlla per FORMATO, per nomi conosciuti dal chiamante e per
// limiti (RIS_ELENCO_MAX byte, RIS_ELENCO_RIGHE righe). Chi sostituisce l'elenco e un Admin; resta l'avviso nell'audit (vos_risorse_tocco).
#pragma once
#include "vos_risorse_rule.h"

#define RIS_ELENCO_MAX    1024       // byte di SHA256SUMS al massimo
#define RIS_ELENCO_RIGHE  16         // righe al massimo
#define RIS_FILE_NOME_MAX 40         // caratteri di un nome di file
#define RIS_ELENCO_ESCLUSO "html.vrs"

struct RisRiga {
  const char* riga;                  // inizio della riga (dentro il testo dato)
  size_t      lunga;                 // byte della riga, a-capo compreso
  const char* nome;                  // inizio del nome
  size_t      nomeLen;
};

inline bool risHexVal(char c, uint8_t& v) {
  if (c >= '0' && c <= '9') { v = (uint8_t)(c - '0'); return true; }
  if (c >= 'a' && c <= 'f') { v = (uint8_t)(c - 'a' + 10); return true; }
  return false;
}

inline bool risNomeFileOk(const char* s, size_t n) {
  if (n < 1 || n > RIS_FILE_NOME_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

// Divide il testo in righe e controlla ogni riga. Ritorna quante righe (da 1 a RIS_ELENCO_RIGHE) o 0 se qualcosa non va:
// riga non valida, nome non valido o doppio, ultimo a-capo mancante, testo troppo lungo.
inline int risElencoRighe(const char* t, size_t n, RisRiga* out) {
  if (!t || n == 0 || n > RIS_ELENCO_MAX) return 0;
  int righe = 0;
  size_t pos = 0;
  while (pos < n) {
    const char* nl = (const char*)memchr(t + pos, '\n', n - pos);
    if (!nl) return 0;
    size_t len = (size_t)(nl - (t + pos)) + 1;
    if (len < 64 + 2 + 1 + 1 || righe >= RIS_ELENCO_RIGHE) return 0;
    uint8_t v;
    for (int i = 0; i < 64; i++) if (!risHexVal(t[pos + i], v)) return 0;
    if (t[pos + 64] != ' ' || t[pos + 65] != ' ') return 0;
    const char* nome = t + pos + 66;
    size_t nomeLen = len - 1 - 66;
    if (!risNomeFileOk(nome, nomeLen)) return 0;
    for (int i = 0; i < righe; i++)
      if (out[i].nomeLen == nomeLen && memcmp(out[i].nome, nome, nomeLen) == 0) return 0;
    out[righe] = RisRiga{t + pos, len, nome, nomeLen};
    righe++;
    pos += len;
  }
  return righe;
}

// Lo SHA-256 scritto per un file dell'elenco. false se l'elenco non e valido o il nome non c'e.
inline bool risElencoCerca(const char* t, size_t n, const char* nome, uint8_t hash[32]) {
  RisRiga r[RIS_ELENCO_RIGHE];
  int k = risElencoRighe(t, n, r);
  size_t nl = nome ? strlen(nome) : 0;
  for (int i = 0; i < k; i++) {
    if (r[i].nomeLen != nl || memcmp(r[i].nome, nome, nl) != 0) continue;
    for (int b = 0; b < 32; b++) {
      uint8_t hi, lo;
      risHexVal(r[i].riga[b * 2], hi);
      risHexVal(r[i].riga[b * 2 + 1], lo);
      hash[b] = (uint8_t)((hi << 4) | lo);
    }
    return true;
  }
  return false;
}

// Nuovo testo di SHA256SUMS: la riga di `nome` prende il posto di quella che c'e (o si aggiunge in fondo), le altre restano uguali.
// t/n = elenco attuale; se manca o non e valido si riparte da un elenco con la sola riga nuova. Ritorna i byte scritti in out, 0 se non entra
// (cap, RIS_ELENCO_MAX, RIS_ELENCO_RIGHE) o se il nome non e valido. Pura: la scrittura sul disco la fa chi chiama (vos_risorse.cpp).
inline size_t risElencoSostituisci(const char* t, size_t n, const char* nome, const uint8_t hash[32], char* out, size_t cap) {
  size_t nl = nome ? strlen(nome) : 0;
  if (!risNomeFileOk(nome, nl) || !out) return 0;
  RisRiga r[RIS_ELENCO_RIGHE];
  int k = (t && n) ? risElencoRighe(t, n, r) : 0;
  static const char CIFRE[] = "0123456789abcdef";
  size_t pos = 0;
  bool trovata = false;
  for (int i = 0; i <= k; i++) {
    bool nuova = (i == k);                                    // l'ultimo giro scrive la riga in fondo, solo se non c'era
    if (nuova && trovata) break;
    bool questa = nuova || (r[i].nomeLen == nl && memcmp(r[i].nome, nome, nl) == 0);
    if (!questa) {
      if (pos + r[i].lunga > cap) return 0;
      memcpy(out + pos, r[i].riga, r[i].lunga);
      pos += r[i].lunga;
      continue;
    }
    if (pos + 64 + 2 + nl + 1 > cap) return 0;
    for (int b = 0; b < 32; b++) {
      out[pos++] = CIFRE[hash[b] >> 4];
      out[pos++] = CIFRE[hash[b] & 15];
    }
    out[pos++] = ' ';
    out[pos++] = ' ';
    memcpy(out + pos, nome, nl);
    pos += nl;
    out[pos++] = '\n';
    trovata = true;
  }
  if (k >= RIS_ELENCO_RIGHE && !trovata) return 0;
  return pos <= RIS_ELENCO_MAX ? pos : 0;
}

// Il chiamante dice se un nome e conosciuto dal firmware (solo i file di risorse veri: niente percorsi, niente nomi inventati).
typedef bool (*RisNomeNoto)(const char* nome, size_t lunghezza);

// L'elenco e valido? RIS_FORMATO = righe non valide, troppe, troppo lungo, o un nome che il firmware non conosce (noto != nullptr).
inline RisEsito risElencoControlla(const char* t, size_t n, RisNomeNoto noto) {
  RisRiga r[RIS_ELENCO_RIGHE];
  int k = risElencoRighe(t, n, r);
  if (k == 0) return RIS_FORMATO;
  if (noto) {
    for (int i = 0; i < k; i++) {
      if (!noto(r[i].nome, r[i].nomeLen)) return RIS_FORMATO;
    }
  }
  return RIS_OK;
}

// ---- Stato di ogni file (ricordato in RAM: un byte per file) e confronto con la riga dell'elenco ----
enum RisStato : uint8_t { RS_NON_CONTROLLATO = 0, RS_OK, RS_DIVERSO, RS_MANCANTE };

// Dall'esito di un controllo allo stato del file. RIS_MANCA = il file non c'e; ogni altro problema = diverso dalla riga dell'elenco.
inline RisStato risStatoDaEsito(RisEsito e) {
  if (e == RIS_OK) return RS_OK;
  return e == RIS_MANCA ? RS_MANCANTE : RS_DIVERSO;
}

// SHA-256 a pezzi di un file, confrontato con quello scritto nell'elenco. E' il confronto di TUTTI i controlli (avvio, prima richiesta,
// caricamento): un solo posto. Uso: avvia(atteso), dai(...) per ogni pezzo, fine() = true se coincide.
class RisConfronto {
 public:
  explicit RisConfronto(const RisSha& sha) : sha_(sha) {}
  RisConfronto(const RisConfronto&) = delete;
  RisConfronto& operator=(const RisConfronto&) = delete;

  void avvia(const uint8_t atteso[32]) {
    memcpy(atteso_, atteso, 32);
    sha_.avvia(sha_.ctx);
    aperto_ = true;
  }
  void dai(const uint8_t* d, size_t n) {
    if (aperto_) sha_.aggiungi(sha_.ctx, d, n);
  }
  bool fine() {
    if (!aperto_) return false;
    aperto_ = false;
    uint8_t h[32];
    sha_.chiudi(sha_.ctx, h);
    uint8_t diverso = 0;
    for (int i = 0; i < 32; i++) diverso |= (uint8_t)(h[i] ^ atteso_[i]);
    return diverso == 0;
  }
  void chiudi(uint8_t out[32]) {            // fine senza confronto: da' lo SHA-256 (per chi lo confronta con un valore scritto nel file stesso)
    if (!aperto_) { memset(out, 0, 32); return; }
    aperto_ = false;
    sha_.chiudi(sha_.ctx, out);
  }
  void annulla() {                          // si e smesso a meta: libera il contesto SHA
    if (!aperto_) return;
    uint8_t h[32];
    sha_.chiudi(sha_.ctx, h);
    aperto_ = false;
  }

 private:
  RisSha  sha_;
  uint8_t atteso_[32] = {0};
  bool    aperto_ = false;
};
