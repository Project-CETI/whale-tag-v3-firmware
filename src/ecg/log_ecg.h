/*****************************************************************************
 *   @file      ecg/log_ecg.h
 *   @brief     ECG data acquisition and logging code
 *   @project   Project CETI
 *   @date      04/14/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_LOG_ECG_H__
#define CETI_LOG_ECG_H__
#include "acq_ecg.h"

int log_ecg_sample_buffer_is_half_full(void);
void log_ecg_init(void);
void log_ecg_deinit(void);
void log_ecg_push_sample(const EcgSample *sample);
void log_ecg_task(void);

#endif // _CETI_LOG_ECG_H__