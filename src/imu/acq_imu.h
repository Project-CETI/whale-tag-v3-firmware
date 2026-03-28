/*****************************************************************************
 *   @file      imu/acq_imu.h
 *   @brief     IMU sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_ACQ_IMU_H
#define CETI_ACQ_IMU_H

#include <stdint.h>

#include "sh2_SensorValue.h"

typedef enum {
    IMU_SENSOR_ROTATION,
    IMU_SENSOR_ACCELEROMETER,
    IMU_SENSOR_MAGNETOMETER,
    IMU_SENSOR_GYROSCOPE,
    IMU_SENSOR_COUNT,
} ImuSensor;

void acq_imu_init(void);
void acq_imu_task(void);
int acq_imu_start_sensor(ImuSensor sensor, uint32_t time_interval_us);
int acq_imu_stop_sensor(ImuSensor sensor);
void acq_imu_register_callback(ImuSensor sensor_kind, void (*callback)(const sh2_SensorValue_t *));
void acq_imu_stop_all(void);
void acq_imu_EXTI_Callback(void);


#endif // CETI_ACQ_IMU_H