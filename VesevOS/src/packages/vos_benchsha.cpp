// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_benchsha.cpp
// Misura diagnostica: SHA-256 hardware (mbedTLS) contro SHA-256 software (core/vos_sha256_sw), con tre prove:
//  1) tanti hash di un messaggio corto; 2) la formula della password senza String; 3) authHashPass vera (con String e pause).
// Regole di misura: ogni blocco si cronometra da solo e le pause per il watchdog restano FUORI dal cronometro (tranne in authHashPass,
// dove le pause fanno parte del tempo che vede l'utente); i risultati sono letti dopo, cosi il compilatore non toglie il lavoro.
#include "vos_benchsha.h"
#include <string.h>
#include "mbedtls/sha256.h"
#include "../core/vos_i18n.h"
#include "../core/vos_sha256_sw.h"
#include "../security/vos_auth.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_cpu.h"

namespace {
const int MSG_LEN = 100;        // byte di ogni messaggio (come un giro della password: 64 di hash + sale + password)
const int HASHES = 1000;        // hash per ogni prova
const int BLOCK = 100;          // hash per blocco cronometrato
const int RUNS = 5;             // prove ripetute: si riporta la mediana
const int ITER = 3000;          // giri della formula (uguale a HASH_ITER di vos_auth.cpp: se cambia, il controllo "uguale" lo segnala)
const int ITER_BLOCK = 250;     // giri per blocco cronometrato
const char* const TEST_SALT = "bench-salt-0123456789abcdef0123456789";    // valori di prova, NON di un utente
const char* const TEST_PASS = "bench-pass-test";

volatile uint8_t g_sink;        // legge un byte di ogni risultato: il lavoro non si puo eliminare
typedef void (*ShaFn)(const uint8_t* data, size_t len, uint8_t out[32]);

void shaHw(const uint8_t* d, size_t n, uint8_t out[32]) {
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, d, n);
  mbedtls_sha256_finish(&c, out);
  mbedtls_sha256_free(&c);
}

void toHex(const uint8_t d[32], char out[65]) {
  static const char X[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) { out[2 * i] = X[d[i] >> 4]; out[2 * i + 1] = X[d[i] & 15]; }
  out[64] = 0;
}

