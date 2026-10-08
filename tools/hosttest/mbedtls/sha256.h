// Finto mbedtls/sha256.h per le prove sul computer (il vero e nella libreria della scheda)
#pragma once
#include <stddef.h>
typedef struct { unsigned char x[128]; } mbedtls_sha256_context;
#ifdef __cplusplus
extern "C" {
#endif
void mbedtls_sha256_init(mbedtls_sha256_context*);
void mbedtls_sha256_free(mbedtls_sha256_context*);
int mbedtls_sha256_starts(mbedtls_sha256_context*, int);
int mbedtls_sha256_update(mbedtls_sha256_context*, const unsigned char*, size_t);
int mbedtls_sha256_finish(mbedtls_sha256_context*, unsigned char*);
#ifdef __cplusplus
}
#endif
