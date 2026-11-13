/*
 * Unit tests for helpers in utils.c and the analytics description file loader.
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* Pull in the function under test */
#include "../utils.h"
#include "../onvif_simple_server.h"
#include "../analytics_desc.h"

/* The analytics loader matches message topics against the configured events */
service_context_t service_ctx;

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
 * Test: cat_n() never writes past its capacity
 * ---------------------------------------------------------------------- */
static void test_cat_n(void)
{
    char path[] = "/tmp/test_utils_cat_XXXXXX";
    int fd = mkstemp(path);
    const char *tpl = "<a>%X%</a>\n<b>%X%</b>\n";
    char buf[64];
    long size, ret;
    size_t cap;

    if (fd < 0 || write(fd, tpl, strlen(tpl)) != (ssize_t) strlen(tpl)) {
        CHECK("cat_n: template file created", 0);
        return;
    }
    close(fd);

    size = cat(NULL, path, 2, "%X%", "hello");
    CHECK("cat_n: size pass counts the expanded template", size == 24);

    memset(buf, 'Z', sizeof(buf));
    ret = cat_n(buf, size + 1, path, 2, "%X%", "hello");
    CHECK("cat_n: exact fit returns the full size", ret == size);
    CHECK("cat_n: exact fit is NUL terminated", buf[size] == '\0' && strlen(buf) == (size_t) size);
    CHECK("cat_n: exact fit leaves the canary", buf[size + 1] == 'Z');

    for (cap = 1; cap <= (size_t) size; cap++) {
        memset(buf, 'Z', sizeof(buf));
        ret = cat_n(buf, cap, path, 2, "%X%", "hello");
        if (ret != size || strlen(buf) >= cap || buf[cap] != 'Z')
            break;
    }
    CHECK("cat_n: every smaller capacity truncates safely", cap == (size_t) size + 1);

    memset(buf, 'Z', sizeof(buf));
    ret = cat_n(buf, 0, path, 2, "%X%", "hello");
    CHECK("cat_n: zero capacity writes nothing", ret == size && buf[0] == 'Z');

    unlink(path);
}

/* -------------------------------------------------------------------------
 * Test: event item descriptions
 * ---------------------------------------------------------------------- */
static event_t test_events[] = {
    { "tns1:RuleEngine/MyRuleDetector/PeopleDetect", "VideoSourceConfigurationToken", "xsd:string", "VideoSourceToken", "/tmp/a" },
    { "tns1:RuleEngine/MyRuleDetector/PeopleDetect", "Second", "tt:ReferenceToken", "VideoSourceToken", "/tmp/b" },
    { "tns1:RuleEngine/MyRuleDetector/VehicleDetect", "VideoSourceConfigurationToken", NULL, "VideoSourceToken", "/tmp/c" },
    { "tns1:Device/Trigger/Relay", "RelayToken", "tt:ReferenceToken", "RelayOutputToken_0", "/tmp/d" },
    { "tns1:RuleEngine/MyRuleDetector/BadType", "Source", "ai:Token", "VideoSourceToken", "/tmp/e" },
    { "tns1:RuleEngine/MyRuleDetector/NoSource", NULL, NULL, "VideoSourceToken", "/tmp/f" },
    { "tns1:RuleEngine/MyRuleDetector/SameName", "State", "xsd:string", "VideoSourceToken", "/tmp/g" },
};

static void test_event_description(void)
{
    const char *st, *dn, *dt;
    event_t no_topic = { NULL, NULL, NULL, NULL, NULL };

    get_event_description(&test_events[0], &st, &dn, &dt);
    CHECK("event description: configured source type",
          strcmp(st, "xsd:string") == 0 && strcmp(dn, "State") == 0 && strcmp(dt, "xsd:boolean") == 0);
    get_event_description(&test_events[2], &st, &dn, &dt);
    CHECK("event description: default source type", strcmp(st, "tt:ReferenceToken") == 0);
    get_event_description(&test_events[3], &st, &dn, &dt);
    CHECK("event description: relay state",
          strcmp(dn, "LogicalState") == 0 && strcmp(dt, "tt:RelayLogicalState") == 0);
    get_event_description(&no_topic, &st, &dn, &dt);
    CHECK("event description: no topic", strcmp(dn, "State") == 0);
}

