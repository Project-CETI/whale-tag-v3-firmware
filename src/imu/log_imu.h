/*****************************************************************************
 *   @file      log_imu.h
 *   @brief     code for saving acquired IMU data to disk
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *   @date      3/30/2026
 *****************************************************************************/
#ifndef CETI_LOG_IMU_H
#define CETI_LOG_IMU_H

#include <sh2_SensorValue.h>
#include "acq_imu.h"

void log_imu_init(void);
void log_imu_deinit(void);
void log_imu_task(void);
void log_imu_accel_sample_callback(const sh2_SensorValue_t *p_sample);
void log_imu_gyro_sample_callback(const sh2_SensorValue_t *p_sample);
void log_imu_mag_sample_callback(const sh2_SensorValue_t *p_sample);
void log_imu_quat_sample_callback(const sh2_SensorValue_t *p_sample);
int log_imu_any_buffer_half_full(void);
int log_imu_buffer_half_full(ImuSensor sensor_type);

#endif // CETI_LOG_IMU_H