/*
 * Unit tests for gen_uuid_v5_mac() in utils.c.
 *
 * Build: see Makefile target 'test'
 * Run:   make test
 *
 * KAT vectors verified with Python:
 *   import uuid
 *   NS = uuid.UUID('6ba7b810-9dad-11d1-80b4-00c04fd430c8')
 *   str(uuid.uuid5(NS, 'mac:aa:bb:cc:dd:ee:ff'))
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Pull in the function under test */
#include "../utils.h"

/* -------------------------------------------------------------------------
 * Minimal test harness
 * ---------------------------------------------------------------------- */
static int g_tests = 0;
static int g_pass  = 0;
static int g_fail  = 0;

#define CHECK(desc, expr) do {                                        \
    g_tests++;                                                        \
    if (expr) { g_pass++; printf("  PASS  %s\n", (desc)); }         \
    else       { g_fail++; printf("  FAIL  %s\n", (desc)); }        \
} while (0)

/* -------------------------------------------------------------------------
 * Test: known-answer vectors
 * ---------------------------------------------------------------------- */
static void test_kat_vectors(void)
{
    struct { const char *label; uint8_t mac[6]; const char *expected; } cases[] = {
        { "aa:bb:cc:dd:ee:ff", {0xaa,0xbb,0xcc,0xdd,0xee,0xff},
          "b47bd8ea-7b8b-58c7-8354-88dbaddf57aa" },
        { "00:00:00:00:00:00", {0x00,0x00,0x00,0x00,0x00,0x00},
          "cf42c1e8-5d21-513e-a915-aeb28c4ef818" },
        { "ff:ff:ff:ff:ff:ff", {0xff,0xff,0xff,0xff,0xff,0xff},
          "75329539-b98d-596a-9553-ff2c46753546" },
        { "dc:a6:32:01:23:45", {0xdc,0xa6,0x32,0x01,0x23,0x45},
          "06faaef9-3b02-5952-b349-a1a1375d80b1" },
    };

    char desc[64], result[UUID_LEN + 2];
    unsigned int i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        gen_uuid_v5_mac(result, cases[i].mac);
        snprintf(desc, sizeof(desc), "KAT mac=%s", cases[i].label);
        CHECK(desc, strcmp(result, cases[i].expected) == 0);
        if (strcmp(result, cases[i].expected) != 0)
            printf("        got      %s\n"
                   "        expected %s\n", result, cases[i].expected);
    }
}

/* -------------------------------------------------------------------------
 * Test: output format  (length, hyphens, hex chars, version, variant)
 * ---------------------------------------------------------------------- */
static void test_output_format(void)
{
    static const uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    char buf[UUID_LEN + 2];
    int i;

    gen_uuid_v5_mac(buf, mac);

    CHECK("length == 36", strlen(buf) == UUID_LEN);
    CHECK("hyphen at [8]",  buf[8]  == '-');
    CHECK("hyphen at [13]", buf[13] == '-');
    CHECK("hyphen at [18]", buf[18] == '-');
    CHECK("hyphen at [23]", buf[23] == '-');

    /* All non-hyphen chars must be lowercase hex digits */
    int all_hex = 1;
    for (i = 0; i < UUID_LEN; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        if (!((buf[i] >= '0' && buf[i] <= '9') ||
              (buf[i] >= 'a' && buf[i] <= 'f'))) {
            all_hex = 0; break;
        }
    }
    CHECK("all non-hyphen chars are lowercase hex", all_hex);

    CHECK("version nibble == '5'", buf[14] == '5');
    CHECK("variant high bits in {8,9,a,b}", buf[19] == '8' || buf[19] == '9' ||
                                             buf[19] == 'a' || buf[19] == 'b');
}

/* -------------------------------------------------------------------------
 * Test: buffer boundary (sentinel byte must not be overwritten)
 * ---------------------------------------------------------------------- */
static void test_buffer_canary(void)
{
    static const uint8_t mac[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
    char buf[UUID_LEN + 2];

    buf[UUID_LEN]     = '\0'; /* sentinel at expected NUL position */
    buf[UUID_LEN + 1] = 0x5A; /* canary byte just past the buffer   */

    gen_uuid_v5_mac(buf, mac);

    CHECK("NUL terminator at [36]",    buf[UUID_LEN]     == '\0');
    CHECK("canary byte [37] untouched", buf[UUID_LEN + 1] == 0x5A);
}

/* -------------------------------------------------------------------------
 * Test: determinism (two calls with the same MAC yield identical output)
 * ---------------------------------------------------------------------- */
static void test_determinism(void)
{
    static const uint8_t mac[6] = {0x02, 0x42, 0xac, 0x11, 0x00, 0x02};
    char r1[UUID_LEN + 1], r2[UUID_LEN + 1];

    gen_uuid_v5_mac(r1, mac);
    gen_uuid_v5_mac(r2, mac);
    CHECK("same MAC -> same UUID on repeated call", strcmp(r1, r2) == 0);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void)
{
    printf("=== test_utils: gen_uuid_v5_mac ===\n");
    test_kat_vectors();
    test_output_format();
    test_determinism();
    test_buffer_canary();
    printf("---\n%d/%d passed", g_pass, g_tests);
    if (g_fail)
        printf("  (%d FAILED)", g_fail);
    printf("\n");
    return g_fail ? 1 : 0;
}
