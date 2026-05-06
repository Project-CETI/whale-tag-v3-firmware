#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including log_pressure.c ---*/

/* Stub app_filex.h types */
#define __APP_FILEX_H__
typedef unsigned int UINT;
typedef struct {
    int dummy;
} FX_MEDIA;
typedef struct {
    int dummy;
} FX_FILE;
#define FX_SUCCESS 0
#define FX_ALREADY_CREATED 0x02

UINT fx_file_create(FX_MEDIA *m, char *name) {
    (void)m;
    (void)name;
    return FX_SUCCESS;
}
UINT fx_file_open(FX_MEDIA *m, FX_FILE *f, char *name, unsigned int mode) {
    (void)m;
    (void)f;
    (void)name;
    (void)mode;
    return FX_SUCCESS;
}
UINT fx_file_write(FX_FILE *f, void *buf, unsigned long sz) {
    (void)f;
    (void)buf;
    (void)sz;
    return FX_SUCCESS;
}
UINT fx_file_close(FX_FILE *f) {
    (void)f;
    return FX_SUCCESS;
}
UINT fx_media_flush(FX_MEDIA *m) {
    (void)m;
    return FX_SUCCESS;
}
#define FX_OPEN_FOR_WRITE 0x01

FX_MEDIA sdio_disk;

/* Stub syslog.h */
#define CETI_SYSLOG_H
typedef struct {
    const char *ptr;
    size_t len;
} str;
#define str_from_string(s) ((str){.ptr = (s), .len = sizeof(s) - 1})
#define CETI_LOG(...)
#define syslog_write(...)

/* Stub error.h */
#define CETI_ERROR_H
typedef uint32_t CetiStatus;
#define CETI_ERROR(subsys, type, code) ((CetiStatus)0)
void error_queue_push(CetiStatus error, void *calling_func) {
    (void)error;
    (void)calling_func;
}

/* Stub metadata.h */
#define CETI_METADATA_H
typedef enum { DATA_TYPE_PRESSURE = 13 } DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) {
    (void)f;
    (void)t;
    (void)fmt;
    (void)v;
}

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

UINT buffer_writer_open(BufferWriter *w, char *filename) {
    (void)w;
    (void)filename;
    return FX_SUCCESS;
}
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) {
    (void)w;
    (void)p_bytes;
    (void)len;
    return FX_SUCCESS;
}
UINT buffer_writer_close(BufferWriter *w) {
    (void)w;
    return FX_SUCCESS;
}
UINT buffer_writer_flush(BufferWriter *w) {
    (void)w;
    return FX_SUCCESS;
}

/*--- Include the source under test ---*/
#include "pressure/log_pressure.c"

/* ===== Spec reader — loads field definitions from tag_data_formats.yaml === */
#include "../../spec_reader.h"

void setUp(void) {}
void tearDown(void) {}

/* ===== Helpers ===== */
static int count_char(const char *s, char c) {
    int n = 0;
    while (*s) {
        if (*s == c)
            n++;
        s++;
    }
    return n;
}

/// @brief split a CSV header/line into trimmed fields (modifies input in-place)
static int split_csv_fields(char *line, char *fields[], int max_fields) {
    int n = 0;
    char *p = line;
    while (n < max_fields && *p) {
        while (*p == ' ')
            p++;
        fields[n++] = p;
        char *comma = strchr(p, ',');
        if (!comma)
            break;
        *comma = '\0';
        p = comma + 1;
    }
    if (n > 0) {
        char *last = fields[n - 1];
        size_t len = strlen(last);
        while (len > 0 && (last[len - 1] == '\n' || last[len - 1] == ' '))
            last[--len] = '\0';
    }
    return n;
}

/// @brief find a SpecField by name, returns NULL if not found
static const SpecField *find_spec_field(const FileSpec *spec, const char *name) {
    for (int i = 0; i < spec->num_fields; i++) {
        if (strcmp(spec->fields[i].name, name) == 0)
            return &spec->fields[i];
    }
    return NULL;
}

/// @brief return pointer to the first line in the header that isn't a comment
/// (i.e. skip lines starting with comment_prefix)
static const char *skip_comment_lines(const char *header, const char *prefix) {
    size_t plen = strlen(prefix);
    const char *p = header;
    while (strncmp(p, prefix, plen) == 0) {
        const char *nl = strchr(p, '\n');
        if (!nl)
            return p; /* no newline — entire header is a comment? */
        p = nl + 1;
    }
    return p;
}

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
    const char *columns = skip_comment_lines(log_pressure_csv_header, pressure_spec.comment_prefix);
    int header_commas = count_char(columns, ',');
    TEST_ASSERT_EQUAL_INT(pressure_spec.num_fields, header_commas + 1);
}

void test_spec_field_names_match_header(void) {
    const char *columns = skip_comment_lines(log_pressure_csv_header, pressure_spec.comment_prefix);
    char header_copy[512];
    strncpy(header_copy, columns, sizeof(header_copy));
    header_copy[sizeof(header_copy) - 1] = '\0';

    char *fields[SPEC_MAX_FIELDS];
    int n = split_csv_fields(header_copy, fields, SPEC_MAX_FIELDS);

    TEST_ASSERT_EQUAL_INT_MESSAGE(pressure_spec.num_fields, n,
                                  "header field count does not match spec");

    for (int i = 0; i < n && i < pressure_spec.num_fields; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "field %d name mismatch", i);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(pressure_spec.fields[i].name, fields[i], msg);
    }
}

