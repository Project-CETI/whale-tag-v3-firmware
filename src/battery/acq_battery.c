/*****************************************************************************
 *   @file      battery/acq_battery.c
 *   @brief     Battery sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "acq_battery.h"
#include "bms_ctl.h"
#include "max17320.h"

#include "log/log_syslog.h"

#include "main.h"
#include "tim.h"


extern TIM_HandleTypeDef BATTERY_htim;

#define ACQ_BATTERY_BUFFER_SIZE (8)

CetiBatterySample acq_battery_buffer[ACQ_BATTERY_BUFFER_SIZE];
size_t acq_battery_buffer_write_position = 0;
size_t acq_battery_buffer_latest_position = 0;
size_t acq_battery_buffer_read_position = 0;

/* wrapped to make read-only*/
size_t acq_battery_buffer_get_write_position(void) {
    return acq_battery_buffer_write_position;
}

void acq_battery_get_sample(CetiBatterySample *pSample) {
    // create sample
    pSample->time_us = rtc_get_epoch_us();
    pSample->error = 0;
    if (pSample->error == 0) {
        pSample->error = max17320_get_cell_voltage_v(0, &pSample->cell_voltage_v[0]);
    }
    if (pSample->error == 0) {
        pSample->error = max17320_get_cell_voltage_v(1, &pSample->cell_voltage_v[1]);
    }
    if (pSample->error == WT_OK) {
        pSample->error = max17320_get_average_current_mA(&pSample->current_mA);
    }
    for (int i = 0; i < MAX17320_CELL_COUNT; i++) {
        if (pSample->error != 0) {
            break;
        }
        max17320_get_cell_temperature_c(i, &pSample->cell_temperature_c[i]);
    }
    if (pSample->error == WT_OK) {
        pSample->error = max17320_get_state_of_charge(&pSample->state_of_charge_percent);
    }
    if (pSample->error == WT_OK) {
        pSample->error = max17320_read(MAX17320_REG_STATUS, &pSample->status);
    }
    if (pSample->error == WT_OK) {
        pSample->error = max17320_read(MAX17320_REG_PROTALRT, &pSample->protection_alert);
    }

    // clear protection alert flags and status flags
    if (pSample->error == WT_OK) {
        pSample->error = max17320_write(MAX17320_REG_PROTALRT, 0x0000);
    }
    if (pSample->error == WT_OK) {
        pSample->error = max17320_write(MAX17320_REG_STATUS, 0x0000);
    }
}

void __acq_battery_timer_complete_cb(TIM_HandleTypeDef *htim) {
    acq_battery_get_sample(&acq_battery_buffer[acq_battery_buffer_write_position]);
    acq_battery_buffer_latest_position = acq_battery_buffer_write_position;
    size_t next_w_pos = (acq_battery_buffer_latest_position + 1) % ACQ_BATTERY_BUFFER_SIZE;
    if (next_w_pos == acq_battery_buffer_read_position) {
        // ToDo: alert overflow
    }
    acq_battery_buffer_write_position = next_w_pos;
}

const CetiBatterySample* acq_battery_get_next_sample(void) {
    if (acq_battery_buffer_read_position == acq_battery_buffer_write_position) {
        return NULL;
    }
    const CetiBatterySample *next_sample = &acq_battery_buffer[acq_battery_buffer_read_position];
    acq_battery_buffer_read_position = (acq_battery_buffer_read_position + 1) % ACQ_BATTERY_BUFFER_SIZE;
    return next_sample;
}

/**
 * @brief returns a pointer to the latest battery sample
 * 
 * @return const CetiBatterySample* 
 */
const CetiBatterySample* acq_battery_peak_latest_sample(void) {
    return &acq_battery_buffer[acq_battery_buffer_latest_position];
}

/**
 * @brief starts data acquisition of battery samples on 1 second interval
 * 
 */
void acq_battery_enable(void) {
    // validate max17320
    int bms_settings_verified = bms_ctl_verify();
    if (!bms_settings_verified) {
        // CETI_ERR("MAX17320 nonvolatile memory was not as expected: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        CETI_ERR("    Consider rewriting NV memory!!!!");
        CETI_LOG("Attempting to overlay values:");
        bms_ctl_temporary_overwrite_nv_values();
    }

	max17320_clear_write_protection();
    //Note: consider not using MX_TIM2 generated code to move easily swap timers 
    MX_TIM2_Init();
    HAL_TIM_RegisterCallback(&BATTERY_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, __acq_battery_timer_complete_cb);
    HAL_TIM_Base_Start_IT(&BATTERY_htim);
}
