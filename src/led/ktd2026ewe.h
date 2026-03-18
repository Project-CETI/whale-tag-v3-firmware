//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: Device driver for ktd2026ewe i2c led driver
//-----------------------------------------------------------------------------
#ifndef __KTD2026EWE_H__
#define __KTD2026EWE_H__

#include <stdint.h>

#include "stm32u5xx_hal.h"

/***  Public Types  **********************************************************/
typedef enum {
    KTD2026EWE_MODE_OFF = 0b00,
    KTD2026EWE_MODE_ON = 0b01,
    KTD2026EWE_MODE_PWM1 = 0b10,
    KTD2026EWE_MODE_PWM2 = 0b11,
} ktd2026ewe_LedMode;

/***  Function Declarations  *************************************************/
HAL_StatusTypeDef ktd2026ewe_reset(void);
HAL_StatusTypeDef ktd2026ewe_set_led_mode(uint8_t led_index, ktd2026ewe_LedMode mode);
HAL_StatusTypeDef ktd2026ewe_set_modes(ktd2026ewe_LedMode mode0, ktd2026ewe_LedMode mode1, ktd2026ewe_LedMode mode2);
HAL_StatusTypeDef ktd2026ewe_set_led_current(uint8_t led_index, float current_mA);
HAL_StatusTypeDef ktd2026ewe_set_period_s(float period_s);
HAL_StatusTypeDef ktd2026ewe_set_pwm1_duty(float duty);
HAL_StatusTypeDef ktd2026ewe_set_pwm2_duty(float duty);

#endif // __KTD2026EWE_H__
