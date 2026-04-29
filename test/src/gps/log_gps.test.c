#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including log_gps.c ---*/

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
typedef enum { DATA_TYPE_PRESSURE = 13 } DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) { (void)f; (void)t; (void)fmt; (void)v; }

/* Stub buffer_writer */
#include "util/buffer_writer.h"
UINT buffer_writer_open(BufferWriter *w, char *filename) { (void)w; (void)filename; return 0; }
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) { (void)w; (void)p_bytes; (void)len; return 0; }
UINT buffer_writer_close(BufferWriter *w) { (void)w; return 0; }
UINT buffer_writer_flush(BufferWriter *w) { (void)w; return 0; }

void HAL_PWR_DisableSleepOnExit(void) {;}

/* Include unit under test */
#include "gps/log_gps.c"

#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

static FileSpec gps_spec;

/* ===== Helpers ============================================================ */

static GpsSentence make_gps_sentence(uint64_t ts, const char *nmea) {
    GpsSentence s = {0};
    s.timestamp_us = ts;
    strncpy((char *)s.msg, nmea, GPS_BUFFER_SIZE - 1);
    s.msg_len = strlen(nmea);
    return s;
}

/* ===== Spec-contract tests ================================================ */

void test_spec_filename_matches(void) {
    log_gps_init();
    TEST_ASSERT_EQUAL_STRING("data_gps.csv", s_created_file_name);
}

void test_spec_field_count_matches_header(void) {
    csv_assert_header_field_count(s_csv_header, &gps_spec);
}

void test_spec_field_names_match_header(void) {
    csv_assert_header_field_names(s_csv_header, &gps_spec);
}

void test_spec_csv_line_field_count(void) {
    GpsSentence s = make_gps_sentence(1000000, "$GNRMC,123456.00,A,4807.038,N,01131.000,E,0.0,0.0,230525,,,A*68");
    uint8_t buf[256];
    size_t len = priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &gps_spec);
}

void test_spec_uses_semicolon_separator(void) {
    TEST_ASSERT_EQUAL_STRING_MESSAGE(";", gps_spec.separator,
        "GPS spec should use semicolon separator");
}

void test_spec_notes_empty_for_clean_sample(void) {
    GpsSentence s = make_gps_sentence(100, "$GNRMC,test*00");
    uint8_t buf[256];
    priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &gps_spec);
}

/* ===== CSV structure tests ================================================ */

void test_csv_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_csv_header);
}

void test_csv_line_ends_with_newline(void) {
    GpsSentence s = make_gps_sentence(12345678, "$GNGGA,test*00");
    uint8_t buf[256];
    size_t len = priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_csv_line_no_embedded_newlines(void) {
    GpsSentence s = make_gps_sentence(500, "$GNRMC,test*00");
    uint8_t buf[256];
    size_t len = priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_csv_line_large_timestamp(void) {
    GpsSentence s = make_gps_sentence(0xFFFFFFFFFFFFFFFFULL, "$GNRMC,test*00");
    uint8_t buf[256];
    size_t len = priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &gps_spec);
}

/* ===== Value tests ======================================================== */

void test_nmea_sentence_appears_in_csv(void) {
    const char *nmea = "$GNRMC,123456.00,A,4807.038,N,01131.000,E,0.0,0.0,230525,,,A*68";
    GpsSentence s = make_gps_sentence(100, nmea);
    uint8_t buf[256];
    priv__sentence_to_csv_line(&s, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr((char *)buf, nmea),
        "NMEA sentence not found in CSV line");
}

/* ===== Entry point ======================================================== */

int main(void) {
    if (spec_parse("data_gps.csv", &gps_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_gps.csv from %s\n", SPEC_FILE);
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_spec_filename_matches);
    RUN_TEST(test_spec_uses_semicolon_separator);

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

    /* value tests */
    RUN_TEST(test_nmea_sentence_appears_in_csv);

    return UNITY_END();
}
