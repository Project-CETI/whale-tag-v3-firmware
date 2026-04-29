#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/*--- Stub out hardware dependencies before including log_imu.c ---*/

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
typedef enum {
    DATA_TYPE_IMU_ROTATION = 10,
    DATA_TYPE_IMU_ACCEL,
    DATA_TYPE_IMU_MAG,
    DATA_TYPE_IMU_GYRO,
} DataType;
typedef enum { DATA_FORMAT_CSV = 1 } DataFormat;
void metadata_log_file_creation(char *f, DataType t, DataFormat fmt, uint16_t v) { (void)f; (void)t; (void)fmt; (void)v; }

/* Stub config.h — provide ImuSensor enum */
#define CETI_CONFIG_H
typedef enum {
    IMU_SENSOR_ROTATION,
    IMU_SENSOR_ACCELEROMETER,
    IMU_SENSOR_MAGNETOMETER,
    IMU_SENSOR_GYROSCOPE,
    IMU_SENSOR_COUNT,
} ImuSensor;

/* Stub buffer_writer */
#include "util/buffer_writer.h"
UINT buffer_writer_open(BufferWriter *w, char *filename) { (void)w; (void)filename; return 0; }
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) { (void)w; (void)p_bytes; (void)len; return 0; }
UINT buffer_writer_close(BufferWriter *w) { (void)w; return 0; }
UINT buffer_writer_flush(BufferWriter *w) { (void)w; return 0; }

/* Include unit under test */
#include "imu/log_imu.c"

#include "../../spec_reader.h"
#include "../../csv_spec_assert.h"

void setUp(void) {}
void tearDown(void) {}

/* Parsed specs — loaded once in main */
static FileSpec accel_spec;
static FileSpec gyro_spec;
static FileSpec mag_spec;
static FileSpec quat_spec;

/* ===== Helper: build an sh2_SensorValue_t with known values =============== */

static sh2_SensorValue_t make_accel_sample(uint64_t ts, float x, float y, float z, uint8_t status) {
    sh2_SensorValue_t sv = {0};
    sv.timestamp = ts;
    sv.status = status;
    sv.un.accelerometer.x = x;
    sv.un.accelerometer.y = y;
    sv.un.accelerometer.z = z;
    return sv;
}

static sh2_SensorValue_t make_gyro_sample(uint64_t ts, float x, float y, float z, uint8_t status) {
    sh2_SensorValue_t sv = {0};
    sv.timestamp = ts;
    sv.status = status;
    sv.un.gyroscope.x = x;
    sv.un.gyroscope.y = y;
    sv.un.gyroscope.z = z;
    return sv;
}

static sh2_SensorValue_t make_mag_sample(uint64_t ts, float x, float y, float z, uint8_t status) {
    sh2_SensorValue_t sv = {0};
    sv.timestamp = ts;
    sv.status = status;
    sv.un.magneticField.x = x;
    sv.un.magneticField.y = y;
    sv.un.magneticField.z = z;
    return sv;
}

static sh2_SensorValue_t make_quat_sample(uint64_t ts, float i, float j, float k, float real, float acc) {
    sh2_SensorValue_t sv = {0};
    sv.timestamp = ts;
    sv.un.rotationVector.i = i;
    sv.un.rotationVector.j = j;
    sv.un.rotationVector.k = k;
    sv.un.rotationVector.real = real;
    sv.un.rotationVector.accuracy = acc;
    return sv;
}

/* ===== Accelerometer tests ================================================ */

void test_accel_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("data_imu_accel.csv", s_filename[IMU_SENSOR_ACCELEROMETER]);
}

void test_accel_header_field_count(void) {
    csv_assert_header_field_count(s_csv_header[IMU_SENSOR_ACCELEROMETER], &accel_spec);
}

void test_accel_header_field_names(void) {
    csv_assert_header_field_names(s_csv_header[IMU_SENSOR_ACCELEROMETER], &accel_spec);
}

void test_accel_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_csv_header[IMU_SENSOR_ACCELEROMETER]);
}

void test_accel_csv_line_field_count(void) {
    sh2_SensorValue_t sv = make_accel_sample(1000000, 0.1f, -0.2f, 9.8f, 3);
    uint8_t buf[256];
    size_t len = priv__accel_sample_to_csv_line(&sv, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &accel_spec);
}

