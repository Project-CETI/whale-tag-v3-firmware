#ifndef CETI_QUATERNION_H__
#define CETI_QUATERNION_H__
#include <math.h>

#define Euler(T) Euler_##T
#define Quaternion(T) Quaternion_##T
#define quaternion_multiplication(T) quaternion_multiplication_##T
#define quaternion_addition(T) quaternion_addition_##T
#define quaternion_subtraction(T) quaternion_subtraction_##T
#define quaternion_magnitude(T) quaternion_magnitude_##T
#define quaternion_remove_yaw(T) quaternion_remove_yaw_##T
#define quaternion_to_euler(T) quaternion_to_euler##T

#define DEFINE_QUATERION_TYPE(T)                                                         \
typedef struct {                                                                         \
    T real;                                                                              \
    T i;                                                                                 \
    T j;                                                                                 \
    T k;                                                                                 \
} Quaternion(T);                                                                         

#define DEFINE_EULER_TYPE(T) \
typedef struct {             \
    T pitch;                 \
    T yaw;                   \
    T roll;                  \
} Euler(T);                

/* Generic Type implementation */
#define IMPLEMENT_QUATERION_TYPE(T)                                                      \
__attribute__((const)) static inline                                                     \
Quaternion(T) quaternion_multiplication(T)(Quaternion(T) q1, Quaternion(T) q2) {         \
    return (Quaternion(T)){                                                              \
        .real = q1.real * q2.real - q1.i*q2.i - q1.j*q2.j - q1.k*q2.k,                   \
        .i = q1.real * q2.i + q1.i*q2.real + q1.j*q2.k - q1.k*q2.j,                      \
        .j = q1.real * q2.j - q1.i*q2.k + q1.j*q2.real + q1.k*q2.i,                      \
        .k = q1.real * q2.k + q1.i*q2.j - q1.j*q2.i + q1.k*q2.real,                      \
    };                                                                                   \
}                                                                                        \
                                                                                         \
__attribute__((const))                                                                   \
static inline Quaternion(T) quaternion_remove_yaw(T)(Quaternion(T) q_in) {               \
    /* calculate sin(yaw) and cos(yaw) */                                                \
    float sinyCosp = 2.0 * (q_in.real * q_in.k + q_in.i * q_in.j);                       \
    float cosyCosp = 1.0 - 2.0 * (q_in.j * q_in.j + q_in.k * q_in.k);                    \
    float cosp = sqrt(sinyCosp*sinyCosp + cosyCosp*cosyCosp);                            \
    float siny = sinyCosp/cosp;                                                          \
    float cosy = cosyCosp/cosp;                                                          \
    /* use half angle identities to get half yaw values */                               \
    if (cosy == 1.0) {                                                                   \
        return q_in;                                                                     \
    }                                                                                    \
    float sin_half_y = copysign(siny, sqrt((1.0 - cosy)/2.0));                           \
    float cos_half_y = sin_half_y * siny/ (1.0 - cosy); /* sin(y/2) / tan(y/2) */        \
    /* construct and apply inverse yaw rotation quaternion */                            \
    Quaternion(T) q_deyaw = {                                                            \
        .real = cos_half_y,                                                              \
        .i = 0.0,                                                                        \
        .j = 0.0,                                                                        \
        .k = -sin_half_y                                                                 \
    };                                                                                   \
    return quaternion_multiplication(T)(q_deyaw, q_in);                                  \
}                                                                                        \
                                                                                         \
__attribute__((const))                                                                   \
static inline                                                                            \
Quaternion(T) quaternion_addition(T)(Quaternion(T) q1, Quaternion(T) q2) {               \
    return (Quaternion(T)){                                                              \
        .real = q1.real + q2.real,                                                       \
        .i =  q1.i + q2.i,                                                               \
        .j =  q1.j + q2.j,                                                               \
        .k =  q1.k + q2.k,                                                               \
    };                                                                                   \
}                                                                                        \
                                                                                         \
__attribute__((const))                                                                   \
static inline                                                                            \
Quaternion(T) quaternion_subtraction(T)(Quaternion(T) q1, Quaternion(T) q2) {            \
    return (Quaternion(T)){                                                              \
        .real = q1.real - q2.real,                                                       \
        .i =  q1.i - q2.i,                                                               \
        .j =  q1.j - q2.j,                                                               \
        .k =  q1.k - q2.k,                                                               \
    };                                                                                   \
}                                                                                        \
                                                                                         \
                                                                                         \
__attribute__((const))                                                                   \
static inline                                                                            \
T quaternion_magnitude(T)(Quaternion(T) q) {                                             \
    return sqrt(q.real*q.real + q.i*q.i + q.j*q.j + q.k*q.k);                            \
}                                                                                        \
                                                                                         \
[[gnu::const]]                                                                           \
Euler(T) quaternion_to_euler(T)(Quaternion(T) q) {                                       \
    T sinpCosr = 2 * ((q.real * q.i) + (q.j * q.k));                                     \
    T cospCosr = 1 - 2 * ((q.i * q.i) + (q.j * q.j));                                    \
    T pitch = atan2(sinpCosr, cospCosr);                                                 \
                                                                                         \
    T sinyCosp = 2.0 * (q.real * q.k + q.i * q.j);                                       \
    T cosyCosp = 1.0 - 2.0 * (q.j * q.j + q.k * q.k);                                    \
    T yaw = atan2(sinyCosp, cosyCosp);                                                   \
                                                                                         \
    T sinr = sqrt(1+ 2 * ((q.real * q.j) - (q.k * q.i)));                                \
    T cosr = sqrt(1 + 2 * ((q.real * q.j) - (q.k * q.i)));                               \
    T roll =  (2.0 * atan2(sinr, cosr)) - (M_PI / 2.0);                                  \
                                                                                         \
    return (Euler(T)) {                                                                  \
        .pitch = pitch,                                                                  \
        .yaw = yaw,                                                                      \
        .roll = roll,                                                                    \
    };                                                                                   \
}

#endif // CETI_QUATERNION_H__
