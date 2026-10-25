// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tls.cpp
#include "vos_tls.h"
#include "../core/vos_config.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_nvs.h"
#include "esp_random.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha256.h"
#include "../drivers/vos_drv_time.h"

#define TLS_PEM_MAX 4096                          // un PEM (certificato o chiave) non supera questa dimensione
static char* g_cert = nullptr;                    // certificato e chiave in uso: blocchi di heap, mai liberati mentre il server li usa
static char* g_key = nullptr;
static char g_fp[TLS_FP_BUF] = "";
static char g_name[40] = "";                      // nome della scheda per cui e stato fatto il certificato
static bool g_custom = false, g_pending = false;   // pending: certificato nuovo salvato, vale dal prossimo riavvio

static int rng(void*, unsigned char* out, size_t len) { esp_fill_random(out, len); return 0; }

static void fpOf(const unsigned char* der, size_t len, char out[TLS_FP_BUF]) {
  unsigned char h[32];
  mbedtls_sha256(der, len, h, 0);
  static const char* hx = "0123456789ABCDEF";
  size_t k = 0;
  for (int i = 0; i < 32; i++) { if (i) out[k++] = ':'; out[k++] = hx[h[i] >> 4]; out[k++] = hx[h[i] & 15]; }
  out[k] = 0;
}

// impronta di un certificato PEM (per quelli caricati); out vuoto se il PEM non e valido
static void fpOfPem(const char* pem, char out[TLS_FP_BUF]) {
  out[0] = 0;
  mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
  if (mbedtls_x509_crt_parse(&c, (const unsigned char*)pem, strlen(pem) + 1) == 0) fpOf(c.raw.p, c.raw.len, out);
  mbedtls_x509_crt_free(&c);
}

// date di validita del certificato in uso ("AAAA-MM-GG", 11 byte ciascuna)
static void validityOf(const char* pem, char from[11], char to[11]) {
  from[0] = 0; to[0] = 0;
  if (!pem || !pem[0]) return;
  mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
  if (mbedtls_x509_crt_parse(&c, (const unsigned char*)pem, strlen(pem) + 1) == 0) {
    snprintf(from, 11, "%04d-%02d-%02d", c.valid_from.year, c.valid_from.mon, c.valid_from.day);
    snprintf(to, 11, "%04d-%02d-%02d", c.valid_to.year, c.valid_to.mon, c.valid_to.day);
  }
  mbedtls_x509_crt_free(&c);
}

static bool generate(bool apply) {
  uint32_t t0 = drvMillis();
  bool ok = false;
  char* cert = nullptr; char* kpem = nullptr;
  char fp[TLS_FP_BUF] = "";
  mbedtls_pk_context key; mbedtls_pk_init(&key);
  mbedtls_x509write_cert crt; mbedtls_x509write_crt_init(&crt);
  unsigned char* buf = (unsigned char*)malloc(2048);
  do {
    if (!buf) break;
    if (mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) break;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), rng, NULL) != 0) break;
    char cn[96];
    snprintf(cn, sizeof(cn), "CN=%s.local,O=VesevOS", cfg.hostname.c_str());
    if (mbedtls_x509write_crt_set_subject_name(&crt, cn) != 0) break;
    if (mbedtls_x509write_crt_set_issuer_name(&crt, cn) != 0) break;
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    unsigned char serial[16]; esp_fill_random(serial, sizeof(serial)); serial[0] &= 0x7F; serial[0] |= 0x01;
    if (mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial)) != 0) break;
    if (mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "20360101000000") != 0) break;
    if (mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1) != 0) break;
    // nomi alternativi: nome.local, nome, 192.168.4.1
    char n1[80], n2[80];
    snprintf(n2, sizeof(n2), "%s", cfg.hostname.c_str());
    snprintf(n1, sizeof(n1), "%s.local", cfg.hostname.c_str());
    static unsigned char apIp[4] = {192, 168, 4, 1};
    mbedtls_x509_san_list s3; memset(&s3, 0, sizeof(s3));
    s3.node.type = MBEDTLS_X509_SAN_IP_ADDRESS; s3.node.san.unstructured_name.p = apIp; s3.node.san.unstructured_name.len = 4; s3.next = NULL;
    mbedtls_x509_san_list s2; memset(&s2, 0, sizeof(s2));
    s2.node.type = MBEDTLS_X509_SAN_DNS_NAME; s2.node.san.unstructured_name.p = (unsigned char*)n2; s2.node.san.unstructured_name.len = strlen(n2); s2.next = &s3;
    mbedtls_x509_san_list s1; memset(&s1, 0, sizeof(s1));
    s1.node.type = MBEDTLS_X509_SAN_DNS_NAME; s1.node.san.unstructured_name.p = (unsigned char*)n1; s1.node.san.unstructured_name.len = strlen(n1); s1.next = &s2;
    if (mbedtls_x509write_crt_set_subject_alternative_name(&crt, &s1) != 0) break;
    int len = mbedtls_x509write_crt_der(&crt, buf, 2048, rng, NULL);
    if (len <= 0) break;
    fpOf(buf + 2048 - len, len, fp);                       // il DER si scrive in fondo al buffer
    if (mbedtls_x509write_crt_pem(&crt, buf, 2048, rng, NULL) != 0) break;
    cert = strdup((const char*)buf);
    if (mbedtls_pk_write_key_pem(&key, buf, 2048) != 0) break;
    kpem = strdup((const char*)buf);
    ok = cert && kpem;
  } while (0);
  if (buf) { memset(buf, 0, 2048); free(buf); }
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  if (!ok) { free(cert); free(kpem); vlog("TLS: creazione del certificato non riuscita"); return false; }
  DrvNvs p;
  if (p.begin("tls", false)) { p.putString("cert", cert); p.putString("key", kpem); p.putString("name", cfg.hostname.c_str()); p.putBool("custom", false); p.end(); }
  if (apply) {                                       // mai mentre il server lo sta usando
    free(g_cert); free(g_key);
    g_cert = cert; g_key = kpem;
    memcpy(g_fp, fp, sizeof(g_fp)); snprintf(g_name, sizeof(g_name), "%s", cfg.hostname.c_str()); g_custom = false;
  } else { free(cert); free(kpem); g_pending = true; }
  vlog("TLS: nuovo certificato per %s.local in %lu ms", cfg.hostname.c_str(), (unsigned long)(drvMillis() - t0));
  return true;
}

