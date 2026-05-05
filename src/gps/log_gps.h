/*****************************************************************************
 *   @file      log_gps.h
 *   @brief     GPS Logging
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_LOG_GPS_H__
#define CETI_LOG_GPS_H__

#include "gps/acq_gps.h"

void log_gps_init(void);
void log_gps_deinit(void);
void log_gps_task(void);
int log_gps_buffer_is_half_full(void);
void log_gps_push_sentence(const GpsSentence *p_sentence);

#endif // CETI_LOG_GPS_H__
