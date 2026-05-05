/*****************************************************************************
 *   @file      acq_pressure.h
 *   @brief     pressure acquisition code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_ACQ_PRESSURE_H
#define CETI_ACQ_PRESSURE_H

#include <stdint.h>

#define PRESSURE_MIN 0   // bar
#define PRESSURE_MAX 200 // bar
#define acq_pressure_raw_to_pressure_bar(raw) (((double)(raw) - 16384.0) * ((PRESSURE_MAX - PRESSURE_MIN) / 32768.0))
#define acq_pressure_raw_to_temperature_c(raw) ((double)(((raw) >> 4) - 24) * 0.05 - 50.0)

#define acq_pressure_bar_to_pressure_raw(pressure_bar) ((uint16_t)(((pressure_bar) * 32768.0 / (PRESSURE_MAX - PRESSURE_MIN)) + 16384.0))

typedef struct {
    uint64_t timestamp_us;
    uint8_t status;
    uint16_t pressure;
    uint16_t temperature;
} CetiPressureSample;

void acq_pressure_init(uint16_t samplerate_hz);
void acq_pressure_deinit(void);
void acq_pressure_start(void);
void acq_pressure_stop(void);
void acq_pressure_get_latest(CetiPressureSample *p_sample);
void acq_pressure_register_sample_callback(void (*p_callback)(const CetiPressureSample *));

#endif // CETI_ACQ_PRESSURE_H
