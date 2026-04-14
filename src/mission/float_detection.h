#include <math.h>
#include <string.h>

#include "sh2_SensorValue.h"


#include "util/quaternion.h"

IMPLEMENT_QUATERION_TYPE(float) // use float based quaternions

#define FLOAT_DETECT_HOLD_TIME_S 20*60 
#define FLOAT_DETECTION_BUFFER_LEN (20)
const Quaternion(float) s_target_float_angle
static Quaternion(float) s_measurement_queue[FLOAT_DETECTION_BUFFER_LEN];
static Quaternion(float) s_measurement_sum = {};
static size_t s_buffer_cursor_position = 0;


/*
    [float_detection_timer] -> [buffer sample]
*/



int float_detect_is_oriented_upright(void) {
    // Pitch (x-axis rotation)
    const sinrCosp = 2 * (real * i + j * k); // 2(pitch due to pitch + pitch due to yaw*roll)
    const cosrCosp = 1 - 2 * (i * i + j * j);
    const pitch = Math.atan2(sinrCosp, cosrCosp) * RAD2DEG;

    // Roll (y-axis rotation)
    const sinp = 2 * (real * j - k * i);
    const roll = (Math.abs(sinp) >= 1
        ? Math.sign(sinp) * 90
        : Math.asin(sinp) * RAD2DEG);
}

// float
void float_detection_enable(void) {
    // ToDo: set float detection timer?
}

void float_detection_disable(void) {
    bzero(s_measurement_queue, sizeof(s_measurement_queue));
    bzero(&s_measurement_sum, sizeof(s_measurement_sum));
    s_buffer_cursor_position = 0;
}

void float_detection_push_rotation(const sh2_RotationVectorWAcc_t *rotation) {
    // remove yaw from the quaternion as we don't care about it
    Quaternion(float) yawless_rotation = quaternion_remove_yaw(float)((Quaternion(float)){.i = rotation.i, .j = rotation.j, .k = rotation.k, .real = rotation.real});
    s_measurement_sum = quaternion_subtraction(float)(s_measurement_sum, s_measurement_queue[s_buffer_cursor_position]);
    s_measurement_queue[s_buffer_cursor_position] = yawless_rotation;
    s_measurement_sum = quaternion_addition(float)(s_measurement_sum, yawless_rotation);

    s_buffer_cursor_position++;
}

void float_detection_get_average_rotation(sh2_RotationVectorWAcc_t *rotation) {
    float sum_length = quaternion_magnitude(float)(s_measurement_sum);

    sh2_RotationVectorWAcc_t estimated_rotation;
    estimated_rotation.i = s_measurement_sum.i/sum_length;
    estimated_rotation.j = s_measurement_sum.j/sum_length;
    estimated_rotation.k = s_measurement_sum.k/sum_length;
    estimated_rotation.real = s_measurement_sum.real/sum_length;
    estimated_rotation.accuracy = acos(sum_length/FLOAT_DETECTION_BUFFER_LEN);
    if (NULL != rotation) {
        *rotation = estimated_rotation;
    }
}

int float_detection_is_floating(void) {
    // ToDo: is upright
    // ToDo:
}