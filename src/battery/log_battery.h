/*****************************************************************************
 *   @file      battery/log_battery.h
 *   @brief     code for saving acquired battery data to disk
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_LOG_BATTERY_H
#define CETI_LOG_BATTERY_H

#include "acq_battery.h"

void log_battery_buffer_sample(const CetiBatterySample *p_sample);
void log_battery_init(void);
void log_battery_task(void);
int log_battery_sample_buffer_is_half_full(void);
#endif // CETI_LOG_BATTERY_H
