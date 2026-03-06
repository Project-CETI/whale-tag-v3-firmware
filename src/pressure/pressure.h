/*****************************************************************************
 *   @file      pressure.h
 *   @brief     pressure processing and storing code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_PRESSURE_H
#define CETI_PRESSURE_H

#include <stdint.h>
#include "acq_pressure.h"
#include "log_pressure.h"

void pressure_init(uint16_t samplerate_hz);
void pressure_start(void);
void pressure_get(CetiPressureSample *p_sample);
void pressure_task(void);
void pressure_deinit(void);

#ifdef STB_PRESSURE_IMPLEMENTATION
void pressure_init(uint16_t samplerate_hz) {
    // initialize logging code
    log_pressure_init();

    // link logging and  acquisition code
    acq_pressure_register_sample_callback(log_pressure_buffer_sample);

    // initialize sensor acquistion
    acq_pressure_init(samplerate_hz);
}

void pressure_start(void) {
    acq_pressure_start();
}

void pressure_get(CetiPressureSample *p_sample) {
    acq_pressure_get_latest(p_sample);
}

void pressure_task(void) {
    log_pressure_task();
}

void pressure_deinit(void) {
    acq_pressure_deinit();
    log_pressure_deinit();
}
#endif // STB_PRESSURE_IMPLEMENTATION

#endif // CETI_PRESSURE_H
