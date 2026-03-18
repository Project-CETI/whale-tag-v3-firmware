/*****************************************************************************
 *   @file   acq_ecg.h
 *   @brief  ecg acquisition code. Note this code just gathers ecg data
 *           into RAM, but does not perform any analysis, transformation, or
 *           storage of said data.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/

#include "acq_ecg.h"
#include "ads1219.h"
#include "timing.h"

#include "i2c.h"
#include "main.h"

extern I2C_HandleTypeDef ECG_hi2c;

typedef struct {
    uint64_t timestamp;
    int32_t value;
    uint8_t lod_p;
    uint8_t lod_n;
} EcgSample;

EcgSample ecg_sample_buffer[2000] = {};
static volatile int ecg_sample_write_position = 0;
static int ecg_sample_read_position = 0;

static uint8_t s_waiting_for_sample = 0;

static void acq_ecg_acquire_sample_callback(void) {
    s_waiting_for_sample = 0;
}

// ToDo: implement ecg EXTI callback to store sample into sample buffer
void acq_ecg_EXTI_Callback(void) {
    EcgSample *curent_sample = &ecg_sample_buffer[ecg_sample_write_position];

    // acquire sample
    if (s_waiting_for_sample) {
        __NOP();
        // ToDo: throw error as i2c read still occuring
    }

    s_waiting_for_sample = 1;
    curent_sample->timestamp = rtc_get_epoch_us();
    ads1219_read_data_raw_it(&curent_sample->value, acq_ecg_acquire_sample_callback);
    curent_sample->lod_p = HAL_GPIO_ReadPin(ECG_LOD_P_GPIO_Input_GPIO_Port, ECG_LOD_P_GPIO_Input_Pin);
    curent_sample->lod_n = HAL_GPIO_ReadPin(ECG_LOD_N_GPIO_Input_GPIO_Port, ECG_LOD_N_GPIO_Input_Pin);

    // increment sample_position
    ecg_sample_write_position = (ecg_sample_write_position + 1) % 2000;
    if (ecg_sample_write_position == ecg_sample_read_position) {
        // ToDo: handle buffer overflow
    }
}

void acq_ecg_disable(void) {
    /* disable acq_ecg interrupt */
    HAL_NVIC_DisableIRQ(EXTI2_IRQn);
    // ToDo: reconfigure ECG_NDRDY as analog to save power

    /* shutdown ADC */
    HAL_GPIO_WritePin(ECG_NSD_GPIO_Output_GPIO_Port, ECG_NSD_GPIO_Output_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ECG_ADC_NRSET_GPIO_Output_GPIO_Port, ECG_ADC_NRSET_GPIO_Output_Pin, GPIO_PIN_RESET);

    /* ToDo: Disable i2c2 peripheral to save power */
}

void acq_ecg_enable(void) {
    // ToDo: error reporting
    /* turn on ADC */
    HAL_GPIO_WritePin(ECG_NSD_GPIO_Output_GPIO_Port, ECG_NSD_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ECG_ADC_NRSET_GPIO_Output_GPIO_Port, ECG_ADC_NRSET_GPIO_Output_Pin, GPIO_PIN_SET);

    // enable i2c bus
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPINIT_CB_ID, HAL_I2C_MspInit);
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPDEINIT_CB_ID, HAL_I2C_MspDeInit);
    MX_I2C2_Init();

    // ToDo: enable lead off detection

    // configure adc
    ads1219_reset();
    // Send a reset command
    const ADS1219_Configuration adc_config = {
        .vref = ADS1219_VREF_EXTERNAL,
        .gain = ADS1219_GAIN_ONE,
        .data_rate = ADS1219_DATA_RATE_1000,
        .mode = ADS1219_MODE_CONTINUOUS,
        .mux = ADS1219_MUX_SINGLE_0,
    };
    ads1219_apply_configuration(&adc_config);

    /* Configure GPIO pin : ECG_ADC_NDRDY_GPIO_Input_Pin */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ECG_ADC_NDRDY_GPIO_Input_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ECG_ADC_NDRDY_GPIO_Input_GPIO_Port, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(ECG_ADC_NDRDY_GPIO_Input_EXTI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ECG_ADC_NDRDY_GPIO_Input_EXTI_IRQn);
}

void acq_ecg_start(void) {
    /* start continuous conversion */
    ads1219_start();
}

void acq_ecg_task(void) {}