/* -------------------------------------------------------------------------
 * Test: XML character and name checks
 * ---------------------------------------------------------------------- */
static void test_xml_checks(void)
{
    CHECK("xml chars: ASCII, tab and newlines", is_xml_chars("a b\tc\r\n<&>"));
    CHECK("xml chars: UTF-8", is_xml_chars("\xc3\xa4\xe2\x82\xac\xf0\x9f\x98\x80"));
    CHECK("xml chars: empty", is_xml_chars(""));
    CHECK("xml chars: control character", !is_xml_chars("a\x01"));
    CHECK("xml chars: bare 0xFF", !is_xml_chars("\xff\xfe"));
    CHECK("xml chars: truncated sequence", !is_xml_chars("a\xc3"));
    CHECK("xml chars: overlong encoding", !is_xml_chars("\xc0\xbc"));
    CHECK("xml chars: surrogate", !is_xml_chars("\xed\xa0\x80"));
    CHECK("xml chars: U+FFFF", !is_xml_chars("\xef\xbf\xbf"));
    CHECK("xml chars: above U+10FFFF", !is_xml_chars("\xf4\x90\x80\x80"));
    CHECK("ncname: valid", is_ncname("_a-b.c9", 7));
    CHECK("ncname: prefix part", is_ncname("tt:Frame", 2));
    CHECK("ncname: leading digit", !is_ncname("9a", 2));
    CHECK("ncname: markup", !is_ncname("a<b", 3));
    CHECK("ncname: empty", !is_ncname("", 0));
}

/* -------------------------------------------------------------------------
 * Test: read_file()
 * ---------------------------------------------------------------------- */
static void test_read_file(void)
{
    char path[] = "/tmp/test_utils_read_XXXXXX";
    char fifo[64];
    int fd = mkstemp(path);
    size_t len = 0;
    char *buf;

    if (fd < 0 || write(fd, "hello", 5) != 5) {
        CHECK("read_file: test file created", 0);
        return;
    }
    close(fd);

    buf = read_file(path, 5, &len);
    CHECK("read_file: exact size", buf != NULL && len == 5 && strcmp(buf, "hello") == 0);
    free(buf);
    CHECK("read_file: too large", read_file(path, 4, &len) == NULL);
    CHECK("read_file: directory", read_file("/tmp", 1024, &len) == NULL);
    CHECK("read_file: missing", read_file("/nonexistent/x", 1024, &len) == NULL);
    snprintf(fifo, sizeof(fifo), "%s.fifo", path);
    if (mkfifo(fifo, 0600) == 0) {
        CHECK("read_file: FIFO does not block", read_file(fifo, 1024, &len) == NULL);
        unlink(fifo);
    }
    unlink(path);
}

/* -------------------------------------------------------------------------
 * Test: analytics description file loader
 * ---------------------------------------------------------------------- */
static int load_desc(analytics_desc_t *d, const char *json)
{
    char path[] = "/tmp/test_utils_desc_XXXXXX";
    int fd = mkstemp(path);
    int ret;

    if (fd < 0 || write(fd, json, strlen(json)) != (ssize_t) strlen(json)) {
        if (fd >= 0) {
            close(fd);
            unlink(path);
        }
        memset(d, 0, sizeof(*d));
        return -99;
    }
    close(fd);
    ret = analytics_desc_load(d, path);
    unlink(path);
    return ret;
}

