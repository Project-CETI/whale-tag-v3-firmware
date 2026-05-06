/*****************************************************************************
 *   @file      log_gps.h
 *   @brief     This header parses collected the GPS data
 *              (uBlox M10s)
 *   @project   Project CETI
 *   @date      06/22/2023
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, Kaveet Grewal,
 *****************************************************************************/
#ifndef CETI_PARSE_GPS_H_
#define CETI_PARSE_GPS_H_

#include "acq_gps.h"

typedef struct {
    uint8_t valid;
    uint8_t year;
    uint8_t month;
    uint8_t day;     // (0..31]
    uint8_t hours;   // (0..24]
    uint8_t minutes; // (0..60]
    uint8_t seconds; // (0..60] // used for RTC sync not logging
    uint8_t latitude_sign;
    uint8_t longitude_sign;
    // uint8_t unused[3];
    float latitude;
    float longitude;
} GpsCoord;

void parse_gps_push_sentence(const GpsSentence *p_sentence);
GpsCoord parse_gps_get_latest_coordinates(void);

#endif // CETI_PARSE_GPS_H_
