/*****************************************************************************
 *   @file      log_pressure.h
 *   @brief     pressure processing and storing code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_LOG_PRESSURE_H
#define CETI_LOG_PRESSURE_H

#include "acq_pressure.h"

void log_pressure_buffer_sample(const CetiPressureSample *p_sample);
void log_pressure_init(void);
void log_pressure_task(void);
void log_pressure_deinit(void);
int log_pressure_sample_buffer_is_half_full(void);

#endif // CETI_LOG_PRESSURE_H
