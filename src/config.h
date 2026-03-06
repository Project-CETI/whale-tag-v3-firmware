
#ifndef CETI_CONFIG_H
#define CETI_CONFIG_H

#include <stdint.h>
#include "stm32u5xx_hal.h"


/* AUDIO CONFIG */
#define AUDIO_ENABLED
#define BMS_ENABLED
#define BURNWIRE_ENABLED
// #define ECG_ENABLED
// #define IMU_ENABLED
#define GPS_ENABLED
#define SATELLITE_ENABLED
#define USB_ENABLED
// #define FLASHER_ENABLED


#ifndef AUDIO_ENABLED
#warning "Audio is currently disabled"
#endif
#define AUDIO_SAMPLE_BITDEPTH (16)
#define AUDIO_SAMPLERATE_SPS  (96000)
#define AUDIO_CH_0_EN         (1)
#define AUDIO_CH_1_EN         (1)
#define AUDIO_CH_2_EN         (1)
#define AUDIO_CH_3_EN         (0)
#define AUDIO_CHANNEL_MASK    (((AUDIO_CH_0_EN) << 0) | ((AUDIO_CH_1_EN) << 0) | ((AUDIO_CH_2_EN) << 0) | (AUDIO_CH_3_EN << 0))
#define AUDIO_CH_COUNT        (AUDIO_CH_0_EN + AUDIO_CH_1_EN + AUDIO_CH_2_EN + AUDIO_CH_3_EN)
#define AUDIO_DATARATE_BPS    (AUDIO_CH_COUNT * (AUDIO_SAMPLE_BITDEPTH/8) * AUDIO_SAMPLERATE_SPS)

#define AUDIO_PRIORITIZE_NOISE 0
#define AUDIO_PRIORITIZE_POWER 1
#define AUDIO_PRIORITY AUDIO_PRIORITIZE_NOISE

#define AUDIO_FILTER_SINC     0
#define AUDIO_FILTER_WIDEBAND 1
#define AUDIO_FILTER_TYPE AUDIO_FILTER_WIDEBAND

/* BMS CONFIG */
#define BMS_SAMPLERATE_HZ (1)

/* ECG CONFIG */
#ifndef ECG_ENABLED
#warning "ECG is currently disabled"
#endif

/* IMU CONFIG */
#ifndef IMU_ENABLED
#warning "IMU is currently disabled"
#endif

/* PRESSURE CONFIG */
#define PRESSURE_ENABLED
#define PRESSURE_SAMPLERATE_HZ (1)

typedef struct {
    uint8_t bitdepth;
    uint8_t channel_mask;
    uint8_t filter_type;
    uint8_t priority;
    uint32_t samplerate_sps;
} AudioConfig;

typedef struct {
    uint32_t version; // used for updating firmware
    AudioConfig audio;
} CetiTagRuntimeConfiguration;

extern CetiTagRuntimeConfiguration tag_config;

void aop_update(uint8_t *data, uint16_t data_size);

void config_apply_to_system(void);
void config_save(void);
void config_reload(void);
HAL_StatusTypeDef config_init(void);

#endif // CETI_CONFIG_H
