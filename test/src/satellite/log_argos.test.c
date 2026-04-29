#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including log_argos.c ---*/

/* Stub app_filex.h types */
#define __APP_FILEX_H__
typedef unsigned int UINT;
typedef struct { int dummy; } FX_MEDIA;
typedef struct { unsigned long fx_file_current_file_size; } FX_FILE;
#define FX_SUCCESS 0
#define FX_ALREADY_CREATED 0x02

static char s_created_file_name[256];
UINT fx_file_create(FX_MEDIA *m, char *name) { (void)m; strncpy(s_created_file_name, name, sizeof(s_created_file_name) - 1); return FX_SUCCESS; }
UINT fx_file_open(FX_MEDIA *m, FX_FILE *f, char *name, unsigned int mode) { (void)m; (void)f; (void)name; (void)mode; return FX_SUCCESS; }
UINT fx_file_write(FX_FILE *f, void *buf, unsigned long sz) { (void)f; (void)buf; (void)sz; return FX_SUCCESS; }
UINT fx_file_close(FX_FILE *f) { (void)f; return FX_SUCCESS; }
UINT fx_media_flush(FX_MEDIA *m) { (void)m; return FX_SUCCESS; }
#define FX_OPEN_FOR_WRITE 0x01

FX_MEDIA sdio_disk;

/* Stub syslog.h */
#define CETI_SYSLOG_H
typedef struct { const char *ptr; size_t len; } str;
#define str_from_string(s) ((str){ .ptr = (s), .len = sizeof(s) - 1 })
#define CETI_LOG(...)
#define syslog_write(...)

/* Stub error.h */
#define CETI_ERROR_H
typedef uint32_t CetiStatus;
#define CETI_ERROR(subsys, type, code) ((CetiStatus)0)
void error_queue_push(CetiStatus error, void *calling_func) { (void)error; (void)calling_func; }

/* Stub metadata.h */
#define CETI_METADATA_H
typedef enum { DATA_TYPE_ARGOS = 30 } DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) { (void)f; (void)t; (void)fmt; (void)v; }

/* Stub satellite.h — provide RecoveryArgoModulation and guard includes */
#define CETI_SATELLITE_H
#define CETI_CONFIG_H
typedef enum {
    ARGOS_MOD_LDA2,
    ARGOS_MOD_VLDA4,
    ARGOS_MOD_LDK,
    ARGOS_MOD_LDA2L,
} RecoveryArgoModulation;

/* Include unit under test */
#include "satellite/log_argos.c"

#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

static FileSpec argos_spec;

/* ===== Helpers ============================================================ */

static ArgosTxEvent make_event(uint64_t ts, RecoveryArgoModulation type, const char *msg) {
    ArgosTxEvent e = {0};
    e.timestamp_us = ts;
    e.tx_type = type;
    strncpy((char *)e.message, msg, sizeof(e.message) - 1);
    return e;
}

/* ===== Spec-contract tests ================================================ */

void test_spec_filename_matches(void) {
    log_argos_init();
    TEST_ASSERT_EQUAL_STRING("tag_rf.csv", s_created_file_name);
}

void test_spec_version_comment_in_header(void) {
    csv_assert_version_comment(s_argos_tx_log_csv_header, &argos_spec);
}

void test_spec_field_count_matches_header(void) {
    csv_assert_header_field_count(s_argos_tx_log_csv_header, &argos_spec);
}

void test_spec_field_names_match_header(void) {
    csv_assert_header_field_names(s_argos_tx_log_csv_header, &argos_spec);
}

void test_spec_csv_line_field_count(void) {
    ArgosTxEvent e = make_event(1000000, ARGOS_MOD_LDA2, "0123456789ABCDEF");
    uint8_t buf[256];
    size_t len = priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &argos_spec);
}

void test_spec_notes_empty_for_clean_sample(void) {
    ArgosTxEvent e = make_event(100, ARGOS_MOD_LDA2, "AABB");
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &argos_spec);
}

/* ===== CSV structure tests ================================================ */

void test_csv_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_argos_tx_log_csv_header);
}

void test_csv_line_ends_with_newline(void) {
    ArgosTxEvent e = make_event(12345678, ARGOS_MOD_LDK, "DEADBEEF");
    uint8_t buf[256];
    size_t len = priv__event_to_csv_line(e, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_csv_line_no_embedded_newlines(void) {
    ArgosTxEvent e = make_event(500, ARGOS_MOD_VLDA4, "0011");
    uint8_t buf[256];
    size_t len = priv__event_to_csv_line(e, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_csv_line_large_timestamp(void) {
    ArgosTxEvent e = make_event(0xFFFFFFFFFFFFFFFFULL, ARGOS_MOD_LDA2, "FF");
    uint8_t buf[256];
    size_t len = priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &argos_spec);
}

/* ===== Modulation type tests ============================================== */

void test_type_lda2(void) {
    ArgosTxEvent e = make_event(100, ARGOS_MOD_LDA2, "AA");
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((char *)buf, "LDA2"), "LDA2 type not in CSV line");
}

void test_type_vlda2(void) {
    ArgosTxEvent e = make_event(100, ARGOS_MOD_VLDA4, "AA");
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((char *)buf, "VLDA2"), "VLDA2 type not in CSV line");
}

void test_type_ldk(void) {
    ArgosTxEvent e = make_event(100, ARGOS_MOD_LDK, "AA");
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((char *)buf, "LDK"), "LDK type not in CSV line");
}

void test_type_lda2l(void) {
    ArgosTxEvent e = make_event(100, ARGOS_MOD_LDA2L, "AA");
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((char *)buf, "LDA2L"), "LDA2L type not in CSV line");
}

void test_message_appears_in_csv(void) {
    const char *msg = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    ArgosTxEvent e = make_event(100, ARGOS_MOD_LDA2, msg);
    uint8_t buf[256];
    priv__event_to_csv_line(e, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr((char *)buf, msg),
        "Full hex message not found in CSV line");
}

/* ===== Entry point ======================================================== */

int main(void) {
    if (spec_parse("tag_rf.csv", &argos_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for tag_rf.csv from %s\n", SPEC_FILE);
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_spec_filename_matches);
    RUN_TEST(test_spec_version_comment_in_header);

    /* spec contract: structure */
    RUN_TEST(test_spec_field_count_matches_header);
    RUN_TEST(test_spec_field_names_match_header);
    RUN_TEST(test_spec_csv_line_field_count);
    RUN_TEST(test_spec_notes_empty_for_clean_sample);

    /* csv structure */
    RUN_TEST(test_csv_header_ends_with_newline);
    RUN_TEST(test_csv_line_ends_with_newline);
    RUN_TEST(test_csv_line_no_embedded_newlines);
    RUN_TEST(test_csv_line_large_timestamp);

    /* modulation types */
    RUN_TEST(test_type_lda2);
    RUN_TEST(test_type_vlda2);
    RUN_TEST(test_type_ldk);
    RUN_TEST(test_type_lda2l);
    RUN_TEST(test_message_appears_in_csv);

    return UNITY_END();
}