// Prova di correttezza: valori noti e hardware = software su messaggi di varie lunghezze.
bool digestsOk() {
  static const uint8_t ABC[3] = {'a', 'b', 'c'};
  static const char ABC_HEX[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
  static const char EMPTY_HEX[] = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  static const size_t LENS[] = {1, 55, 56, 63, 64, 65, 100, 119, 120, 128, 130};    // vicino ai limiti dei blocchi
  uint8_t msg[130], a[32], b[32];
  char hex[65];
  for (size_t i = 0; i < sizeof(msg); i++) msg[i] = (uint8_t)(i * 7 + 1);
  shaHw(ABC, 3, a); toHex(a, hex); if (strcmp(hex, ABC_HEX)) return false;
  sha256SwHash(ABC, 3, b); toHex(b, hex); if (strcmp(hex, ABC_HEX)) return false;
  shaHw(msg, 0, a); toHex(a, hex); if (strcmp(hex, EMPTY_HEX)) return false;
  sha256SwHash(msg, 0, b); toHex(b, hex); if (strcmp(hex, EMPTY_HEX)) return false;
  for (size_t n : LENS) {
    shaHw(msg, n, a); sha256SwHash(msg, n, b);
    if (memcmp(a, b, 32)) return false;
  }
  return true;
}

// Cronometra `count` hash dello stesso messaggio, a blocchi; le pause sono fuori dal cronometro. Ritorna i microsecondi totali.
uint32_t timeHashes(ShaFn fn, const uint8_t* msg, size_t len, int count) {
  uint8_t out[32];
  uint32_t total = 0;
  for (int done = 0; done < count; done += BLOCK) {
    uint32_t t0 = drvMicros();
    for (int i = 0; i < BLOCK; i++) { fn(msg, len, out); g_sink = out[0]; }
    total += (uint32_t)drvMicros() - t0;
    drvTaskPause();
  }
  return total;
}

// Come sopra, ma con un solo contesto hardware gia inizializzato (init e free una volta sola).
uint32_t timeHashesReuse(const uint8_t* msg, size_t len, int count) {
  uint8_t out[32];
  uint32_t total = 0;
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  for (int done = 0; done < count; done += BLOCK) {
    uint32_t t0 = drvMicros();
    for (int i = 0; i < BLOCK; i++) {
      mbedtls_sha256_starts(&c, 0); mbedtls_sha256_update(&c, msg, len); mbedtls_sha256_finish(&c, out); g_sink = out[0];
    }
    total += (uint32_t)drvMicros() - t0;
    drvTaskPause();
  }
  mbedtls_sha256_free(&c);
  return total;
}

// La formula di authHashPass senza String: h = SHA(sale+password), poi ITER volte h = SHA(h in esadecimale + sale + password).
// Scrive il risultato in hex e ritorna i microsecondi (solo i blocchi di calcolo, pause escluse).
uint32_t timeFormula(ShaFn fn, char hex[65]) {
  uint8_t buf[64 + 48 + 32], d[32];
  size_t sl = strlen(TEST_SALT), pl = strlen(TEST_PASS);
  memcpy(buf, TEST_SALT, sl); memcpy(buf + sl, TEST_PASS, pl);
  fn(buf, sl + pl, d); toHex(d, hex);
  memcpy(buf + 64, TEST_SALT, sl); memcpy(buf + 64 + sl, TEST_PASS, pl);
  uint32_t total = 0;
  for (int i = 0; i < ITER; i += ITER_BLOCK) {
    uint32_t t0 = drvMicros();
    for (int k = 0; k < ITER_BLOCK; k++) {
      memcpy(buf, hex, 64);
      fn(buf, 64 + sl + pl, d);
      toHex(d, hex);
    }
    total += (uint32_t)drvMicros() - t0;
    drvTaskPause();
  }
  return total;
}

uint32_t median(uint32_t* v, int n) {
  for (int i = 1; i < n; i++) for (int j = i; j > 0 && v[j] < v[j - 1]; j--) { uint32_t t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
  return v[n / 2];
}

// Riga con mediana e minimo di RUNS prove; run() ritorna i microsecondi totali per `count` hash.
template <typename F> void reportRuns(OutSink& o, const char* name, int count, F run) {
  uint32_t v[RUNS];
  for (int i = 0; i < RUNS; i++) v[i] = run();
  uint32_t mn = v[0];
  for (int i = 1; i < RUNS; i++) if (v[i] < mn) mn = v[i];
  uint32_t med = median(v, RUNS);
  uint32_t m10 = (uint32_t)((uint64_t)med * 10 / count), n10 = (uint32_t)((uint64_t)mn * 10 / count);   // decimi di microsecondo per hash
  o.println(trf("%s: %lu.%lu us per hash (mediana di %d prove, minimo %lu.%lu)", name, (unsigned long)(m10 / 10), (unsigned long)(m10 % 10),
                RUNS, (unsigned long)(n10 / 10), (unsigned long)(n10 % 10)));
}
}  // namespace

void benchShaRun(OutSink& o) {
  uint8_t msg[MSG_LEN];
  for (int i = 0; i < MSG_LEN; i++) msg[i] = (uint8_t)(i * 7 + 1);
  o.println(trf("Misura SHA-256 (diagnostica): CPU %lu MHz, messaggio di %d byte, %d hash per prova", (unsigned long)drvCpuMhz(), MSG_LEN, HASHES));
  if (!digestsOk()) { o.println(tr("Verifica degli hash: ERRORE (hardware e software non concordano): misure annullate")); return; }
  o.println(tr("Verifica degli hash (valori noti e hardware uguale al software): OK"));

  timeHashes(shaHw, msg, MSG_LEN, BLOCK);                  // riscaldamento: codice e dati nella cache, non si riporta
  reportRuns(o, tr("Hardware"), HASHES, [&] { return timeHashes(shaHw, msg, MSG_LEN, HASHES); });
  reportRuns(o, tr("Software"), HASHES, [&] { return timeHashes(sha256SwHash, msg, MSG_LEN, HASHES); });
  reportRuns(o, tr("Hardware, contesto riusato"), HASHES, [&] { return timeHashesReuse(msg, MSG_LEN, HASHES); });

  char hwHex[65], swHex[65];
  uint32_t hw[3], sw[3];
  for (int i = 0; i < 3; i++) { hw[i] = timeFormula(shaHw, hwHex); sw[i] = timeFormula(sha256SwHash, swHex); }
  bool same = strcmp(hwHex, swHex) == 0;
  o.println(trf("Formula della password, %d giri, senza String e senza pause: hardware %lu ms, software %lu ms (mediane), risultati uguali: %s",
                ITER, (unsigned long)(median(hw, 3) / 1000), (unsigned long)(median(sw, 3) / 1000), same ? tr("si") : tr("no")));

  uint32_t real[3];
  bool sameReal = true;
  for (int i = 0; i < 3; i++) {
    uint32_t t0 = drvMicros();
    char h[SEC_HEX_BUF];
    authHashPass(TEST_SALT, TEST_PASS, h);                 // quella vera, con le pause per il watchdog
    real[i] = (uint32_t)drvMicros() - t0;
    if (strcmp(h, hwHex)) sameReal = false;
  }
  o.println(trf("authHashPass vera (con String e pause): %lu ms (mediana di 3), uguale al calcolo senza String: %s",
                (unsigned long)(median(real, 3) / 1000), sameReal ? tr("si") : tr("no")));
}
