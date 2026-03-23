/*****************************************************************************
 *   @file      log_gps.h
 *   @brief     Code for logging acquired GPS messages to file
 *   @project   Project CETI
 *   @date      03/20/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, Kaveet Grewal,
 *              [TODO: Add other contributors here]
 *****************************************************************************/

#ifndef INC_RECOVERY_INC_LOG_GPS_H_
#define INC_RECOVERY_INC_LOG_GPS_H_

#include "gps.h" // for GpsSentence

#define LOG_GPS_FORMAT_RAW (0)
#define LOG_GPS_FORMAT_CSV (1)

#define LOG_GPS_FORMAT LOG_GPS_FORMAT_RAW

void log_gps_init(void);
void log_gps_tack(void);
void log_gps_deinit(void);
int log_gps_task_call_required(void);

#if LOG_GPS_FORMAT == LOG_GPS_FORMAT_RAW
#define log_gps_message_complete_callback NULL
#else 
void log_gps_message_complete_callback(const GpsSentence *p_sentence);
#endif


#if LOG_GPS_FORMAT == LOG_GPS_FORMAT_CSV
#define log_gps_raw_rx_complete NULL
#else 
static void log_gps_raw_rx_complete(const uint8_t *data, size_t len);
#endif

#endif // INC_RECOVERY_INC_LOG_GPS_H_
