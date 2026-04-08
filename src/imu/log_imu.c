/*****************************************************************************
 *   @file      log_imu.c
 *   @brief     code for saving acquired IMU data to disk
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *   @date      3/30/2026
 *****************************************************************************/
#include "log_imu.h"

#include "acq_imu.h"

#include "metadata.h"

#include "syslog.h"
#include "util/buffer_writer.h"
/*
    @note imu size buffered as a timestamp, and a measurement packet.
*/

#define IMU_FORMAT_SEPERATE_CSV 0
#define IMU_FORMAT_UNIFIED_CSV 1
#define IMU_FORMAT_BIN 2

#define IMU_FORMAT IMU_FORMAT_SEPERATE_CSV

#include <sh2_SensorValue.h>
#include <app_filex.h> // filex

#include <stdio.h>

extern FX_MEDIA sdio_disk;

#define IMU_MAX_SAMPLE_RATE_HZ 50
#define IMU_BUFFER_LEN_S 8
#define IMU_PACKET_BUFFER_LEN (IMU_BUFFER_LEN_S * IMU_MAX_SAMPLE_RATE_HZ)

/********* SEPERATE CSVs *****************************************************/
#if IMU_FORMAT == IMU_FORMAT_SEPERATE_CSV

static volatile uint16_t s_sample_buffer_write_cursor[IMU_SENSOR_COUNT] = {0};
static uint16_t s_sample_buffer_read_cursor[IMU_SENSOR_COUNT] = {0};

static sh2_SensorValue_t s_sample_buffer[IMU_SENSOR_COUNT][IMU_PACKET_BUFFER_LEN];

#define LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD (3 * 1024)
#define LOG_IMU_ENCODE_BUFFER_SIZE (LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t s_log_encode_buffer[IMU_SENSOR_COUNT][LOG_IMU_ENCODE_BUFFER_SIZE];
static char *s_csv_header[IMU_SENSOR_COUNT] = {
    [IMU_SENSOR_ACCELEROMETER] = "Timestamp [us]"
        ", Notes"
        ", Accel_x_raw"
        ", Accel_y_raw"
        ", Accel_z_raw"
        ", Accel_status"
        "\n"
    ,
    [IMU_SENSOR_GYROSCOPE] = "Timestamp [us]"
        ", Notes"
        ", Gyro_x_raw"
        ", Gyro_y_raw"
        ", Gyro_z_raw"
        ", Gyro_status"
        "\n"
    ,
    [IMU_SENSOR_MAGNETOMETER] = "Timestamp [us]"
        ", Notes"
        ", Mag_x_raw"
        ", Mag_y_raw"
        ", Mag_z_raw"
        ", Mag_status"
        "\n"
    ,
    [IMU_SENSOR_ROTATION] = "Timestamp [us]"
        ", Notes"
        ", Quat_i"
        ", Quat_j"
        ", Quat_k"
        ", Quat_Re"
        ", Quat_accuracy"
        "\n"
    ,
};
static char * s_filename[IMU_SENSOR_COUNT] = {
    [IMU_SENSOR_ACCELEROMETER] = "data_imu_accel.csv",
    [IMU_SENSOR_GYROSCOPE] = "data_imu_gyro.csv",
    [IMU_SENSOR_MAGNETOMETER] = "data_imu_mag.csv",
    [IMU_SENSOR_ROTATION] = "data_imu_quat.csv",
};

static BufferWriter s_bw[IMU_SENSOR_COUNT] = {
    [IMU_SENSOR_ACCELEROMETER] = {
        .buffer = s_log_encode_buffer[IMU_SENSOR_ACCELEROMETER],
        .threshold = LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD,
        .capacity = LOG_IMU_ENCODE_BUFFER_SIZE,
    },
    [IMU_SENSOR_GYROSCOPE] = {
        .buffer = s_log_encode_buffer[IMU_SENSOR_GYROSCOPE],
        .threshold = LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD,
        .capacity = LOG_IMU_ENCODE_BUFFER_SIZE,
    },
    [IMU_SENSOR_MAGNETOMETER] = {
        .buffer = s_log_encode_buffer[IMU_SENSOR_MAGNETOMETER],
        .threshold = LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD,
        .capacity = LOG_IMU_ENCODE_BUFFER_SIZE,
    },
    [IMU_SENSOR_ROTATION] = {
        .buffer = s_log_encode_buffer[IMU_SENSOR_ROTATION],
        .threshold = LOG_IMU_ENCODE_BUFFER_FLUSH_THRESHOLD,
        .capacity = LOG_IMU_ENCODE_BUFFER_SIZE,
    },
};

