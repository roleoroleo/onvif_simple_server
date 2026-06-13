/* Stub implementations of mbedTLS functions for test builds.
 * None of these are called by gen_uuid_v5_mac or its helpers. */
#include <string.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

void mbedtls_sha1_init(mbedtls_sha1_context *ctx)    { (void)ctx; }
void mbedtls_sha1_starts(mbedtls_sha1_context *ctx)  { (void)ctx; }
void mbedtls_sha1_update(mbedtls_sha1_context *ctx, const unsigned char *input, size_t ilen)
    { (void)ctx; (void)input; (void)ilen; }
void mbedtls_sha1_finish(mbedtls_sha1_context *ctx, unsigned char output[20])
    { (void)ctx; memset(output, 0, 20); }
void mbedtls_sha1_free(mbedtls_sha1_context *ctx)    { (void)ctx; }

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
    { (void)dst; (void)dlen; (void)src; (void)slen; if (olen) *olen = 0; return -1; }
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
    { (void)dst; (void)dlen; (void)src; (void)slen; if (olen) *olen = 0; return -1; }
