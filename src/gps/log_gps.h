/*****************************************************************************
 *   @file      log_gps.h
 *   @brief     GPS Logging
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef __CETI_LOG_GPS_H__
#define __CETI_LOG_GPS_H__

void log_gps_init(void);
void log_gps_deinit(void);
void log_gps_task(void);

#endif // __CETI_LOG_GPS_H__
