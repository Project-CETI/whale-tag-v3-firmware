/*****************************************************************************
 *   @file      log_argos.h
 *   @brief     code for saving argos tranmission to csv
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *   @date      4/20/2026
 *****************************************************************************/
#ifndef CETI_LOG_ARGOS_H__
#define CETI_LOG_ARGOS_H__
#include "satellite.h"

typedef struct {
    uint64_t timestamp_us;
    uint8_t status;
    RecoveryArgoModulation tx_type;
    uint8_t message[2 * 24 + 1];
} ArgosTxEvent;

void log_argos_init(void);
void log_argos_event(ArgosTxEvent event);
#endif // CETI_LOG_ARGOS_H__
