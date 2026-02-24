/*****************************************************************************
 *   @file      gps.h
 *   @brief     This header is the api to communicate with the GPS module 
 *              (uBlox M10s) 
 *   @project   Project CETI
 *   @date      06/22/2023
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, Kaveet Grewal, 
 *              [TODO: Add other contributors here]
 *****************************************************************************/

#ifndef INC_RECOVERY_INC_GPS_H_
#define INC_RECOVERY_INC_GPS_H_

#include "main.h"
#include <stdint.h> //for uint8_t

#define GPS_BULK_TRANSFER_SIZE (8*512)

/* public methods */
void gps_rx_callback(UART_HandleTypeDef *huart, uint16_t pos);

uint8_t gps_bulk_queue_is_empty(void);
const uint8_t *gps_pop_bulk_transfer(void);

uint8_t gps_queue_is_empty(void);
const uint8_t *gps_pop_sentence(void);

void gps_sleep(void); 	// stops gps collection thread
void gps_wake(void); 	// starts gps collection thread
void gps_standby(void); 

void gps_set_data_rate(uint32_t period_s);

void gps_low_data_rate(void);
void gps_high_data_rate(void);

#endif /* INC_RECOVERY_INC_GPS_H_ */
