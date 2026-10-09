// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tls.cpp
#include "vos_tls.h"
#include "../core/vos_config.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include <Preferences.h>
#include "esp_random.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecp.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/sha256.h"

static String g_cert, g_key, g_fp, g_name;
static bool g_custom = false, g_pending = false;   // pending: certificato nuovo salvato, vale dal prossimo riavvio

static int rng(void*, unsigned char* out, size_t len) { esp_fill_random(out, len); return 0; }

static String fpOf(const unsigned char* der, size_t len) {
  unsigned char h[32];
  mbedtls_sha256(der, len, h, 0);
  static const char* hx = "0123456789ABCDEF";
  String r;
  for (int i = 0; i < 32; i++) { if (i) r += ':'; r += hx[h[i] >> 4]; r += hx[h[i] & 15]; }
  return r;
}

// impronta di un certificato PEM (per quelli caricati)
static String fpOfPem(const String& pem) {
  mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
  String r;
  if (mbedtls_x509_crt_parse(&c, (const unsigned char*)pem.c_str(), pem.length() + 1) == 0) r = fpOf(c.raw.p, c.raw.len);
  mbedtls_x509_crt_free(&c);
  return r;
}

// date di validita del certificato in uso ("AAAA-MM-GG")
static void validityOf(const String& pem, String& from, String& to) {
  from = ""; to = "";
  if (!pem.length()) return;
  mbedtls_x509_crt c; mbedtls_x509_crt_init(&c);
  if (mbedtls_x509_crt_parse(&c, (const unsigned char*)pem.c_str(), pem.length() + 1) == 0) {
    char b[12];
    snprintf(b, sizeof(b), "%04d-%02d-%02d", c.valid_from.year, c.valid_from.mon, c.valid_from.day); from = b;
    snprintf(b, sizeof(b), "%04d-%02d-%02d", c.valid_to.year, c.valid_to.mon, c.valid_to.day); to = b;
  }
  mbedtls_x509_crt_free(&c);
}

static bool generate(bool apply) {
  uint32_t t0 = millis();
  bool ok = false;
  String cert, kpem, fp;
  mbedtls_pk_context key; mbedtls_pk_init(&key);
  mbedtls_x509write_cert crt; mbedtls_x509write_crt_init(&crt);
  unsigned char* buf = (unsigned char*)malloc(2048);
  do {
    if (!buf) break;
    if (mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0) break;
    if (mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key), rng, NULL) != 0) break;
    String cn = "CN=" + cfg.hostname + ".local,O=VesevOS";
    if (mbedtls_x509write_crt_set_subject_name(&crt, cn.c_str()) != 0) break;
    if (mbedtls_x509write_crt_set_issuer_name(&crt, cn.c_str()) != 0) break;
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    unsigned char serial[16]; esp_fill_random(serial, sizeof(serial)); serial[0] &= 0x7F; serial[0] |= 0x01;
    if (mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial)) != 0) break;
    if (mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "20360101000000") != 0) break;
    if (mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1) != 0) break;
    // nomi alternativi: nome.local, nome, 192.168.4.1
    String n1 = cfg.hostname + ".local", n2 = cfg.hostname;
    static unsigned char apIp[4] = {192, 168, 4, 1};
    mbedtls_x509_san_list s3; memset(&s3, 0, sizeof(s3));
    s3.node.type = MBEDTLS_X509_SAN_IP_ADDRESS; s3.node.san.unstructured_name.p = apIp; s3.node.san.unstructured_name.len = 4; s3.next = NULL;
    mbedtls_x509_san_list s2; memset(&s2, 0, sizeof(s2));
    s2.node.type = MBEDTLS_X509_SAN_DNS_NAME; s2.node.san.unstructured_name.p = (unsigned char*)n2.c_str(); s2.node.san.unstructured_name.len = n2.length(); s2.next = &s3;
    mbedtls_x509_san_list s1; memset(&s1, 0, sizeof(s1));
    s1.node.type = MBEDTLS_X509_SAN_DNS_NAME; s1.node.san.unstructured_name.p = (unsigned char*)n1.c_str(); s1.node.san.unstructured_name.len = n1.length(); s1.next = &s2;
    if (mbedtls_x509write_crt_set_subject_alternative_name(&crt, &s1) != 0) break;
    int len = mbedtls_x509write_crt_der(&crt, buf, 2048, rng, NULL);
    if (len <= 0) break;
    fp = fpOf(buf + 2048 - len, len);                       // il DER si scrive in fondo al buffer
    if (mbedtls_x509write_crt_pem(&crt, buf, 2048, rng, NULL) != 0) break;
    cert = String((const char*)buf);
    if (mbedtls_pk_write_key_pem(&key, buf, 2048) != 0) break;
    kpem = String((const char*)buf);
    ok = true;
  } while (0);
  if (buf) { memset(buf, 0, 2048); free(buf); }
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  if (!ok) { vlog("TLS: creazione del certificato non riuscita"); return false; }
  Preferences p;
  if (p.begin("tls", false)) { p.putString("cert", cert); p.putString("key", kpem); p.putString("name", cfg.hostname); p.putBool("custom", false); p.end(); }
  if (apply) { g_cert = cert; g_key = kpem; g_fp = fp; g_name = cfg.hostname; g_custom = false; }   // mai mentre il server lo sta usando
  else g_pending = true;
  vlog("TLS: nuovo certificato per %s.local in %lu ms", cfg.hostname.c_str(), (unsigned long)(millis() - t0));
  return true;
}

