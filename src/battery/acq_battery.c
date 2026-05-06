/*****************************************************************************
 *   @file      battery/acq_battery.c
 *   @brief     Battery sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "acq_battery.h"
#include "bms_ctl.h"
#include "max17320.h"
#include "timing.h"

#include "syslog.h"

#include "main.h"
#include "tim.h"

extern TIM_HandleTypeDef battery_htim;

static CetiBatterySample s_sample;
static void (*s_sample_complete_callback)(const CetiBatterySample *p_sample) = NULL;

/// @brief Perfroms the action of reading the desired values from the
//// underlying bms device
/// @param
static void priv__get_sample(void) {
    // create sample
    s_sample.time_us = rtc_get_epoch_us();
    s_sample.error = 0;
    if (s_sample.error == 0) {
        s_sample.error = max17320_get_cell_voltage_v(0, &s_sample.cell_voltage_v[0]);
    }
    if (s_sample.error == 0) {
        s_sample.error = max17320_get_cell_voltage_v(1, &s_sample.cell_voltage_v[1]);
    }
    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_get_average_current_mA(&s_sample.current_mA);
    }
    for (int i = 0; i < MAX17320_CELL_COUNT; i++) {
        if (s_sample.error != 0) {
            break;
        }
        max17320_get_cell_temperature_c(i, &s_sample.cell_temperature_c[i]);
    }
    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_get_state_of_charge(&s_sample.state_of_charge_percent);
    }

    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_get_average_power_mw(&s_sample.average_power_mw);
    }

    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_read(MAX17320_REG_STATUS, &s_sample.status);
    }
    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_read(MAX17320_REG_PROTALRT, &s_sample.protection_alert);
    }

    // clear protection alert flags and status flags
    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_write(MAX17320_REG_PROTALRT, 0x0000);
    }
    if (s_sample.error == CETI_STATUS_OK) {
        s_sample.error = max17320_write(MAX17320_REG_STATUS, 0x0000);
    }
}

/// @brief Callback performed at every sampling interval
/// @param htim
static void priv__timer_complete_cb(TIM_HandleTypeDef *htim) {
    priv__get_sample();

    if (NULL != s_sample_complete_callback) {
        s_sample_complete_callback(&s_sample);
    }
}

/// @brief Returns a copy of the latest sample
/// @param p_sample destination sample
void acq_battery_get(CetiBatterySample *p_sample) {
    // prevent sample from being overwritten
    HAL_NVIC_DisableIRQ(BATTERY_TIM_IRQn);
    *p_sample = s_sample;
    HAL_NVIC_EnableIRQ(BATTERY_TIM_IRQn);
}

/// @brief Initializes BMS sampling interval timer
/// @param
void acq_battery_init(void) {
    // Note: consider not using MX_TIM2 generated code to move easily swap timers
    HAL_TIM_RegisterCallback(&battery_htim, HAL_TIM_BASE_MSPINIT_CB_ID, HAL_TIM_Base_MspInit);
    HAL_TIM_RegisterCallback(&battery_htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, HAL_TIM_Base_MspDeInit);
    MX_TIM2_Init();
    HAL_TIM_RegisterCallback(&battery_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, priv__timer_complete_cb);
}

/// @brief Registers callback to be performed after every sample acquisition
/// @param callback
void acq_battery_register_callback(void (*callback)(const CetiBatterySample *)) {
    s_sample_complete_callback = callback;
}

/// @brief Starts battery sensor acquisition
/// @param
void acq_battery_start(void) {
    HAL_TIM_Base_Start_IT(&battery_htim);
}

/// @brief Stops battery sensor acquisition
/// @param
void acq_battery_stop(void) {
    HAL_TIM_Base_Stop_IT(&battery_htim);
}
