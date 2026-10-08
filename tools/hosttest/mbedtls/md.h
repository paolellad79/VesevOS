// Finto mbedtls/md.h per le prove sul computer (usa OpenSSL); non va mai nel firmware
#pragma once
#include <stddef.h>
typedef int mbedtls_md_type_t;
#define MBEDTLS_MD_SHA256 6
typedef struct { int t; } mbedtls_md_info_t;
#ifdef __cplusplus
extern "C" {
#endif
const mbedtls_md_info_t* mbedtls_md_info_from_type(mbedtls_md_type_t t);
int mbedtls_md_hmac(const mbedtls_md_info_t*, const unsigned char* key, size_t kl, const unsigned char* in, size_t il, unsigned char* out);
#ifdef __cplusplus
}
#endif
