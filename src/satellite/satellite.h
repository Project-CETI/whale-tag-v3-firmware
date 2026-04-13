/*****************************************************************************
 *   @file      satellite.c
 *   @brief     This header is the api to handles communication Arribada ARGOS
 *              module
 *   @project   Project CETI
 *   @date      12/10/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_SATELLITE_H
#define CETI_SATELLITE_H

/* Public Dependencies */
#include "main.h"
#include <unistd.h>

/* Public Macros */
#define ARGOS_ID_LENGTH 6
#define ARGOS_MAC_ADDRESS_LENGTH 8
#define ARGOS_SECRET_KEY_LENGTH 32

/* Public Types */
typedef enum {
    ARGOS_MOD_LDA2,
    ARGOS_MOD_VLDA4,
    ARGOS_MOD_LDK,
    ARGOS_MOD_LDA2L,
} RecoveryArgoModulation;

/* Public Variables */

/* Public Functions */
void satellite_rx_callback(UART_HandleTypeDef *huart, uint16_t position);

void satellite_init(void);
void satellite_wait_for_programming(void);
void satellite_transmit(const char *message, size_t message_len);

int satellite_get_id(char dst[ARGOS_ID_LENGTH + 1], uint16_t *length, uint16_t capacity);
int satellite_get_mac_address(char dst[ARGOS_MAC_ADDRESS_LENGTH + 1], uint16_t *length, uint16_t capacity);
int satellite_get_secret_key(char dst[ARGOS_SECRET_KEY_LENGTH + 1], uint16_t *length, uint16_t capacity);
int satellite_get_rconf(RecoveryArgoModulation *rconf);

int satellite_set_id(const char *id, size_t id_len);
int satellite_set_mac_address(const char address[static ARGOS_MAC_ADDRESS_LENGTH]);
int satellite_set_secret_key(const char secret_key[static ARGOS_SECRET_KEY_LENGTH]);

void satellite_configure_radio(RecoveryArgoModulation modulation);
#endif // CETI_SATELLITE_H