void test_accel_csv_line_ends_with_newline(void) {
    sh2_SensorValue_t sv = make_accel_sample(100, 1.0f, 2.0f, 3.0f, 0);
    uint8_t buf[256];
    size_t len = priv__accel_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_accel_csv_line_no_embedded_newlines(void) {
    sh2_SensorValue_t sv = make_accel_sample(100, 1.0f, 2.0f, 3.0f, 0);
    uint8_t buf[256];
    size_t len = priv__accel_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_accel_notes_empty(void) {
    sh2_SensorValue_t sv = make_accel_sample(100, 0.0f, 0.0f, 9.8f, 3);
    uint8_t buf[256];
    priv__accel_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &accel_spec);
}

void test_accel_status_masked_to_2_bits(void) {
    sh2_SensorValue_t sv = make_accel_sample(100, 0.0f, 0.0f, 0.0f, 0xFF);
    uint8_t buf[256];
    priv__accel_sample_to_csv_line(&sv, buf, sizeof(buf));
    char copy[256];
    strncpy(copy, (char *)buf, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';
    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, ',', fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_TRUE(n >= 6);
    int status_val = atoi(fields[5]);
    TEST_ASSERT_TRUE_MESSAGE(status_val >= 0 && status_val <= 3,
        "Accel status should be masked to 2 bits (0-3)");
}

/* ===== Gyroscope tests ==================================================== */

void test_gyro_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("data_imu_gyro.csv", s_filename[IMU_SENSOR_GYROSCOPE]);
}

void test_gyro_header_field_count(void) {
    csv_assert_header_field_count(s_csv_header[IMU_SENSOR_GYROSCOPE], &gyro_spec);
}

void test_gyro_header_field_names(void) {
    csv_assert_header_field_names(s_csv_header[IMU_SENSOR_GYROSCOPE], &gyro_spec);
}

void test_gyro_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_csv_header[IMU_SENSOR_GYROSCOPE]);
}

void test_gyro_csv_line_field_count(void) {
    sh2_SensorValue_t sv = make_gyro_sample(1000000, 0.001f, -0.002f, 0.0001f, 2);
    uint8_t buf[256];
    size_t len = priv__gyro_sample_to_csv_line(&sv, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &gyro_spec);
}

void test_gyro_csv_line_ends_with_newline(void) {
    sh2_SensorValue_t sv = make_gyro_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    size_t len = priv__gyro_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_gyro_csv_line_no_embedded_newlines(void) {
    sh2_SensorValue_t sv = make_gyro_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    size_t len = priv__gyro_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_gyro_notes_empty(void) {
    sh2_SensorValue_t sv = make_gyro_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    priv__gyro_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &gyro_spec);
}

void test_gyro_status_masked_to_2_bits(void) {
    sh2_SensorValue_t sv = make_gyro_sample(100, 0.0f, 0.0f, 0.0f, 0xFF);
    uint8_t buf[256];
    priv__gyro_sample_to_csv_line(&sv, buf, sizeof(buf));
    char copy[256];
    strncpy(copy, (char *)buf, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';
    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, ',', fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_TRUE(n >= 6);
    int status_val = atoi(fields[5]);
    TEST_ASSERT_TRUE_MESSAGE(status_val >= 0 && status_val <= 3,
        "Gyro status should be masked to 2 bits (0-3)");
}

/* ===== Magnetometer tests ================================================= */

void test_mag_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("data_imu_mag.csv", s_filename[IMU_SENSOR_MAGNETOMETER]);
}

void test_mag_header_field_count(void) {
    csv_assert_header_field_count(s_csv_header[IMU_SENSOR_MAGNETOMETER], &mag_spec);
}

void test_mag_header_field_names(void) {
    csv_assert_header_field_names(s_csv_header[IMU_SENSOR_MAGNETOMETER], &mag_spec);
}

void test_mag_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_csv_header[IMU_SENSOR_MAGNETOMETER]);
}

void test_mag_csv_line_field_count(void) {
    sh2_SensorValue_t sv = make_mag_sample(1000000, 23.4f, -12.3f, 45.6f, 2);
    uint8_t buf[256];
    size_t len = priv__mag_sample_to_csv_line(&sv, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &mag_spec);
}

