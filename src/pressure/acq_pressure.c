/*****************************************************************************
 *   @file      acq_pressure.c
 *   @brief     pressure acquisition code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "acq_pressure.h"

#include "main.h"
#include "tim.h"

#include <string.h>

extern TIM_HandleTypeDef PRESSURE_htim;

#define ACQ_PRESSURE_BUFFER_SIZE (8)

static CetiPressureSample pressure_sample_buffer[ACQ_PRESSURE_BUFFER_SIZE]; //minute long buffer
static volatile uint16_t s_acq_pressure_write_position = 0;
static uint16_t s_acq_pressure_latest_position = 0;
static uint16_t s_acq_pressure_read_position = 0;

// Signal Chain:
// TIM3 -> <request measurement> -> <EOC EXTI> -> <read measurement start> -> <read measurement complete interrupt>

/// @brief pressure sensor reading complete external interrupt callback
/// @param  
void acq_pressure_EXTI_cb(void) {
    CetiPressureSample * p_sample = &pressure_sample_buffer[s_acq_pressure_write_position];
    p_sample->timestamp_us = rtc_get_epoch_us();
    [[ maybe_unused ]] int result = keller4ld_read_measurement_it(&p_sample->data);

    s_acq_pressure_latest_position = s_acq_pressure_write_position;
    s_acq_pressure_write_position = (s_acq_pressure_write_position + 1) % ACQ_PRESSURE_BUFFER_SIZE;
    if (s_acq_pressure_write_position == s_acq_pressure_read_position) {
        // ToDo: Handle overflow
    }
}

/// @brief pressure sensor sampling interval timer callback
/// @param htim 
static void __acq_pressure_timer_complete_cb(TIM_HandleTypeDef *htim) {
	[[ maybe_unused ]] int result = keller4ld_request_measurement_it();
}

// Note: this method is currently kinda unsafe since the buffer could be overwritten once pointer is returned
// however it is faster than copying the memory to another location
const CetiPressureSample *acq_pressure_get_next_sample(void) {
    if (s_acq_pressure_read_position == s_acq_pressure_write_position) {
        return NULL;
    }

    const CetiPressureSample *next_sample = &pressure_sample_buffer[s_acq_pressure_read_position];
    s_acq_pressure_read_position = (s_acq_pressure_read_position + 1) % ACQ_PRESSURE_BUFFER_SIZE;
    return next_sample;
}

void acq_pressure_get_next_buffer_range(void **ppBuffer, size_t *pSize) {
    if ((ppBuffer == NULL) || (pSize == NULL)){
        //ToDo: bad params handler
        return;
    }

    // calculate buffer range and size
    uint16_t end_index = s_acq_pressure_write_position;
    if (end_index < s_acq_pressure_read_position) {
        end_index = ACQ_PRESSURE_BUFFER_SIZE; // only go to end if 
    }
    *ppBuffer = &pressure_sample_buffer[s_acq_pressure_read_position];
    *pSize = (end_index - s_acq_pressure_read_position)*sizeof(CetiPressureSample);
    
    // update read position
    s_acq_pressure_read_position = end_index % ACQ_PRESSURE_BUFFER_SIZE;
}

void acq_pressure_peak_latest_sample(CetiPressureSample *pSample) {
    memcpy(pSample, &pressure_sample_buffer[s_acq_pressure_latest_position], sizeof(CetiPressureSample));
    return;
}

void acq_pressure_init(void) {
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
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    PRESSURE_htim.Instance = TIM3;
    PRESSURE_htim.Init.Prescaler = 64000 - 1;
    PRESSURE_htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    PRESSURE_htim.Init.Period = (2500 - 1);
    PRESSURE_htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    PRESSURE_htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    ret |= HAL_TIM_Base_Init(&PRESSURE_htim);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    ret |= HAL_TIM_ConfigClockSource(&PRESSURE_htim, &sClockSourceConfig);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    ret |= HAL_TIMEx_MasterConfigSynchronization(&PRESSURE_htim, &sMasterConfig);
    if (HAL_OK != ret) {
        // ToDo: handle errors
    } 
    MX_TIM3_Init();

    // enable timer and interrupts
    __HAL_RCC_TIM3_CLK_ENABLE();
    HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    // setup timer callback
    HAL_TIM_RegisterCallback(&PRESSURE_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, __acq_pressure_timer_complete_cb);
    return;
}

/// @brief Start Pressure sensor acquisition
/// @param  
void acq_pressure_start(void) {
    HAL_TIM_Base_Start_IT(&PRESSURE_htim);
}

/// @brief Stop pressure sensor acquisition
/// @param  
void acq_pressure_stop(void) {
    HAL_TIM_Base_Stop_IT(&PRESSURE_htim);
}

/// @brief Deinitialize 
/// @param  
void acq_pressure_disable(void) {
    acq_pressure_stop();
    HAL_TIM_UnRegisterCallback(&PRESSURE_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID);
    HAL_TIM_Base_DeInit(&PRESSURE_htim);
}

void acq_pressure_flush(void) {
}

CetiPressureSample *acq_pressure_get_latest(void) {
    uint16_t latest_position = s_acq_pressure_write_position;
    if (0 == latest_position) {
        latest_position = (ACQ_PRESSURE_BUFFER_SIZE);
    } 
    latest_position -= 1;
    return &pressure_sample_buffer[s_acq_pressure_write_position];
}