static const char *desc_full =
    "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"http://example.invalid/a?x=1&y=2\" },"
    "  \"unknown\": 1,"
    "  \"modules\": ["
    "    { \"type\": \"ai:ObjectDetector\", \"name\": \"Detector1\","
    "      \"parameters\": ["
    "        { \"name\": \"Sensitivity\", \"type\": \"xsd:int\", \"value\": \"50\" },"
    "        { \"name\": \"Sensitivity\", \"type\": \"xsd:int\", \"value\": \"60\" },"
    "        { \"name\": \"Mode\", \"type\": \"ai:Mode\", \"value\": \"fast & <safe>\" },"
    "        { \"name\": \"Bad%\", \"type\": \"xsd:int\", \"value\": \"1\" },"
    "        { \"name\": \"P2\", \"type\": \"foo:int\", \"value\": \"1\" },"
    "        { \"name\": \"P3\", \"type\": \"xsd:int\", \"value\": \"50%\" },"
    "        { \"name\": \"P4\", \"type\": \"xsd:int\" }"
    "      ],"
    "      \"messages\": ["
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/PeopleDetect\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/PeopleDetect\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/VehicleDetect\" },"
    "        { \"topic\": \"tns1:Device/Trigger/Relay\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/Unknown\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/BadType\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/NoSource\" },"
    "        { \"topic\": \"tns1:RuleEngine/MyRuleDetector/SameName\" },"
    "        { \"topic\": \"ai:Other/Topic\" },"
    "        { \"no_topic\": 1 }"
    "      ],"
    "      \"sample_frame_file\": \"/tmp/frame.xml\" },"
    "    { \"type\": \"ai:ObjectDetector\", \"name\": \"Detector2\" },"
    "    { \"type\": \"ai:ObjectDetector\", \"name\": \"Detector1\" },"
    "    { \"type\": \"tt:CellMotionEngine\", \"name\": \"Cell\" },"
    "    { \"type\": \"ai:Bad Type\", \"name\": \"X\" },"
    "    { \"type\": \"ai:Face\", \"name\": \"with space\" },"
    "    { \"type\": \"ai:Face\", \"name\": \"Face1\", \"rules\": [] },"
    "    { \"type\": \"ai:Face\", \"name\": \"A\xff\xc0\" },"
    "    { \"type\": \"ai:Face\", \"name\": \"Face2\","
    "      \"parameters\": [ { \"name\": \"P\", \"type\": \"xsd:string\", \"value\": \"v\xfe\" },"
    "                      { \"name\": \"Q\", \"type\": \"xsd:string\", \"value\": \"\xc3\xa4 ok\" } ] },"
    "    \"not an object\""
    "  ] }";

