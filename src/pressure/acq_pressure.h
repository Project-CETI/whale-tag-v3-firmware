/*****************************************************************************
 *   @file      acq_pressure.h
 *   @brief     pressure acquisition code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_ACQ_PRESSURE_H
#define CETI_ACQ_PRESSURE_H

#include "keller4ld.h"
#include "timing.h"

typedef struct {
    time_t timestamp_us;
    Keller4LD_Measurement data;
} CetiPressureSample;

#define acq_pressure_raw_to_pressure_bar(raw) KELLER4LD_RAW_TO_PRESSURE_BAR(raw)
#define acq_pressure_raw_to_temperature_c(raw) KELLER4LD_RAW_TO_TEMPERATURE_C(raw)


void acq_pressure_init(uint16_t samplerate_hz);
void acq_pressure_deinit(void);
void acq_pressure_start(void);
void acq_pressure_stop(void);
void acq_pressure_get_latest(CetiPressureSample *p_sample);
void acq_pressure_register_sample_callback(void (*p_callback)(const CetiPressureSample *));

#endif // CETI_ACQ_PRESSURE_H
