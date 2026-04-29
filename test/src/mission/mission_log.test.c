#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including mission_log.c ---*/

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
typedef enum { DATA_TYPE_MISSION = 20 } DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) { (void)f; (void)t; (void)fmt; (void)v; }

/* Stub timing.h */
#define CETI_TIMING_H
static uint64_t s_fake_epoch_us = 1672531200000000ULL;
uint64_t rtc_get_epoch_us(void) { return s_fake_epoch_us; }

/* Provide mission.h types (guard the real include) */
#define CETI_WHALE_TAG_MISSION_H
typedef enum {
    MISSION_STATE_MISSION_START,
    MISSION_STATE_RECORD_SURFACE,
    MISSION_STATE_RECORD_FLOATING,
    MISSION_STATE_RECORD_DIVE,
    MISSION_STATE_BURN,
    MISSION_STATE_LOW_POWER_BURN,
    MISSION_STATE_RETRIEVE,
    MISSION_STATE_LOW_POWER_RETRIEVE,
    MISSION_STATE_PREDEPLOYMENT,
    MISSION_STATE_ERROR,
} MissionState;

typedef enum {
    MISSION_TRANSITION_NONE,
    MISSION_TRANSITION_START,
    MISSION_TRANSITION_LOW_PRESSURE,
    MISSION_TRANSITION_HIGH_PRESSURE,
    MISSION_TRANSITION_FLOAT_DETECTED,
    MISSION_TRANSITION_FLOAT_ENDED,
    MISSION_TRANSITION_BATTERY_ERRORS,
    MISSION_TRANSITION_LOW_VOLTAGE,
    MISSION_TRANSITION_TIMER,
    MISSION_TRANSITION_TIME_OF_DAY,
    MISSION_TRANSITION_UNKNOWN,
} MissionTransitionCause;

/* Include unit under test */
#include "mission/mission_log.c"

#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

static FileSpec mission_spec;

/* ===== Helpers ============================================================ */

/* Capture the CSV line that mission_log_state_transition would write.
   We can't easily capture fx_file_write output, so we call snprintf
   directly the same way the source does. */
static size_t format_transition_line(char *buf, size_t buf_len,
                                     uint64_t ts,
                                     MissionState from,
                                     MissionState to,
                                     MissionTransitionCause cause) {
    return snprintf(buf, buf_len, "%lld, %s, %s, %s\n",
                    (long long)ts,
                    MissionStateNames[from],
                    MissionStateNames[to],
                    MissionTransitionCauseNames[cause]);
}

/* ===== Spec-contract tests ================================================ */

void test_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("tag_mission.csv", MISSION_LOG_CSV_FILENAME);
}

void test_spec_field_count_matches_header(void) {
    csv_assert_header_field_count(MISSION_LOG_CSV_HEADER, &mission_spec);
}

void test_spec_field_names_match_header(void) {
    csv_assert_header_field_names(MISSION_LOG_CSV_HEADER, &mission_spec);
}

void test_spec_csv_line_field_count(void) {
    char buf[256];
    size_t len = format_transition_line(buf, sizeof(buf), 1000000,
        MISSION_STATE_MISSION_START, MISSION_STATE_RECORD_SURFACE, MISSION_TRANSITION_START);
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count(buf, len, &mission_spec);
}

/* ===== CSV structure tests ================================================ */

void test_csv_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(MISSION_LOG_CSV_HEADER);
}

void test_csv_line_ends_with_newline(void) {
    char buf[256];
    size_t len = format_transition_line(buf, sizeof(buf), 100,
        MISSION_STATE_RECORD_SURFACE, MISSION_STATE_RECORD_DIVE, MISSION_TRANSITION_HIGH_PRESSURE);
    csv_assert_line_ends_with_newline(buf, len);
}

void test_csv_line_no_embedded_newlines(void) {
    char buf[256];
    size_t len = format_transition_line(buf, sizeof(buf), 100,
        MISSION_STATE_BURN, MISSION_STATE_RETRIEVE, MISSION_TRANSITION_TIMER);
    csv_assert_line_no_embedded_newlines(buf, len);
}

/* ===== State name tests =================================================== */

/* Verify every MissionState value in the data dictionary has a matching
   entry in MissionStateNames[] */
void test_all_spec_states_have_names(void) {
    const SpecField *from_field = csv_find_spec_field(&mission_spec, "From State");
    TEST_ASSERT_NOT_NULL_MESSAGE(from_field, "From State field not in spec");

    for (int i = 0; i < from_field->num_possible_values; i++) {
        const char *spec_val = from_field->possible_values[i];
        if (strlen(spec_val) == 0) continue;

        int found = 0;
        for (int j = MISSION_STATE_MISSION_START; j <= MISSION_STATE_ERROR; j++) {
            if (MissionStateNames[j] && strcmp(MissionStateNames[j], spec_val) == 0) {
                found = 1;
                break;
            }
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "Spec state '%s' not found in MissionStateNames[]", spec_val);
        TEST_ASSERT_TRUE_MESSAGE(found, msg);
    }
}

/* Verify every MissionTransitionCause value in the data dictionary has
   a matching entry in MissionTransitionCauseNames[] */
void test_all_spec_causes_have_names(void) {
    const SpecField *cause_field = csv_find_spec_field(&mission_spec, "Cause");
    TEST_ASSERT_NOT_NULL_MESSAGE(cause_field, "Cause field not in spec");

    for (int i = 0; i < cause_field->num_possible_values; i++) {
        const char *spec_val = cause_field->possible_values[i];
        if (strlen(spec_val) == 0) continue;

        int found = 0;
        for (int j = MISSION_TRANSITION_NONE; j <= MISSION_TRANSITION_UNKNOWN; j++) {
            if (MissionTransitionCauseNames[j] && strcmp(MissionTransitionCauseNames[j], spec_val) == 0) {
                found = 1;
                break;
            }
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "Spec cause '%s' not found in MissionTransitionCauseNames[]", spec_val);
        TEST_ASSERT_TRUE_MESSAGE(found, msg);
    }
}

/* Verify state names appear correctly in formatted CSV lines */
void test_state_names_in_csv_line(void) {
    char buf[256];
    format_transition_line(buf, sizeof(buf), 100,
        MISSION_STATE_RECORD_DIVE, MISSION_STATE_RECORD_FLOATING, MISSION_TRANSITION_FLOAT_DETECTED);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "RECORD_DIVE"), "From state not in line");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "RECORD_FLOATING"), "To state not in line");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, "FLOAT_DETECTED"), "Cause not in line");
}

/* ===== Entry point ======================================================== */

int main(void) {
    if (spec_parse("tag_mission.csv", &mission_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for tag_mission.csv from %s\n", SPEC_FILE);
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_spec_filename_matches);

    /* spec contract: structure */
    RUN_TEST(test_spec_field_count_matches_header);
    RUN_TEST(test_spec_field_names_match_header);
    RUN_TEST(test_spec_csv_line_field_count);

    /* csv structure */
    RUN_TEST(test_csv_header_ends_with_newline);
    RUN_TEST(test_csv_line_ends_with_newline);
    RUN_TEST(test_csv_line_no_embedded_newlines);

    /* state/cause name mapping */
    RUN_TEST(test_all_spec_states_have_names);
    RUN_TEST(test_all_spec_causes_have_names);
    RUN_TEST(test_state_names_in_csv_line);

    return UNITY_END();
}
