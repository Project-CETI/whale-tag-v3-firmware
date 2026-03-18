
#ifndef CETI_CONFIG_H
#define CETI_CONFIG_H

#include <stdint.h>

#include "stm32u5xx_hal.h"


/// CONFIG VERSION CHANGE LOG 
/// !!!IMPORTANT!!! update version if any typedefs in this file change
#define CONFIG_VERSION_ALPHA (0xdeadbeef) // prerelease config version (subject to frequent changes)

#define CURRENT_CONFIG_VERSION (CONFIG_VERSION_ALPHA)

/* AUDIO CONFIG */
#define AUDIO_ENABLED (1)
#define AUDIO_SAMPLE_BITDEPTH (16)
#define AUDIO_SAMPLERATE_SPS (96000)
#define AUDIO_CH_0_EN (1)
#define AUDIO_CH_1_EN (0)
#define AUDIO_CH_2_EN (1)
#define AUDIO_CH_3_EN (1)
#define AUDIO_CHANNEL_MASK (((AUDIO_CH_0_EN) << 0) | ((AUDIO_CH_1_EN) << 0) | ((AUDIO_CH_2_EN) << 0) | (AUDIO_CH_3_EN << 0))
#define AUDIO_CH_COUNT (AUDIO_CH_0_EN + AUDIO_CH_1_EN + AUDIO_CH_2_EN + AUDIO_CH_3_EN)
#define AUDIO_DATARATE_BPS (AUDIO_CH_COUNT * (AUDIO_SAMPLE_BITDEPTH / 8) * AUDIO_SAMPLERATE_SPS)

#define AUDIO_PRIORITIZE_NOISE 0
#define AUDIO_PRIORITIZE_POWER 1
#define AUDIO_PRIORITY AUDIO_PRIORITIZE_POWER

#define AUDIO_FILTER_SINC 0
#define AUDIO_FILTER_WIDEBAND 1
#define AUDIO_FILTER_TYPE AUDIO_FILTER_WIDEBAND

/* BMS CONFIG */
#define BMS_ENABLED (1)
#define BMS_SAMPLERATE_HZ (1)

/* BURNWIRE*/
#define BURNWIRE_ENABLED (1)


/* ECG CONFIG */
#define ECG_ENABLED (0)

/* ANTENNA FLASHER CONFIG*/
#define FLASHER_ENABLED (0)

/* GPS CONFIG */
#define GPS_ENABLED (1)


/* IMU CONFIG */
#define IMU_ENABLED (0)

/* PRESSURE CONFIG */
#define PRESSURE_ENABLED (1)
#define PRESSURE_SAMPLERATE_HZ (1)

/* ARGOS ENABLED */
#define SATELLITE_ENABLED (1)


typedef struct {
    uint8_t enabled;
    uint8_t bitdepth;
    uint8_t filter_type;
    uint8_t priority;
    uint8_t channel_enabled[4];
    uint32_t samplerate_sps;
} AudioConfig;

typedef struct {
    uint8_t enabled;
    uint8_t id[8];
    uint8_t address[8];
    uint8_t secret_key[64];
} ArgosConfig;

typedef struct {
    uint8_t enabled;
    uint32_t duration_s;
} BurnwireConfig;

typedef struct {
    uint8_t enabled;
} BmsConfig;

typedef struct {
    uint8_t enabled;
} EcgConfig;

typedef struct {
    uint8_t enabled;
} FlasherConfig;

typedef struct {
    uint8_t enabled;
} GpsConfig;

typedef struct {
    uint8_t enabled;
    uint8_t quat_enabled;
    uint8_t accel_enabled;
    uint8_t gyro_enabled;
    uint8_t mag_enabled;
    uint16_t quaternion_samplerate_Hz;
    uint16_t accel_samplerate_Hz;
    uint16_t gyro_samplerate_Hz;
    uint16_t mag_samplerate_Hz;
    float quaternion_offest[4][2];
} ImuConfig;

typedef struct {
    uint8_t float_detection_enabled;
    struct {
        uint8_t enabled;
        uint8_t hour;
        uint8_t minute;
    }time_of_day_release_utc;
    struct {
        uint8_t enabled;
        uint8_t hours;
        uint16_t minutes;
    } timer_release;
    struct {
        uint8_t enabled;
        uint16_t threshold_mV;
    } low_power_release;
} MissionConfig;

typedef struct {
    uint8_t enabled;
    uint16_t samplerate_Hz;
} PressureConfig;

typedef struct {
    uint32_t config_version; // used to identify what tag configuration is being used
    uint32_t hw_version; // used for tracking hardware compatibilty
    uint32_t fw_version; // used for updating firmware
    uint8_t hostname[16];

    AudioConfig audio;
    ArgosConfig argos;
    BmsConfig battery;
    BurnwireConfig burnwire;
    EcgConfig ecg;
    FlasherConfig flasher;
    ImuConfig imu;
    GpsConfig gps;
    MissionConfig mission;
    PressureConfig pressure;
} CetiTagRuntimeConfiguration;

extern CetiTagRuntimeConfiguration tag_config;

void aop_update(uint8_t *data, uint16_t data_size);

void config_apply_to_system(void);
void config_save(void);
void config_reload(void);
HAL_StatusTypeDef config_init(void);

#endif // CETI_CONFIG_H