static void test_analytics_desc(void)
{
    analytics_desc_t d;
    analytics_module_t *m;
    char json[4096];
    int i, ret;

    service_ctx.events = test_events;
    service_ctx.events_num = sizeof(test_events) / sizeof(test_events[0]);

    ret = load_desc(&d, desc_full);
    CHECK("analytics desc: valid file loads", ret == 0);
    CHECK("analytics desc: namespace",
          d.ns_prefix != NULL && strcmp(d.ns_prefix, "ai") == 0 &&
          strcmp(d.ns_uri, "http://example.invalid/a?x=1&y=2") == 0);
    CHECK("analytics desc: invalid and duplicate modules skipped", d.modules_num == 4);
    if (d.modules_num == 4) {
        m = &d.modules[0];
        CHECK("analytics desc: module name and type",
              strcmp(m->config.name, "Detector1") == 0 && strcmp(m->config.type, "ai:ObjectDetector") == 0);
        CHECK("analytics desc: invalid and duplicate parameters skipped",
              m->config.params_num == 2 &&
              strcmp(m->config.params[0].value, "50") == 0 &&
              strcmp(m->config.params[1].name, "Mode") == 0 &&
              strcmp(m->config.params[1].value, "fast & <safe>") == 0);
        CHECK("analytics desc: messages match the first conf event of a topic",
              m->messages_num == 3 && m->messages[0] == 0 && m->messages[1] == 2 && m->messages[2] == 3);
        CHECK("analytics desc: sample frame file",
              m->sample_frame_file != NULL && strcmp(m->sample_frame_file, "/tmp/frame.xml") == 0);
        CHECK("analytics desc: second instance of a type",
              strcmp(d.modules[1].config.name, "Detector2") == 0 &&
              d.modules[1].config.params_num == 0 && d.modules[1].messages_num == 0 &&
              d.modules[1].sample_frame_file == NULL);
        CHECK("analytics desc: unknown keys ignored", strcmp(d.modules[2].config.name, "Face1") == 0);
        CHECK("analytics desc: invalid UTF-8 skipped",
              strcmp(d.modules[3].config.name, "Face2") == 0 && d.modules[3].config.params_num == 1 &&
              strcmp(d.modules[3].config.params[0].name, "Q") == 0);
    }
    CHECK("analytics desc: no rules yet", d.rules_num == 0);
    analytics_desc_free(&d);
    CHECK("analytics desc: free resets", d.root == NULL && d.modules_num == 0);

    ret = analytics_desc_load(&d, "/nonexistent/analytics.json");
    CHECK("analytics desc: missing file", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);
    ret = analytics_desc_load(&d, NULL);
    CHECK("analytics desc: no file configured", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);

    ret = load_desc(&d, "[ 1, 2 ]");
    CHECK("analytics desc: root is not an object", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"modules\": [ { \"type\": \"ai:A\", \"name\": \"A\" } ] }");
    CHECK("analytics desc: no namespace", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"tt\", \"uri\": \"http://example.invalid/a\" } }");
    CHECK("analytics desc: reserved prefix", ret == -1);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"xmlai\", \"uri\": \"http://example.invalid/a\" } }");
    CHECK("analytics desc: xml prefix", ret == -1);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"http://example.invalid/a b\" } }");
    CHECK("analytics desc: uri with space", ret == -1);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"http://example.invalid/%41\" } }");
    CHECK("analytics desc: uri with percent", ret == -1);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"http://www.w3.org/2000/xmlns/\" } }");
    CHECK("analytics desc: reserved uri", ret == -1);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"http://example.invalid/a\" } }");
    CHECK("analytics desc: no modules", ret == 0 && d.modules_num == 0);
    analytics_desc_free(&d);
    ret = load_desc(&d, "{ broken");
    CHECK("analytics desc: broken JSON", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);
    ret = analytics_desc_load(&d, "/tmp");
    CHECK("analytics desc: not a regular file", ret == -1 && d.modules_num == 0);
    analytics_desc_free(&d);

    snprintf(json, sizeof(json), "{ \"namespace\": { \"prefix\": \"ai\", \"uri\": \"urn:a\" }, \"modules\": [");
    for (i = 0; i < MAX_ANALYTICS_MODULES + 2; i++) {
        snprintf(json + strlen(json), sizeof(json) - strlen(json),
                 "%s{ \"type\": \"ai:T\", \"name\": \"M%d\" }", i ? "," : "", i);
    }
    snprintf(json + strlen(json), sizeof(json) - strlen(json), "] }");
    ret = load_desc(&d, json);
    CHECK("analytics desc: module limit", ret == 0 && d.modules_num == MAX_ANALYTICS_MODULES);
    analytics_desc_free(&d);

    service_ctx.events = NULL;
    service_ctx.events_num = 0;
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
    test_cat_n();
    printf("=== test_utils: analytics description ===\n");
    test_xml_checks();
    test_read_file();
    test_event_description();
    test_analytics_desc();
    printf("---\n%d/%d passed", g_pass, g_tests);
    if (g_fail)
        printf("  (%d FAILED)", g_fail);
    printf("\n");
    return g_fail ? 1 : 0;
}
