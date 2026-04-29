#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including log_pressure.c ---*/

/* Stub app_filex.h types */
#define __APP_FILEX_H__
typedef unsigned int UINT;
typedef struct { int dummy; } FX_MEDIA;
typedef struct { int dummy; } FX_FILE;
#define FX_SUCCESS 0
#define FX_ALREADY_CREATED 0x02

UINT fx_file_create(FX_MEDIA *m, char *name) { (void)m; (void)name; return FX_SUCCESS; }
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

/* Stub HAL */
#define HAL_PWR_DisableSleepOnExit()

/* Provide buffer_writer (stub) */
#define CETI_BUFFER_WRITER_H__
typedef struct {
    uint8_t *buffer;
    size_t cursor;
    size_t threshold;
    size_t capacity;
    FX_FILE fp;
} BufferWriter;

UINT buffer_writer_open(BufferWriter *w, char *filename) { (void)w; (void)filename; return FX_SUCCESS; }
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) { (void)w; (void)p_bytes; (void)len; return FX_SUCCESS; }
UINT buffer_writer_close(BufferWriter *w) { (void)w; return FX_SUCCESS; }
UINT buffer_writer_flush(BufferWriter *w) { (void)w; return FX_SUCCESS; }

/*--- Include the source under test ---*/
#include "pressure/log_pressure.c"

/* ===== Spec reader and CSV assertion library ===== */
#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

/* Parsed spec (loaded once in main before tests run) */
static FileSpec pressure_spec;

/* ===========================================================================
 * Spec-contract tests
 * ========================================================================= */

void test_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("data_pressure.csv", PRESSURE_FILENAME);
}

void test_spec_format_version_matches(void) {
    TEST_ASSERT_EQUAL_INT(pressure_spec.format_version, PRESSURE_CSV_VERSION);
}

void test_spec_field_count_matches_header(void) {
    csv_assert_header_field_count(log_pressure_csv_header, &pressure_spec);
}

void test_spec_field_names_match_header(void) {
    csv_assert_header_field_names(log_pressure_csv_header, &pressure_spec);
}

void test_spec_version_comment_in_header(void) {
    csv_assert_version_comment(log_pressure_csv_header, &pressure_spec);
}

void test_spec_csv_line_field_count(void) {
    CetiPressureSample sample = {
        .timestamp_us = 1000000,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &pressure_spec);
}

void test_spec_pressure_format_precision(void) {
    uint16_t raw = (uint16_t)(16384 + (10.0 / 200.0) * 32768.0);
    double value = acq_pressure_raw_to_pressure_bar(raw);

    CetiPressureSample sample = { .timestamp_us = 0, .pressure = raw, .temperature = 0x0180 };
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    csv_assert_field_format((char *)csv_buf, &pressure_spec, "Pressure [bar]", value);
}

void test_spec_temperature_format_precision(void) {
    uint16_t raw_temp = 0x4000;
    double value = acq_pressure_raw_to_temperature_c(raw_temp);

    CetiPressureSample sample = { .timestamp_us = 0, .pressure = 16384, .temperature = raw_temp };
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    csv_assert_field_format((char *)csv_buf, &pressure_spec, "Temperature [C]", value);
}

void test_spec_pressure_range_min(void) {
    double p = acq_pressure_raw_to_pressure_bar(16384);
    csv_assert_field_in_range(&pressure_spec, "Pressure [bar]", p, 0.001);
}

void test_spec_pressure_range_max(void) {
    double p = acq_pressure_raw_to_pressure_bar(16384 + 32768);
    csv_assert_field_in_range(&pressure_spec, "Pressure [bar]", p, 0.001);
}

void test_spec_overflow_note_matches(void) {
    CetiPressureSample sample = {
        .timestamp_us = 100,
        .status = CSV_OVERFLOW_FLAG,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    csv_assert_note_present((char *)csv_buf, &pressure_spec, "OVERFLOW");
}

void test_spec_clean_sample_has_empty_notes(void) {
    CetiPressureSample sample = {
        .timestamp_us = 100,
        .status = 0,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    csv_assert_notes_empty((char *)csv_buf, &pressure_spec);
}

/* ===========================================================================
 * CSV structure tests
 * ========================================================================= */

void test_csv_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(log_pressure_csv_header);
}

void test_csv_line_ends_with_newline(void) {
    CetiPressureSample sample = {
        .timestamp_us = 12345678,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_csv_line_no_embedded_newlines(void) {
    CetiPressureSample sample = {
        .timestamp_us = 500,
        .pressure = 20000,
        .temperature = 0x0800,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_csv_line_large_timestamp(void) {
    CetiPressureSample sample = {
        .timestamp_us = 0xFFFFFFFFFFFFFFFFULL,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &pressure_spec);
}

int main(void) {
    if (spec_parse("data_pressure.csv", &pressure_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_pressure.csv from %s\n", SPEC_FILE);
        return 1;
    }

    UNITY_BEGIN();

    /* spec contract: structure */
    RUN_TEST(test_spec_filename_matches);
    RUN_TEST(test_spec_format_version_matches);
    RUN_TEST(test_spec_version_comment_in_header);
    RUN_TEST(test_spec_field_count_matches_header);
    RUN_TEST(test_spec_field_names_match_header);
    RUN_TEST(test_spec_csv_line_field_count);

    /* spec contract: value formats */
    RUN_TEST(test_spec_pressure_format_precision);
    RUN_TEST(test_spec_temperature_format_precision);

    /* spec contract: value ranges */
    RUN_TEST(test_spec_pressure_range_min);
    RUN_TEST(test_spec_pressure_range_max);

    /* spec contract: notes vocabulary */
    RUN_TEST(test_spec_overflow_note_matches);
    RUN_TEST(test_spec_clean_sample_has_empty_notes);

    /* csv structure */
    RUN_TEST(test_csv_header_ends_with_newline);
    RUN_TEST(test_csv_line_ends_with_newline);
    RUN_TEST(test_csv_line_no_embedded_newlines);
    RUN_TEST(test_csv_line_large_timestamp);

    return UNITY_END();
}
