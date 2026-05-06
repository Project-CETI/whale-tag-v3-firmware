/*****************************************************************************
 *   @file      acq_pressure.c
 *   @brief     pressure acquisition code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "keller4ld.h"

#include "acq_pressure.h"
#include "timing.h"

#include "main.h"
#include "tim.h"

#include <string.h>

extern TIM_HandleTypeDef pressure_htim;

static CetiPressureSample s_latest_sample;
static uint8_t s_enabled = 0;
static uint8_t s_initialized = 0;

static void (*s_measurement_complete_callback)(const CetiPressureSample *p_sample) = NULL;

// Possible Signal Chain:
// measurement_read : <i2c read measurement start> -> <i2c read measurement complete interrupt>
//                    | <blocking_i2c_read>
// sample: <sample_timer> -> <request measurement> -> <EOC EXTI> -> <measurement_read>
//         | <sample_timer> -> <request measurement> -> <Timer IT> -> <measurement_read>
//         | <sample_timer> -> <request measurement> -> <blocking_wait> -> <measurement_read>

/// @brief
/// @param p_raw
static void priv__sample_complete_callback(uint8_t p_raw[static 5]) {
    // timestamp sample
    Keller4LD_Measurement measurement;

    s_latest_sample.timestamp_us = rtc_get_epoch_us();
    keller4ld_raw_to_measurement(p_raw, &measurement);
    s_latest_sample.status = measurement.status;
    s_latest_sample.pressure = measurement.pressure;
    s_latest_sample.temperature = measurement.temperature;

    // store value in buffer
    if (NULL != s_measurement_complete_callback) {
        s_measurement_complete_callback(&s_latest_sample);
    }
}

/// @brief pressure sensor sampling interval timer callback. Initalizes sensor sample measurement
/// @param htim
static void priv__acq_pressure_timer_complete_cb(TIM_HandleTypeDef *htim) {
    [[maybe_unused]] int result = keller4ld_request_measurement();
}

/// @brief Initialize pressure acquistion system
/// @param
void acq_pressure_init(uint16_t samplerate_ms) {
    if (s_initialized) {
        acq_pressure_deinit();
    }

    HAL_StatusTypeDef ret = HAL_OK;
    // configure pressure sensor exti gpios
    GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = KELLER_DRDY_EXTI9_Pin,
        .Mode = GPIO_MODE_IT_RISING,
        .Pull = GPIO_NOPULL,
    };
    HAL_GPIO_Init(KELLER_DRDY_EXTI9_GPIO_Port, &GPIO_InitStruct);

    // enable DRDY interrupt
    HAL_NVIC_SetPriority(EXTI9_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_IRQn);

    // configure timer to 1 second
    pressure_htim.Instance = PRESSURE_TIM;
    pressure_htim.Init.Prescaler = (40000 - 1); // 0.25 mS
    pressure_htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    pressure_htim.Init.Period = ((4 * (uint32_t)samplerate_ms) - 1);
    pressure_htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    pressure_htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_RegisterCallback(&pressure_htim, HAL_TIM_BASE_MSPINIT_CB_ID, HAL_TIM_Base_MspInit);
    HAL_TIM_RegisterCallback(&pressure_htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, HAL_TIM_Base_MspDeInit);
    ret |= HAL_TIM_Base_Init(&pressure_htim);
    HAL_TIM_RegisterCallback(&pressure_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, priv__acq_pressure_timer_complete_cb);

    TIM_ClockConfigTypeDef sClockSourceConfig = {
        .ClockSource = TIM_CLOCKSOURCE_INTERNAL,
    };
    ret |= HAL_TIM_ConfigClockSource(&pressure_htim, &sClockSourceConfig);

    TIM_MasterConfigTypeDef sMasterConfig = {
        .MasterOutputTrigger = TIM_TRGO_RESET,
        .MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE,
    };
    ret |= HAL_TIMEx_MasterConfigSynchronization(&pressure_htim, &sMasterConfig);
    if (HAL_OK != ret) {
        // ToDo: handle errors
    }

    keller4ld_register_measurement_complete_callback(priv__sample_complete_callback);
    s_initialized = 1;
    return;
}

/// @brief Start Pressure sensor acquisition
/// @param
void acq_pressure_start(void) {
    if (s_enabled) {
        return;
    }
    HAL_TIM_Base_Start_IT(&pressure_htim);
    s_enabled = 1;
}

/// @brief Stop pressure sensor acquisition
/// @param
void acq_pressure_stop(void) {
    HAL_TIM_Base_Stop_IT(&pressure_htim);
    s_enabled = 0;
}

/// @brief Deinitialize
/// @param
void acq_pressure_deinit(void) {
    acq_pressure_stop();
    HAL_TIM_UnRegisterCallback(&pressure_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID);
    HAL_TIM_Base_DeInit(&pressure_htim);
    // ToDo: deinit/reconfigure other hardware
    s_measurement_complete_callback = NULL;
    s_initialized = 0;
}

void acq_pressure_get_latest(CetiPressureSample *p_sample) {
    memcpy(p_sample, &s_latest_sample, sizeof(CetiPressureSample));
}

void acq_pressure_register_sample_callback(void (*p_callback)(const CetiPressureSample *)) {
    s_measurement_complete_callback = p_callback;
}
