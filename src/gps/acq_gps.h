/*****************************************************************************
 *   @file      acq_gps.h
 *   @brief     This header is the api to communicate with the GPS module
 *              (uBlox M10s)
 *   @project   Project CETI
 *   @date      06/22/2023
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, Kaveet Grewal,
 *****************************************************************************/

#ifndef INC_RECOVERY_INC_GPS_H_
#define INC_RECOVERY_INC_GPS_H_

#include <stdint.h> //for uint8_t


#define NMEA_MAX_SIZE (82)
#define GPS_BUFFER_SIZE (NMEA_MAX_SIZE + 1)

typedef struct {
    uint64_t timestamp_us;
    uint8_t msg[GPS_BUFFER_SIZE];
    uint16_t msg_len;
} GpsSentence;

/* public methods */
void gps_init(void);

void gps_sleep(void); // stops gps collection thread
void gps_wake(void);  // starts gps collection thread
void gps_standby(void);

void gps_set_data_rate(uint32_t period_s);

void gps_low_data_rate(void);
void gps_high_data_rate(void);

void gps_register_msg_complete_callback(void (*callback)(const GpsSentence *));

#endif /* INC_RECOVERY_INC_GPS_H_ */
