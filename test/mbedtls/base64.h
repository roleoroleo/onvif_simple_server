/* Minimal mbedTLS base64 stub for test builds only. */
#ifndef MBEDTLS_BASE64_H
#define MBEDTLS_BASE64_H
#include <stddef.h>

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

#endif
