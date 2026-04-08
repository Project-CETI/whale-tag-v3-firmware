/*****************************************************************************
 *   @file      metadata.h
 *   @brief     code to log tag configurations and metadata
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_METADATA_H
#define CETI_METADATA_H

#include <stdint.h>

typedef enum {
    DATA_TYPE_METADATA,
    DATA_TYPE_ARGOS,
    DATA_TYPE_AUDIO,
    DATA_TYPE_BMS,
    DATA_TYPE_BENCHMARK,
    DATA_TYPE_ECG,
    DATA_TYPE_ERRORS,
    DATA_TYPE_GPS,
    DATA_TYPE_IMU_ROTATION,
    DATA_TYPE_IMU_ACCEL,
    DATA_TYPE_IMU_MAG,
    DATA_TYPE_IMU_GYRO,
    DATA_TYPE_MISSION,
    DATA_TYPE_PRESSURE,
} DataType;

typedef enum {
    DATA_FORMAT_YAML,
    DATA_FORMAT_CSV,
    DATA_FORMAT_BIN,
    DATA_FORMAT_TXT,
} DataFormat;

void metadata_create(char * mission_directory);
void metadata_log_file_creation(char * filename, DataType data_type, DataFormat format, uint16_t version);

#endif