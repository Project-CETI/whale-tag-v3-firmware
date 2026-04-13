/*****************************************************************************
 *   @file      argos_tx_mgr.h
 *   @brief     This code manages when the recovery board should transmit its
 *              GPS coordinates via argos. It implements both a psuedorandom
 *              timer strategy and a satellite pass prediction algorithm.
 *   @project   Project CETI
 *   @date      12/10/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef ARGOS_TX_STRATEGY_HEADER
#define ARGOS_TX_STRATEGY_HEADER

#include "main.h"
#include "version_hw.h"

/* PUBLIC MACROS */
#define ARGOS_TX_STRATEGY_TIMER (0)
#define ARGOS_TX_STRATEGY_PATH_PREDICTOR (1)

#define ARGOS_TX_STRATEGY (ARGOS_TX_STRATEGY_TIMER)

/* PUBLIC FUNCTIONS */
void argos_tx_mgr_pass_start_alarm_callback(RTC_HandleTypeDef *hrtc);
void argos_tx_mgr_TIM_IRQ(TIM_HandleTypeDef *htim);
void argos_tx_mgr_enable(void);
void argos_tx_mgr_disable(void);
uint8_t argos_tx_mgr_ready_to_tx(void);
void argos_tx_mgr_inval_ready_to_tx(void);
void argos_tx_mgr_set_coordinates(float latitude, float longitude);

#endif // ARGOS_TX_STRATEGY_HEADER
