/*****************************************************************************
 *   @file      mission/float_detection.h
 *   @brief     float detection
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_MISSION_FLOAT_DETECTION_H__
#define CETI_MISSION_FLOAT_DETECTION_H__
#include "sh2_SensorValue.h"

#include "util/quaternion.h"

DEFINE_QUATERION_TYPE(float)
DEFINE_EULER_TYPE(float)

void float_detection_enable(void);
void float_detection_disable(void);
void float_detection_push_rotation(const sh2_RotationVectorWAcc_t *rotation);
int float_detection_is_floating(void);

#endif // CETI_MISSION_FLOAT_DETECTION_H__
