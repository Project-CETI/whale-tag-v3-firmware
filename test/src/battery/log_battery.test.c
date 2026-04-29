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

static char s_created_file_name[256];
UINT fx_file_create(FX_MEDIA *m, char *name) { (void)m; strncpy(s_created_file_name, name, sizeof(s_created_file_name) -1); return FX_SUCCESS; }
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
typedef enum { DATA_TYPE_BMS = 3 } DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) { (void)f; (void)t; (void)fmt; (void)v; }

/* Stub buffer_writer*/
#include "util/buffer_writer.h"
UINT buffer_writer_open(BufferWriter *w, char * filename) {(void)w; (void)filename; return 0;}
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) {(void)w; (void)p_bytes; (void)len; return 0;}
UINT buffer_writer_close(BufferWriter *w) {(void)w; return 0;}
UINT buffer_writer_flush(BufferWriter *w) {(void)w; return 0;}


void HAL_PWR_DisableSleepOnExit(void) {;}

// Include Unit under test
#include "battery/log_battery.c"


#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

static FileSpec bms_csv_spec;

void test_spec_filename_matches(void) {
    log_battery_init();
    TEST_ASSERT_EQUAL_STRING(s_created_file_name, "data_battery.csv");
}

void test_spec_field_count_matches_header(void) {
    csv_assert_header_field_count(log_battery_csv_header, &bms_csv_spec);
}

void test_spec_field_names_match_header(void) {
    csv_assert_header_field_names(log_battery_csv_header, &bms_csv_spec);
}

void test_spec_version_comment_in_header(void) {
    csv_assert_version_comment(log_battery_csv_header, &bms_csv_spec);
}

void test_spec_csv_line_field_count(void) {
    CetiBatterySample sample = {};
    uint8_t buf[256];
    size_t len = priv__sample_to_csv(&sample, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &bms_csv_spec);
}

void test_csv_line_ends_with_newline(void) {
    CetiBatterySample sample = {};
    uint8_t buf[256];
    size_t len = priv__sample_to_csv(&sample, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_csv_line_no_embedded_newlines(void) {
    CetiBatterySample sample = {};
    uint8_t buf[256];
    size_t len = priv__sample_to_csv(&sample, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_priv__status_to_str(void) {
    uint16_t input[] = {
        0, 
        0xFFFF, 
        0xAAAA, 
        0x5555
    };
    char * expected[] = {
        "", 
        "PA | POR | Imn | Imx | Vmn | Vmx | Tmn | Tmx | Smn | Smx",
        "PA | POR | Tmn | Tmx",
        "Imn | Imx | Vmn | Vmx | Smn | Smx",
    };

    for (int i = 0; i < sizeof(input)/sizeof(*input); i++) {
        const char output[256];
        strncpy(output, priv__status_to_str(input[i]), sizeof(output));
        TEST_ASSERT_EQUAL_STRING(expected[i], output);
    }
}

void test_priv__protAlrt_to_str(void) {
    uint16_t input[] = {
        0, 
        0xFFFF, 
        0xAAAA, 
        0x5555
    };
    char * expected[] = {
        "", 
        "ChgWDT | TooHotC | Full | TooColdC | OVP | OCCP | Qovflw | PrepF | Imbalance | PermFail | DieHot | TooHotD | UVP | ODCP | ResDFault | LDet",
        "ChgWDT | Full | OVP | Qovflw | Imbalance | DieHot | UVP | ResDFault",
        "TooHotC | TooColdC | OCCP | PrepF | PermFail | TooHotD | ODCP | LDet",
    };

    for (int i = 0; i < sizeof(input)/sizeof(*input); i++) {
        const char output[256];
        strncpy(output, priv__protAlrt_to_str(input[i]), sizeof(output));
        TEST_ASSERT_EQUAL_STRING(expected[i], output);
    }
}

int main(void) {
    if (spec_parse("data_battery.csv", &bms_csv_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_battery.csv from %s\n", SPEC_FILE);
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_spec_filename_matches);

    /* spec contract: structure */
    RUN_TEST(test_spec_field_count_matches_header);
    RUN_TEST(test_spec_field_names_match_header);
    RUN_TEST(test_spec_version_comment_in_header);

    RUN_TEST(test_spec_csv_line_field_count);
    RUN_TEST(test_csv_line_ends_with_newline);
    RUN_TEST(test_csv_line_no_embedded_newlines);

    RUN_TEST(test_priv__status_to_str);
    RUN_TEST(test_priv__protAlrt_to_str);

    return UNITY_END();
}
