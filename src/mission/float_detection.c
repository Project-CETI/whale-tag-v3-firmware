/*****************************************************************************
 *   @file      mission/float_detection.c
 *   @brief     float detection
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "float_detection.h"

#include <math.h>
#include <string.h>
#include <timing.h>

#include "sh2_SensorValue.h"


#include "util/quaternion.h"

IMPLEMENT_QUATERION_TYPE(float) // use float based quaternions


// ToDo: imperically update these values based on hardware
#define FLOAT_DETECT_TARGET_PITCH_RAD ( -85.0 ) * ( M_PI / 180.0 )
#define FLOAT_DETECT_TARGET_ROLL_RAD ( 0.0 ) * ( M_PI / 180.0 )
#define FLOAT_DETECT_TARGET_ACCURACY_RAD (10.0) * ( M_PI / 180.0 )

#define FLOAT_DETECT_HOLD_TIME_S 10*60 
#define FLOAT_DETECTION_BUFFER_LEN (10)

static Quaternion(float) s_measurement_queue[FLOAT_DETECTION_BUFFER_LEN] = {};
static Quaternion(float) s_measurement_sum = {};
static size_t s_buffer_cursor_position = 0;
static uint64_t s_float_start_detected = 0;
static uint64_t s_float_start_time_s = 0;
/*
    [float_detection_timer] -> [buffer sample]
*/

static void priv__reset_float_detection(void) {
    bzero(s_measurement_queue, sizeof(s_measurement_queue));
    bzero(&s_measurement_sum, sizeof(s_measurement_sum));
    s_buffer_cursor_position = 0;
}

static void priv__get_average_rotation(sh2_RotationVectorWAcc_t *q) {
    float sum_length = quaternion_magnitude(float)(s_measurement_sum);

    sh2_RotationVectorWAcc_t estimated_rotation;
    estimated_rotation.i = s_measurement_sum.i/sum_length;
    estimated_rotation.j = s_measurement_sum.j/sum_length;
    estimated_rotation.k = s_measurement_sum.k/sum_length;
    estimated_rotation.real = s_measurement_sum.real/sum_length;
    estimated_rotation.accuracy = acos(sum_length/FLOAT_DETECTION_BUFFER_LEN);
    if (NULL != q) {
        *q = estimated_rotation;
    }
}

static int priv__oriented_upright(void) {
    sh2_RotationVectorWAcc_t q = {};
    priv__get_average_rotation(&q);

    
    // Pitch (x-axis rotation)
    float sinpCosr = 2 * ((q.real * q.i) + (q.j * q.k)); // 2(pitch due to pitch + pitch due to yaw*roll)
    float cospCosr = 1 - 2 * ((q.i * q.i) + (q.j * q.j));
    float pitch = atan2(sinpCosr, cospCosr);

    // Roll (y-axis rotation)
    float sinr = sqrt(1+ 2 * ((q.real * q.j) - (q.k * q.i)));
    float cosr = sqrt(1 + 2 * ((q.real * q.j) - (q.k * q.i)));
    float roll =  (2.0 * atan2(sinr, cosr)) - (M_PI / 2.0);

    // pitch erro
    float d_pitch_norm = fabsf(FLOAT_DETECT_TARGET_PITCH_RAD - pitch);
    float d_roll_norm = fabsf(FLOAT_DETECT_TARGET_ROLL_RAD - roll);
    
    return (d_pitch_norm < FLOAT_DETECT_TARGET_ACCURACY_RAD) 
        && (d_roll_norm < FLOAT_DETECT_TARGET_ACCURACY_RAD)
    ;
}

void float_detection_push_rotation(const sh2_RotationVectorWAcc_t *q) {
    // remove yaw from the quaternion as we don't care about it
    Quaternion(float) yawless_rotation = quaternion_remove_yaw(float)((Quaternion(float)){.i = q->i, .j = q->j, .k = q->k, .real = q->real});
    s_measurement_sum = quaternion_subtraction(float)(s_measurement_sum, s_measurement_queue[s_buffer_cursor_position]);
    s_measurement_queue[s_buffer_cursor_position] = yawless_rotation;
    s_measurement_sum = quaternion_addition(float)(s_measurement_sum, yawless_rotation);
    s_buffer_cursor_position++;

    // is floating
    if (priv__oriented_upright()) {
        if (!s_float_start_detected) {
            s_float_start_time_s =  rtc_get_epoch_s();
            s_float_start_detected = 1;
        }
    } else if (s_float_start_detected) {
        priv__reset_float_detection();
    }
}



/// @brief checks whether the tag is floating in the water
/// @param
/// @return bool - true tag floating; false tag not floating
int float_detection_is_floating(void) {
    return priv__oriented_upright()
        && (rtc_get_epoch_s() - s_float_start_time_s  > FLOAT_DETECT_HOLD_TIME_S);
}
