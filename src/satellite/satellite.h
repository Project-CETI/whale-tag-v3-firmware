/*****************************************************************************
 *   @file      satellite.h
 *   @brief     
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_SATELLITE_H
#define CETI_SATELLITE_H

void satellite_init(void);
void satellite_wait_for_programming(void);
void satellite_transmit(char *message, size_t message_len);

#endif // CETI_SATELLITE_H

