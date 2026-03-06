//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: PDevice driver for Keller 4LD pressure transmitter
//-----------------------------------------------------------------------------
#ifndef __CETI_WHALE_TAG_HAL_KELLER_4LD__
#define __CETI_WHALE_TAG_HAL_KELLER_4LD__

#include "stm32u5xx_hal.h"

#include <stdint.h>

// Keller 4LD Pressure Sensor 200 bar
// Reference pressure is a 1 bar abs
#define PRESSURE_MIN 0   // bar
#define PRESSURE_MAX 200 // bar

#define KELLER4LD_RAW_TO_PRESSURE_BAR(raw) (((double)(raw)-16384.0) * ((PRESSURE_MAX - PRESSURE_MIN) / 32768.0))
#define KELLER4LD_RAW_TO_TEMPERATURE_C(raw) ((double)(((raw) >> 4) - 24) * 0.05 - 50.0)

#define KELLER4LD_PRESSURE_BAR_TO_RAW(pressure_bar) ((uint16_t)(((pressure_bar) * 32768.0/(PRESSURE_MAX - PRESSURE_MIN)) + 16384.0))

typedef struct {
    uint8_t status;
    uint16_t pressure;
    uint16_t temperature;
} Keller4LD_Measurement;

HAL_StatusTypeDef keller4ld_request_measurement(void);
HAL_StatusTypeDef keller4ld_read_status(uint8_t *pStatus);
HAL_StatusTypeDef keller4ld_read_measurement_it(Keller4LD_Measurement *pData);
void keller4ld_raw_to_measurement(const uint8_t p_raw[static 5], Keller4LD_Measurement *p_measurement);
void keller4ld_eoc_callback(void);
void keller4ld_register_measurement_complete_callback(void (*p_callback)(uint8_t raw_packet[static 5]));

#endif // __CETI_WHALE_TAG_HAL_KELLER_4LD__
