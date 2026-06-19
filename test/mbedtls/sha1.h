/* Minimal mbedTLS SHA-1 stub for test builds only. */
#ifndef MBEDTLS_SHA1_H
#define MBEDTLS_SHA1_H
#include <stddef.h>

typedef struct { unsigned char state[20]; } mbedtls_sha1_context;

void mbedtls_sha1_init(mbedtls_sha1_context *ctx);
void mbedtls_sha1_starts(mbedtls_sha1_context *ctx);
void mbedtls_sha1_update(mbedtls_sha1_context *ctx, const unsigned char *input, size_t ilen);
void mbedtls_sha1_finish(mbedtls_sha1_context *ctx, unsigned char output[20]);
void mbedtls_sha1_free(mbedtls_sha1_context *ctx);

#endif