// Legge dalla NVS un PEM e ne fa un blocco di heap della lunghezza giusta (nullptr se manca)
static char* loadPem(DrvNvs& p, const char* key) {
  char* tmp = (char*)malloc(TLS_PEM_MAX);
  if (!tmp) return nullptr;
  tmp[0] = 0;
  p.getString(key, tmp, TLS_PEM_MAX);
  char* r = tmp[0] ? strdup(tmp) : nullptr;
  memset(tmp, 0, TLS_PEM_MAX);
  free(tmp);
  return r;
}

bool tlsEnsure() {
  if (!g_cert || !g_cert[0]) {
    DrvNvs p;
    if (p.begin("tls", true)) {
      g_cert = loadPem(p, "cert"); g_key = loadPem(p, "key");
      g_name[0] = 0; p.getString("name", g_name, sizeof(g_name));
      g_custom = p.getBool("custom", false);
      p.end();
    }
    if (g_cert && g_cert[0]) fpOfPem(g_cert, g_fp);
  }
  if (g_cert && g_key && g_fp[0] && (g_custom || strcmp(g_name, cfg.hostname.c_str()) == 0)) return true;
  return generate(true);
}

const char* tlsCertPem() { return g_cert ? g_cert : ""; }
const char* tlsKeyPem() { return g_key ? g_key : ""; }
const char* tlsFingerprint() { return g_fp; }
bool tlsCustom() { return g_custom; }

bool tlsSetCustom(const char* certPem, const char* keyPem, ErrBuf& err) {
  if (!strstr(certPem, "-----BEGIN CERTIFICATE-----") || !strstr(keyPem, "PRIVATE KEY-----")) { err = tr("Servono certificato e chiave in formato PEM"); return false; }
  size_t certLen = strlen(certPem), keyLen = strlen(keyPem);
  if (certLen > 3800 || keyLen > 3800) { err = tr("Certificato o chiave troppo grandi"); return false; }
  char fp[TLS_FP_BUF];
  fpOfPem(certPem, fp);
  if (!fp[0]) { err = tr("Certificato non valido"); return false; }
  mbedtls_pk_context k; mbedtls_pk_init(&k);
  int r = mbedtls_pk_parse_key(&k, (const unsigned char*)keyPem, keyLen + 1, NULL, 0, rng, NULL);
  mbedtls_pk_free(&k);
  if (r != 0) { err = tr("Chiave non valida (deve essere senza password)"); return false; }
  DrvNvs p;
  if (!p.begin("tls", false)) { err = tr("Memoria non disponibile"); return false; }
  p.putString("cert", certPem); p.putString("key", keyPem); p.putString("name", cfg.hostname.c_str()); p.putBool("custom", true); p.end();
  g_pending = true;                                    // il server in funzione usa ancora il vecchio: vale dal riavvio
  vlog("TLS: certificato caricato dall'utente (%.23s), vale dal prossimo riavvio", fp);
  return true;
}

void tlsRegenerate() { generate(false); }
bool tlsPending() { return g_pending; }

void tlsJson(HeapBuf& j) {
  char vf[11], vt[11]; validityOf(g_cert, vf, vt);
  j.addf("{\"on\":%s,\"custom\":%s,\"pending\":%s,\"fp\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"name\":\"",
         cfg.https ? "true" : "false", g_custom ? "true" : "false", g_pending ? "true" : "false", g_fp, vf, vt);
  j.addJson(cfg.hostname.c_str(), cfg.hostname.length());
  j.add(".local\"}");
}