void test_mag_csv_line_ends_with_newline(void) {
    sh2_SensorValue_t sv = make_mag_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    size_t len = priv__mag_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_mag_csv_line_no_embedded_newlines(void) {
    sh2_SensorValue_t sv = make_mag_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    size_t len = priv__mag_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_mag_notes_empty(void) {
    sh2_SensorValue_t sv = make_mag_sample(100, 0.0f, 0.0f, 0.0f, 0);
    uint8_t buf[256];
    priv__mag_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &mag_spec);
}

void test_mag_status_masked_to_2_bits(void) {
    sh2_SensorValue_t sv = make_mag_sample(100, 0.0f, 0.0f, 0.0f, 0xFF);
    uint8_t buf[256];
    priv__mag_sample_to_csv_line(&sv, buf, sizeof(buf));
    char copy[256];
    strncpy(copy, (char *)buf, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';
    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, ',', fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_TRUE(n >= 6);
    int status_val = atoi(fields[5]);
    TEST_ASSERT_TRUE_MESSAGE(status_val >= 0 && status_val <= 3,
        "Mag status should be masked to 2 bits (0-3)");
}

/* ===== Quaternion tests =================================================== */

void test_quat_spec_filename_matches(void) {
    TEST_ASSERT_EQUAL_STRING("data_imu_quat.csv", s_filename[IMU_SENSOR_ROTATION]);
}

void test_quat_header_field_count(void) {
    csv_assert_header_field_count(s_csv_header[IMU_SENSOR_ROTATION], &quat_spec);
}

void test_quat_header_field_names(void) {
    csv_assert_header_field_names(s_csv_header[IMU_SENSOR_ROTATION], &quat_spec);
}

void test_quat_header_ends_with_newline(void) {
    csv_assert_header_ends_with_newline(s_csv_header[IMU_SENSOR_ROTATION]);
}

void test_quat_csv_line_field_count(void) {
    sh2_SensorValue_t sv = make_quat_sample(1000000, 0.01f, -0.02f, 0.03f, 0.999f, 0.017f);
    uint8_t buf[256];
    size_t len = priv__quat_sample_to_csv_line(&sv, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, len);
    csv_assert_line_field_count((char *)buf, len, &quat_spec);
}

void test_quat_csv_line_ends_with_newline(void) {
    sh2_SensorValue_t sv = make_quat_sample(100, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    uint8_t buf[256];
    size_t len = priv__quat_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_ends_with_newline((char *)buf, len);
}

void test_quat_csv_line_no_embedded_newlines(void) {
    sh2_SensorValue_t sv = make_quat_sample(100, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    uint8_t buf[256];
    size_t len = priv__quat_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_line_no_embedded_newlines((char *)buf, len);
}

void test_quat_notes_empty(void) {
    sh2_SensorValue_t sv = make_quat_sample(100, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    uint8_t buf[256];
    priv__quat_sample_to_csv_line(&sv, buf, sizeof(buf));
    csv_assert_notes_empty((char *)buf, &quat_spec);
}

void test_quat_has_7_fields_not_6(void) {
    /* Quaternion has 7 fields (timestamp, notes, i, j, k, Re, accuracy)
       unlike the 3-axis sensors which have 6. Verify no off-by-one. */
    sh2_SensorValue_t sv = make_quat_sample(100, 0.1f, 0.2f, 0.3f, 0.9f, 0.05f);
    uint8_t buf[256];
    size_t len = priv__quat_sample_to_csv_line(&sv, buf, sizeof(buf));
    char copy[256];
    strncpy(copy, (char *)buf, sizeof(copy));
    copy[sizeof(copy) - 1] = '\0';
    char *fields[SPEC_MAX_FIELDS];
    int n = csv_split_fields(copy, ',', fields, SPEC_MAX_FIELDS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(quat_spec.num_fields, n,
        "Quaternion CSV should have 7 fields");
    (void)len;
}

/* ===== Entry point ======================================================== */

int main(void) {
    if (spec_parse("data_imu_accel.csv", &accel_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_imu_accel.csv\n");
        return 1;
    }
    if (spec_parse("data_imu_gyro.csv", &gyro_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_imu_gyro.csv\n");
        return 1;
    }
    if (spec_parse("data_imu_mag.csv", &mag_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_imu_mag.csv\n");
        return 1;
    }
    if (spec_parse("data_imu_quat.csv", &quat_spec) != 0) {
        fprintf(stderr, "FATAL: could not parse spec for data_imu_quat.csv\n");
        return 1;
    }

    UNITY_BEGIN();

    /* Accelerometer */
    RUN_TEST(test_accel_spec_filename_matches);
    RUN_TEST(test_accel_header_field_count);
    RUN_TEST(test_accel_header_field_names);
    RUN_TEST(test_accel_header_ends_with_newline);
    RUN_TEST(test_accel_csv_line_field_count);
    RUN_TEST(test_accel_csv_line_ends_with_newline);
    RUN_TEST(test_accel_csv_line_no_embedded_newlines);
    RUN_TEST(test_accel_notes_empty);
    RUN_TEST(test_accel_status_masked_to_2_bits);

    /* Gyroscope */
    RUN_TEST(test_gyro_spec_filename_matches);
    RUN_TEST(test_gyro_header_field_count);
    RUN_TEST(test_gyro_header_field_names);
    RUN_TEST(test_gyro_header_ends_with_newline);
    RUN_TEST(test_gyro_csv_line_field_count);
    RUN_TEST(test_gyro_csv_line_ends_with_newline);
    RUN_TEST(test_gyro_csv_line_no_embedded_newlines);
    RUN_TEST(test_gyro_notes_empty);
    RUN_TEST(test_gyro_status_masked_to_2_bits);

    /* Magnetometer */
    RUN_TEST(test_mag_spec_filename_matches);
    RUN_TEST(test_mag_header_field_count);
    RUN_TEST(test_mag_header_field_names);
    RUN_TEST(test_mag_header_ends_with_newline);
    RUN_TEST(test_mag_csv_line_field_count);
    RUN_TEST(test_mag_csv_line_ends_with_newline);
    RUN_TEST(test_mag_csv_line_no_embedded_newlines);
    RUN_TEST(test_mag_notes_empty);
    RUN_TEST(test_mag_status_masked_to_2_bits);

    /* Quaternion */
    RUN_TEST(test_quat_spec_filename_matches);
    RUN_TEST(test_quat_header_field_count);
    RUN_TEST(test_quat_header_field_names);
    RUN_TEST(test_quat_header_ends_with_newline);
    RUN_TEST(test_quat_csv_line_field_count);
    RUN_TEST(test_quat_csv_line_ends_with_newline);
    RUN_TEST(test_quat_csv_line_no_embedded_newlines);
    RUN_TEST(test_quat_notes_empty);
    RUN_TEST(test_quat_has_7_fields_not_6);

    return UNITY_END();
}