typedef size_t (*ImuCsvConversionFunction)(const sh2_SensorValue_t *, uint8_t *, size_t);

size_t __accel_sample_to_csv_line(const sh2_SensorValue_t *, uint8_t *, size_t);
size_t __gyro_sample_to_csv_line(const sh2_SensorValue_t *, uint8_t *, size_t);
size_t __mag_sample_to_csv_line(const sh2_SensorValue_t *, uint8_t *, size_t);
size_t __quat_sample_to_csv_line(const sh2_SensorValue_t *, uint8_t *, size_t);


static ImuCsvConversionFunction s_sample_conversion_method[IMU_SENSOR_COUNT] = {
    [IMU_SENSOR_ACCELEROMETER] = __accel_sample_to_csv_line,
    [IMU_SENSOR_GYROSCOPE] = __gyro_sample_to_csv_line,
    [IMU_SENSOR_MAGNETOMETER] = __mag_sample_to_csv_line,
    [IMU_SENSOR_ROTATION] = __quat_sample_to_csv_line,
};

/********* CSV ***************************************************************/
static void __create_csv_file(ImuSensor sensor_type) {
    /* Create/open imu file */
    UINT fx_create_result = fx_file_create(&sdio_disk, s_filename[sensor_type]);
    if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
        #warning "ToDo: Handle Error creating file"
        // Error_Handler();
    }

    /* open file */
    UINT fx_open_result = buffer_writer_open(&s_bw[sensor_type], s_filename[sensor_type]);
    if (FX_SUCCESS != fx_open_result) {
        #warning "ToDo: Handle Error opening file"
        return;
    }

    metadata_log_file_creation(s_filename[sensor_type], DATA_TYPE_IMU_ROTATION + sensor_type, DATA_FORMAT_CSV, 0);
    
    /* file was newly created. Initialize header */
    if (FX_ALREADY_CREATED != fx_create_result) {
        CETI_LOG("Created new imu file \"%s\"", s_filename[sensor_type]);
        buffer_writer_write(&s_bw[sensor_type], (uint8_t *)s_csv_header[sensor_type], strlen(s_csv_header[sensor_type]));
        buffer_writer_flush(&s_bw[sensor_type]);
    }


}

size_t __accel_sample_to_csv_line(const sh2_SensorValue_t *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    size_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status);

    const sh2_Accelerometer_t * accel = &p_sample->un.accelerometer;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, 
        ", %f, %f, %f", 
        accel->x, accel->y, accel->z
    );
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status & 0b11);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

size_t __gyro_sample_to_csv_line(const sh2_SensorValue_t *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    size_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status);
    
    const sh2_Gyroscope_t * gyro = &p_sample->un.gyroscope;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, 
        ", %f, %f, %f", 
        gyro->x, gyro->y, gyro->z
    );

    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status & 0b11);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

size_t __mag_sample_to_csv_line(const sh2_SensorValue_t *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    size_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status);

    const sh2_MagneticField_t * mag = &p_sample->un.magneticField;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, 
        ", %f, %f, %f", 
        mag->x, mag->y, mag->z
    );
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status & 0b11);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

size_t __quat_sample_to_csv_line(const sh2_SensorValue_t *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    size_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->status);

    const sh2_RotationVectorWAcc_t * rotation = &p_sample->un.rotationVector;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, 
        ", %f, %f, %f, %f", 
        rotation->i, rotation->j, rotation->k, rotation->real
    );
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, 
        ", %f", 
        rotation->accuracy
    );
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

void log_imu_init(void) {
    for (int i = 0; i < IMU_SENSOR_COUNT; i++) {
        __create_csv_file(i);
    }
}

void log_imu_deinit(void) {
    log_imu_task();
    for (int i = 0; i < IMU_SENSOR_COUNT; i++) {
        buffer_writer_close(&s_bw[i]);
        s_sample_buffer_write_cursor[i] = 0;
        s_sample_buffer_read_cursor[i] = 0;
    }
}

