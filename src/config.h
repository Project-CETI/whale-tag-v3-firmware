
#ifndef CETI_CONFIG_H
#define CETI_CONFIG_H

/* AUDIO CONFIG */
#define AUDIO_ENABLED
#define BMS_ENABLED
// #define ECG_ENABLED
// #define IMU_ENABLED
// #define GPS_ENABLED
#define PRESSURE_ENABLED
#define SATELLITE_ENABLED
//  #define USB_ENABLED


#ifndef AUDIO_ENABLED
#warning "Audio is currently disabled"
#endif
#define AUDIO_SAMPLE_BITDEPTH (24)
// #define AUDIO_SAMPLE_BITDEPTH (16)
#define AUDIO_SAMPLERATE_SPS  (96000)
#define AUDIO_CH_0_EN         (1)
#define AUDIO_CH_1_EN         (1)
#define AUDIO_CH_2_EN         (1)
#define AUDIO_CH_3_EN         (1)
#define AUDIO_CHANNEL_MASK    (((AUDIO_CH_0_EN) << 0) | ((AUDIO_CH_1_EN) << 0) | ((AUDIO_CH_2_EN) << 0) | (AUDIO_CH_3_EN << 0))

#define AUDIO_PRIORITIZE_NOISE 0
#define AUDIO_PRIORITIZE_POWER 1
#define AUDIO_PRIORITY AUDIO_PRIORITIZE_NOISE

#define AUDIO_FILTER_SINC     0
#define AUDIO_FILTER_WIDEBAND 1
#define AUDIO_FILTER_TYPE AUDIO_FILTER_WIDEBAND

/* BMS CONFIG */
#ifndef BMS_ENABLED
#warning "BMS is currently disabled"
#endif
#define BMS_SAMPLERATE_HZ (1)

/* ECG CONFIG */
#ifndef ECG_ENABLED
#warning "ECG is currently disabled"
#endif

/* IMU CONFIG */
#ifndef IMU_ENABLED
#warning "IMU is currently disabled"
#endif

/* GPS CONFIG */
#ifndef GPS_ENABLED
#warning "GPS is currently disabled"
#endif

/* PRESSURE CONFIG */
#ifndef PRESSURE_ENABLED
#warning "Pressure is currently disabled"
#endif
#define PRESSURE_SAMPLERATE_HZ (1)

/* SATELLITE CONFIG */
#ifndef SATELLITE_ENABLED
#warning "Satellite communication is currently disabled"
#endif

/* USB CONFIG */
#ifndef USB_ENABLED
#warning "USB is currently disabled"
#endif

#endif // CETI_CONFIG_H
