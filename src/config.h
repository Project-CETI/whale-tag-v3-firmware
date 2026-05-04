
#ifndef CETI_CONFIG_H
#define CETI_CONFIG_H

#include <stdint.h>

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
#define BMS_SAMPLERATE_mS (1000)

/* BURNWIRE*/
#define BURNWIRE_ENABLED (1)

/* ECG CONFIG */
#define ECG_ENABLED (1)
#define ECG_SAMPLERATE_mS (1)

/* ANTENNA FLASHER CONFIG*/
#define FLASHER_ENABLED (0)

/* GPS CONFIG */
#define GPS_ENABLED (1)


/* IMU CONFIG */
#define IMU_ENABLED (1)
#define IMU_ROTATION_SAMPLERATE_mS (50) // 20 Hz
#define IMU_9DOF_SAMPLERATE_mS (20) // 50 Hz

/* PRESSURE CONFIG */
#define PRESSURE_ENABLED (1)
#define PRESSURE_DIVE_THRESHOLD_BAR (5.0)
#define PRESSURE_SURFACE_THRESHOLD_BAR (1.0)
#define PRESSURE_SAMPLERATE_mS (1000)

/* ARGOS ENABLED */
#define SATELLITE_ENABLED (1)

typedef enum {
    ARGOS_MOD_LDA2,
    ARGOS_MOD_VLDA4,
    ARGOS_MOD_LDK,
    ARGOS_MOD_LDA2L,
} RecoveryArgoModulation;

typedef enum {
    AUDIO_FORMAT_RAW,
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_X3,
    AUDIO_FORMAT_FLAC,
} AudioFileFormat;

typedef struct {
    uint8_t enabled;
    uint8_t bitdepth;
    uint8_t filter_type;
    uint8_t priority;
    AudioFileFormat file_format;
    uint8_t channel_enabled[4];
    uint32_t samplerate_sps;
} AudioConfig;

typedef struct {
    uint8_t enabled;
    uint8_t pass_prediction_enabled;
    RecoveryArgoModulation modulation_protocol;
    uint8_t id[8]; // typically 6
    uint8_t address[8];
    uint8_t secret_key[32];
    uint32_t transmission_interval_s;
    uint8_t transmission_variance_percentage;
} ArgosConfig;

typedef struct {
    uint8_t enabled;
    uint32_t duration_s;
} BurnwireConfig;

typedef struct {
    uint8_t enabled;
    uint16_t samplerate_ms;
} BmsConfig;

typedef struct {
    uint8_t enabled;
    uint16_t samplerate_ms;
} EcgConfig;

typedef struct {
    uint8_t enabled;
} FlasherConfig;

typedef struct {
    uint8_t enabled;
} GpsConfig;

typedef enum {
    IMU_SENSOR_ROTATION,
    IMU_SENSOR_ACCELEROMETER,
    IMU_SENSOR_MAGNETOMETER,
    IMU_SENSOR_GYROSCOPE,
    IMU_SENSOR_COUNT,
} ImuSensor;


typedef struct {
    uint8_t enabled;
    uint16_t samplerate_ms;
} ImuSensorConfig;

typedef struct {
    uint8_t enabled;
    ImuSensorConfig sensor[IMU_SENSOR_COUNT];
} ImuConfig;

typedef enum {
    MISSION_STATE_MISSION_START,
    MISSION_STATE_RECORD_SURFACE,
    MISSION_STATE_RECORD_FLOATING,
    MISSION_STATE_RECORD_DIVE,
    MISSION_STATE_BURN,
    MISSION_STATE_LOW_POWER_BURN,
    MISSION_STATE_RETRIEVE,
    MISSION_STATE_LOW_POWER_RETRIEVE,
    MISSION_STATE_PREDEPLOYMENT,
    MISSION_STATE_ERROR
} MissionState;

typedef struct {
    MissionState starting_state;
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
    float   dive_threshold_bar;
    float   surface_threshold_bar;
    uint16_t samplerate_ms;
} PressureConfig;

typedef struct {
    uint8_t enabled;
} VhfPingerConfig;

typedef enum {
    ECG_HW_CONFIG_3_TERMINAL,
    ECG_HW_CONFIG_2_TERMINAL,
} EcgHardwareConfig;

typedef struct {
    struct {
        uint8_t available;
    } argos;
    struct {
        uint8_t available;
        uint8_t channels[4];
    } audio;
    struct {
        uint8_t available;
    } burnwire;
    struct {
        uint8_t available;
    } bms;
    struct {
        uint8_t available;
        EcgHardwareConfig configuration;
        float gain;
    } ecg;
    struct {
        uint8_t available;
    } flasher;
    struct {
        uint8_t available;
    } water_sensor;
    struct {
        uint8_t available;
    } gps;
    struct {
        uint8_t available;
    } imu;
    struct { 
        uint8_t available;
    } pressure;
    struct {
        uint8_t available;
        float carrier_frequency_mhz;
    } vhf_pinger;
} HwConfig;

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
    VhfPingerConfig vhf;
    HwConfig hw_config;
} CetiTagRuntimeConfiguration;

extern CetiTagRuntimeConfiguration tag_config;



void aop_update(uint8_t *data, uint16_t data_size);

void config_save(void);
void config_reload(void);
int config_init(void);

#include <assert.h>
_Static_assert(sizeof(CetiTagRuntimeConfiguration) < 8*1024, "!!! Configuration structure exceeds size of allocated config flash region !!!");

#endif // CETI_CONFIG_H
