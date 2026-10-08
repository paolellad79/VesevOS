// Finto mbedtls/gcm.h per le prove sul computer (usa OpenSSL); non va mai nel firmware
#pragma once
#include <stddef.h>
#define MBEDTLS_CIPHER_ID_AES 2
#define MBEDTLS_GCM_ENCRYPT 1
typedef struct { unsigned char key[32]; } mbedtls_gcm_context;
#ifdef __cplusplus
extern "C" {
#endif
void mbedtls_gcm_init(mbedtls_gcm_context*);
void mbedtls_gcm_free(mbedtls_gcm_context*);
int mbedtls_gcm_setkey(mbedtls_gcm_context*, int cipher, const unsigned char* key, unsigned int bits);
int mbedtls_gcm_crypt_and_tag(mbedtls_gcm_context*, int mode, size_t len, const unsigned char* iv, size_t ivl, const unsigned char* aad, size_t al, const unsigned char* in, unsigned char* out, size_t tl, unsigned char* tag);
int mbedtls_gcm_auth_decrypt(mbedtls_gcm_context*, size_t len, const unsigned char* iv, size_t ivl, const unsigned char* aad, size_t al, const unsigned char* tag, size_t tl, const unsigned char* in, unsigned char* out);
#ifdef __cplusplus
}
#endif