void test_spec_version_comment_in_header(void) {
    /* header should start with "# version: <N>\n" matching spec */
    char expected[64];
    snprintf(expected, sizeof(expected), "%s version: %d\n",
             pressure_spec.comment_prefix, pressure_spec.format_version);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0,
                                  strncmp(log_pressure_csv_header, expected, strlen(expected)),
                                  "header version comment does not match spec");
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
    TEST_ASSERT_EQUAL_INT(pressure_spec.num_fields, count_char((char *)buf, ',') + 1);
}

void test_spec_pressure_format_precision(void) {
    const SpecField *pf = find_spec_field(&pressure_spec, "Pressure [bar]");
    TEST_ASSERT_NOT_NULL_MESSAGE(pf, "Pressure field not found in spec");
    TEST_ASSERT_TRUE_MESSAGE(strlen(pf->format) > 0, "Pressure field has no format in spec");

    uint16_t raw = (uint16_t)(16384 + (10.0 / 200.0) * 32768.0);
    double value = acq_pressure_raw_to_pressure_bar(raw);

    char expected[32];
    snprintf(expected, sizeof(expected), pf->format, value);

    CetiPressureSample sample = {.timestamp_us = 0, .pressure = raw, .temperature = 0x0180};
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr((char *)csv_buf, expected),
        "Pressure value in CSV does not match spec format");
}

void test_spec_temperature_format_precision(void) {
    const SpecField *tf = find_spec_field(&pressure_spec, "Temperature [C]");
    TEST_ASSERT_NOT_NULL_MESSAGE(tf, "Temperature field not found in spec");
    TEST_ASSERT_TRUE_MESSAGE(strlen(tf->format) > 0, "Temperature field has no format in spec");

    uint16_t raw_temp = 0x4000;
    double value = acq_pressure_raw_to_temperature_c(raw_temp);

    char expected[32];
    snprintf(expected, sizeof(expected), tf->format, value);

    CetiPressureSample sample = {.timestamp_us = 0, .pressure = 16384, .temperature = raw_temp};
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));

    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr((char *)csv_buf, expected),
        "Temperature value in CSV does not match spec format");
}

void test_spec_pressure_range_min(void) {
    const SpecField *pf = find_spec_field(&pressure_spec, "Pressure [bar]");
    TEST_ASSERT_NOT_NULL(pf);
    TEST_ASSERT_TRUE(pf->has_range);
    double p = acq_pressure_raw_to_pressure_bar(16384);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, pf->range_min, p);
}

void test_spec_pressure_range_max(void) {
    const SpecField *pf = find_spec_field(&pressure_spec, "Pressure [bar]");
    TEST_ASSERT_NOT_NULL(pf);
    TEST_ASSERT_TRUE(pf->has_range);
    double p = acq_pressure_raw_to_pressure_bar(16384 + 32768);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, pf->range_max, p);
}

void test_spec_overflow_note_matches(void) {
    const SpecField *nf = find_spec_field(&pressure_spec, "Notes");
    TEST_ASSERT_NOT_NULL_MESSAGE(nf, "Notes field not found in spec");

    int found = 0;
    for (int i = 0; i < nf->num_possible_values; i++) {
        if (strcmp(nf->possible_values[i], "OVERFLOW") == 0) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "OVERFLOW not listed as a possible_value in spec");

    CetiPressureSample sample = {
        .timestamp_us = 100,
        .status = CSV_OVERFLOW_FLAG,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t csv_buf[256];
    priv__pressure_sample_to_csv_line(&sample, csv_buf, sizeof(csv_buf));
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr((char *)csv_buf, "OVERFLOW"),
        "C code does not write OVERFLOW when flag is set");
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

    char copy[256];
    strncpy(copy, (char *)csv_buf, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';
    char *fields[SPEC_MAX_FIELDS];
    int n = split_csv_fields(copy, fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_TRUE(n >= 2);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("", fields[1],
                                     "Notes field should be empty for a clean sample");
}

/* ===========================================================================
 * CSV structure tests
 * ========================================================================= */

void test_csv_header_ends_with_newline(void) {
    size_t len = strlen(log_pressure_csv_header);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_CHAR('\n', log_pressure_csv_header[len - 1]);
}

void test_csv_line_ends_with_newline(void) {
    CetiPressureSample sample = {
        .timestamp_us = 12345678,
        .pressure = 16384,
        .temperature = 0x0180,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[len - 1]);
}

void test_csv_line_no_embedded_newlines(void) {
    CetiPressureSample sample = {
        .timestamp_us = 500,
        .pressure = 20000,
        .temperature = 0x0800,
    };
    uint8_t buf[256];
    size_t len = priv__pressure_sample_to_csv_line(&sample, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(1, count_char((char *)buf, '\n'));
    TEST_ASSERT_EQUAL_CHAR('\n', buf[len - 1]);
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
    TEST_ASSERT_EQUAL_INT(pressure_spec.num_fields - 1, count_char((char *)buf, ','));
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