bool tlsEnsure() {
  if (!g_cert.length()) {
    Preferences p;
    if (p.begin("tls", true)) { g_cert = p.getString("cert", ""); g_key = p.getString("key", ""); g_name = p.getString("name", ""); g_custom = p.getBool("custom", false); p.end(); }
    if (g_cert.length()) g_fp = fpOfPem(g_cert);
  }
  if (g_cert.length() && g_key.length() && g_fp.length() && (g_custom || g_name == cfg.hostname)) return true;
  return generate(true);
}

const String& tlsCertPem() { return g_cert; }
const String& tlsKeyPem() { return g_key; }
String tlsFingerprint() { return g_fp; }
bool tlsCustom() { return g_custom; }

bool tlsSetCustom(const String& cert, const String& key, String& err) {
  if (cert.indexOf("-----BEGIN CERTIFICATE-----") < 0 || key.indexOf("PRIVATE KEY-----") < 0) { err = tr("Servono certificato e chiave in formato PEM"); return false; }
  if (cert.length() > 3800 || key.length() > 3800) { err = tr("Certificato o chiave troppo grandi"); return false; }
  String fp = fpOfPem(cert);
  if (!fp.length()) { err = tr("Certificato non valido"); return false; }
  mbedtls_pk_context k; mbedtls_pk_init(&k);
  int r = mbedtls_pk_parse_key(&k, (const unsigned char*)key.c_str(), key.length() + 1, NULL, 0, rng, NULL);
  mbedtls_pk_free(&k);
  if (r != 0) { err = tr("Chiave non valida (deve essere senza password)"); return false; }
  Preferences p;
  if (!p.begin("tls", false)) { err = tr("Memoria non disponibile"); return false; }
  p.putString("cert", cert); p.putString("key", key); p.putString("name", cfg.hostname); p.putBool("custom", true); p.end();
  g_pending = true;                                    // il server in funzione usa ancora il vecchio: vale dal riavvio
  vlog("TLS: certificato caricato dall'utente (%s), vale dal prossimo riavvio", fp.substring(0, 23).c_str());
  return true;
}

void tlsRegenerate() { generate(false); }
bool tlsPending() { return g_pending; }

String tlsJson() {
  String vf, vt; validityOf(g_cert, vf, vt);
  return "{\"on\":" + String(cfg.https ? "true" : "false") + ",\"custom\":" + String(g_custom ? "true" : "false") + ",\"pending\":" + String(g_pending ? "true" : "false") +
         ",\"fp\":\"" + g_fp + "\",\"from\":\"" + vf + "\",\"to\":\"" + vt + "\",\"name\":\"" + jsonEscape(cfg.hostname) + ".local\"}";
}