/// @brief 
/// @param  
void log_imu_task(void) {
    for (int sensor_type = 0; sensor_type < IMU_SENSOR_COUNT; sensor_type++) {
        while (s_sample_buffer_read_cursor[sensor_type] != s_sample_buffer_write_cursor[sensor_type]) {
            uint8_t buffer[256];
            size_t len;
            len = s_sample_conversion_method[sensor_type](&s_sample_buffer[sensor_type][s_sample_buffer_read_cursor[sensor_type]], buffer, sizeof(buffer));
            buffer_writer_write(&s_bw[sensor_type], buffer, len);
            s_sample_buffer_read_cursor[sensor_type] = (s_sample_buffer_read_cursor[sensor_type] + 1) % IMU_PACKET_BUFFER_LEN;
        }
    }
}

void log_imu_accel_sample_callback(const sh2_SensorValue_t *p_sample) {
    uint16_t nv_w = s_sample_buffer_write_cursor[IMU_SENSOR_ACCELEROMETER];
    s_sample_buffer[IMU_SENSOR_ACCELEROMETER][nv_w] = *p_sample;
    nv_w = (nv_w + 1) % IMU_PACKET_BUFFER_LEN;
    if (nv_w == s_sample_buffer_read_cursor[IMU_SENSOR_ACCELEROMETER]) {
        // ToDo: Handle overflow
    }
    s_sample_buffer_write_cursor[IMU_SENSOR_ACCELEROMETER] = nv_w;
}

void log_imu_gyro_sample_callback(const sh2_SensorValue_t *p_sample) {
    uint16_t nv_w = s_sample_buffer_write_cursor[IMU_SENSOR_GYROSCOPE];
    s_sample_buffer[IMU_SENSOR_GYROSCOPE][nv_w] = *p_sample;
    nv_w = (nv_w + 1) % IMU_PACKET_BUFFER_LEN;
    if (nv_w == s_sample_buffer_read_cursor[IMU_SENSOR_GYROSCOPE]) {
        // ToDo: Handle overflow
    }
    s_sample_buffer_write_cursor[IMU_SENSOR_GYROSCOPE] = nv_w;
}

void log_imu_mag_sample_callback(const sh2_SensorValue_t *p_sample) {
    uint16_t nv_w = s_sample_buffer_write_cursor[IMU_SENSOR_MAGNETOMETER];
    s_sample_buffer[IMU_SENSOR_MAGNETOMETER][nv_w] = *p_sample;
    nv_w = (nv_w + 1) % IMU_PACKET_BUFFER_LEN;
    if (nv_w == s_sample_buffer_read_cursor[IMU_SENSOR_MAGNETOMETER]) {
        // ToDo: Handle overflow
    }
    s_sample_buffer_write_cursor[IMU_SENSOR_MAGNETOMETER] = nv_w;
}

void log_imu_quat_sample_callback(const sh2_SensorValue_t *p_sample) {
    uint16_t nv_w = s_sample_buffer_write_cursor[IMU_SENSOR_ROTATION];
    s_sample_buffer[IMU_SENSOR_ROTATION][nv_w] = *p_sample;
    nv_w = (nv_w + 1) % IMU_PACKET_BUFFER_LEN;
    if (nv_w == s_sample_buffer_read_cursor[IMU_SENSOR_ROTATION]) {
        // ToDo: Handle overflow
    }
    s_sample_buffer_write_cursor[IMU_SENSOR_ROTATION] = nv_w;
}

int log_imu_buffer_half_full(ImuSensor sensor_type) {
    int8_t buffered_samples = ((int16_t)s_sample_buffer_write_cursor[sensor_type] - (int16_t)s_sample_buffer_read_cursor[sensor_type]);
    buffered_samples = (buffered_samples >= 0) ? buffered_samples :  IMU_PACKET_BUFFER_LEN + buffered_samples;
    if (buffered_samples >= IMU_PACKET_BUFFER_LEN/2) {
        return 1;
    }
    return 0;
}

/// @brief checks if any imu sample buffer is half full
/// @param  
/// @return bool
int log_imu_any_buffer_half_full(void) {
    for (int sensor_type = 0; sensor_type < IMU_SENSOR_COUNT; sensor_type++) {
        if(log_imu_buffer_half_full(sensor_type)){
            return 1;
        }
    }
    return 0;
}

#elif IMU_FORMAT == IMU_FORMAT_UNIFIED_CSV
#error IMU_FORMAT_UNIFIED_CSV not implemented
#elif IMU_FORMAT == IMU_FORMAT_BIN
#error IMU_FORMAT_BIN not implemented
#endif