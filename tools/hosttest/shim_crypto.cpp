// Finte funzioni mbedtls md/gcm fatte con OpenSSL, solo per le prove sul computer (tools/hosttest)
#include "mbedtls/md.h"
#include "mbedtls/gcm.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string.h>
static mbedtls_md_info_t g_i = {6};
extern "C" {
const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t t) { return t == 6 ? &g_i : nullptr; }
int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* k, size_t kl, const unsigned char* in, size_t il, unsigned char* out) {
  unsigned int n = 32; return HMAC(EVP_sha256(), k, (int)kl, in, il, out, &n) ? 0 : -1;
}
void mbedtls_gcm_init(mbedtls_gcm_context* c) { memset(c, 0, sizeof(*c)); }
void mbedtls_gcm_free(mbedtls_gcm_context* c) { memset(c, 0, sizeof(*c)); }
int mbedtls_gcm_setkey(mbedtls_gcm_context* c, int, const unsigned char* k, unsigned int bits) { if (bits != 256) return -1; memcpy(c->key, k, 32); return 0; }
int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context* c, int, size_t len, const unsigned char* iv, size_t ivl, const unsigned char* aad, size_t al, const unsigned char* in, unsigned char* out, size_t tl, unsigned char* tag) {
  EVP_CIPHER_CTX* x = EVP_CIPHER_CTX_new(); int l = 0, ok = 1;
  ok &= EVP_EncryptInit_ex(x, EVP_aes_256_gcm(), 0, 0, 0); EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_SET_IVLEN, (int)ivl, 0);
  ok &= EVP_EncryptInit_ex(x, 0, 0, c->key, iv); ok &= EVP_EncryptUpdate(x, 0, &l, aad, (int)al);
  ok &= EVP_EncryptUpdate(x, out, &l, in, (int)len); ok &= EVP_EncryptFinal_ex(x, out + l, &l);
  ok &= EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_GET_TAG, (int)tl, tag); EVP_CIPHER_CTX_free(x); return ok ? 0 : -1;
}
int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context* c, size_t len, const unsigned char* iv, size_t ivl, const unsigned char* aad, size_t al, const unsigned char* tag, size_t tl, const unsigned char* in, unsigned char* out) {
  EVP_CIPHER_CTX* x = EVP_CIPHER_CTX_new(); int l = 0, ok = 1;
  ok &= EVP_DecryptInit_ex(x, EVP_aes_256_gcm(), 0, 0, 0); EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_SET_IVLEN, (int)ivl, 0);
  ok &= EVP_DecryptInit_ex(x, 0, 0, c->key, iv); ok &= EVP_DecryptUpdate(x, 0, &l, aad, (int)al);
  ok &= EVP_DecryptUpdate(x, out, &l, in, (int)len); EVP_CIPHER_CTX_ctrl(x, EVP_CTRL_GCM_SET_TAG, (int)tl, (void*)tag);
  ok &= EVP_DecryptFinal_ex(x, out + l, &l) > 0; EVP_CIPHER_CTX_free(x); return ok ? 0 : -1;
}
}
