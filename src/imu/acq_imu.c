/*****************************************************************************
 *   @file      imu/acq_imu.c
 *   @brief     IMU sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *   @note      based on example from https://github.com/ceva-dsp/sh2-demo-nucleo
 *****************************************************************************/
#include "sh2.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"

#include "main.h"
#include "spi.h"

#include "timing.h"

#include <string.h>

#define BNO08X_HDR_LEN (4)

static uint8_t txBuf[SH2_HAL_MAX_TRANSFER_OUT];
static uint8_t rxBuf[SH2_HAL_MAX_TRANSFER_IN];

static uint32_t acq_imu_get_time_us(sh2_Hal_t *self);
static void acq_imu_spi_close(sh2_Hal_t *self);
static int acq_imu_spi_open(sh2_Hal_t *self);
static int acq_imu_spi_hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t);
static int acq_imu_spi_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);

typedef enum {
    SPI_INIT,
    SPI_DUMMY,
    SPI_IDLE,
    SPI_RD_HDR,
    SPI_RD_BODY,
    SPI_WRITE
} SpiState;

static int inReset = 1;
static int s_spi_is_open = 0;
static volatile uint32_t s_rx_timestamp_us;
static volatile uint32_t s_rx_buf_len;
static volatile uint32_t s_tx_buf_len;
static volatile int s_rx_ready;
static volatile SpiState s_spi_state = SPI_INIT;
static sh2_Hal_t bno08x = {
        .getTimeUs = acq_imu_get_time_us,
        .close = acq_imu_spi_close,
        .open = acq_imu_spi_open,
        .read = acq_imu_spi_hal_read,
        .write = acq_imu_spi_hal_write,
};

/*******************************************************************************
 * SPI HW Control Methods
 */
static void csn(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_NCS_GPIO_Output_GPIO_Port, IMU_NCS_GPIO_Output_Pin, state);
}
 
static void ps0_waken(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_PS0_GPIO_Output_GPIO_Port, IMU_PS0_GPIO_Output_Pin, state);
}

static void rstn(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_NRESET_GPIO_Output_GPIO_Port, IMU_NRESET_GPIO_Output_Pin, state);
}

/*******************************************************************************
 * SPI Interrupt Callback Methods
 */

static void acq_imu_start_spi_transfer(void) {
    if (!((s_spi_state == SPI_IDLE) && (s_rx_buf_len == 0) && s_rx_ready)) {
        return; // spi is busy
    }
    s_rx_ready = 0;
    csn(GPIO_PIN_RESET); 
    if (s_tx_buf_len > 0) {
        s_spi_state = SPI_WRITE;
        HAL_SPI_TransmitReceive_IT(&IMU_hspi, txBuf, rxBuf, s_tx_buf_len);
        // Deassert Wake
        ps0_waken(GPIO_PIN_SET);
    } else {
        s_spi_state = SPI_RD_HDR;
        HAL_SPI_Receive_IT(&IMU_hspi, rxBuf, BNO08X_HDR_LEN);
    }

}

void acq_imu_spi_complete_callback(void) {
    // Get length of payload avaiable
    uint16_t rxLen = ((uint16_t)(rxBuf[1] & ~0x80) << 8) | (uint16_t)rxBuf[0];

    rxLen = (rxLen > sizeof(rxBuf)) ? sizeof(rxBuf) : rxLen;
    switch (s_spi_state) {
        case SPI_DUMMY:
            s_spi_state = SPI_IDLE;
            break;

        case SPI_RD_HDR:
            if (rxLen > BNO08X_HDR_LEN) {
                s_spi_state = SPI_RD_BODY;
                HAL_SPI_Receive_IT(&IMU_hspi, rxBuf + BNO08X_HDR_LEN, rxLen - BNO08X_HDR_LEN);
                break;
            } 
            csn(GPIO_PIN_SET);
            s_rx_buf_len = 0;
            s_spi_state = SPI_IDLE;
            acq_imu_start_spi_transfer();
            break;

        case SPI_RD_BODY:
            csn(GPIO_PIN_SET);
            s_rx_buf_len = rxLen;
            s_spi_state = SPI_IDLE;
            acq_imu_start_spi_transfer();
            break;           
        
        case SPI_WRITE:
            csn(GPIO_PIN_SET);
            s_rx_buf_len = (s_tx_buf_len < rxLen) ? s_tx_buf_len : rxLen;
            s_tx_buf_len = 0;
            s_spi_state = SPI_IDLE;
            acq_imu_start_spi_transfer();
            break;

        default:
            break;
    }

}

void acq_imu_EXTI_Callback(void) {
    s_rx_timestamp_us = timing_get_time_since_on_us();
    inReset = 0;
    s_rx_ready = 1;
    acq_imu_start_spi_transfer();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef * hspi) {
    acq_imu_spi_complete_callback();
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef * hspi) {
    acq_imu_spi_complete_callback();
}

static void acq_imu_disable_interrupts(void) {
    HAL_NVIC_DisableIRQ(IMU_NINT_GPIO_EXTI10_EXTI_IRQn);
    HAL_NVIC_DisableIRQ(SPI1_IRQn);
}

static void acq_imu_enable_interrupts(void) {
    HAL_NVIC_EnableIRQ(IMU_NINT_GPIO_EXTI10_EXTI_IRQn);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
}

static void usDelay(uint32_t delay) {
	volatile uint32_t now = timing_get_time_since_on_us();
	uint32_t start = now;
	while ((now - start) < delay) {
		now = timing_get_time_since_on_us();
	}
}

/*******************************************************************************
 * SH2 SPI HAL Methods
 */

static uint32_t acq_imu_get_time_us(sh2_Hal_t *self) {
    return timing_get_time_since_on_us();
}

static int acq_imu_spi_open(sh2_Hal_t *self) {
    if (s_spi_is_open) {
        return SH2_ERR; // can't open another instance
    }
    s_spi_is_open = 1;
    
    
    // initialize spi hardware
    MX_SPI1_Init();

    // hold in reset
    rstn(GPIO_PIN_RESET);

    // deassert CSN
    csn(GPIO_PIN_SET);

    s_rx_buf_len = 0;
    s_tx_buf_len = 0;
    s_rx_ready = 0;

    inReset = 1;

    /* Transmit dummy packet to establish SPI connection*/
    s_spi_state = SPI_DUMMY;
    const uint8_t dummyTx[1] = {0xAA};
    HAL_SPI_Transmit(&IMU_hspi, dummyTx, sizeof(dummyTx), 2);
    s_spi_state = SPI_IDLE;

    usDelay(10000);

    ps0_waken(GPIO_PIN_SET);
    rstn(GPIO_PIN_SET);
    
    acq_imu_enable_interrupts();
    usDelay(2000000);

    return SH2_OK;
}

static void acq_imu_spi_close(sh2_Hal_t *self) {
    acq_imu_disable_interrupts();

    s_spi_state = SPI_INIT;

    rstn(GPIO_PIN_RESET);

    csn(GPIO_PIN_SET);

    HAL_SPI_DeInit(&IMU_hspi);

    s_spi_is_open = 0;
    return;
}

static int acq_imu_spi_hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t) {
    int retval = 0;

    // If there is received data available...
    if (s_rx_buf_len == 0) {
        return 0;
    }
    
    if (len < s_rx_buf_len) {
        // Clear rxBuf so we can receive again
        s_rx_buf_len = 0;
        // Now that rxBuf is empty, activate SPI processing to send any
        // potential write that was blocked.
        acq_imu_disable_interrupts();
        acq_imu_start_spi_transfer();
        acq_imu_enable_interrupts();
        return SH2_ERR_BAD_PARAM;
    }


    // And if the data will fit in this buffer...
    // Copy data to the client buffer
    memcpy(pBuffer, rxBuf, s_rx_buf_len);
    retval = s_rx_buf_len;

    // Set timestamp of that data
    *t = s_rx_timestamp_us;

    // Clear rxBuf so we can receive again
    s_rx_buf_len = 0;

    // Now that rxBuf is empty, activate SPI processing to send any
    // potential write that was blocked.
    acq_imu_disable_interrupts();
    acq_imu_start_spi_transfer();
    acq_imu_enable_interrupts();
    

    return retval;
}

static int acq_imu_spi_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    int retval = SH2_OK;

    // Validate parameters
    if ((self == 0) || (len > sizeof(txBuf)) ||
        ((len > 0) && (pBuffer == 0)))
    {
        return SH2_ERR_BAD_PARAM;
    }

    // If tx buffer is not empty, return 0
    if (s_tx_buf_len != 0) {
        return 0;
    }
    
    // Copy data to tx buffer
    memcpy(txBuf, pBuffer, len);
    s_tx_buf_len = len;
    retval = len;

    // // disable SH2 interrupts for a moment
    acq_imu_disable_interrupts();

    // // Assert Wake
    ps0_waken(GPIO_PIN_RESET);

    // // re-enable SH2 interrupts.
    acq_imu_enable_interrupts();

    return retval;
}


/*******************************************************************************
 * High-level sensor control
 */
#define ACQ_IMU_SENSOR_BUFFER_LENGTH (1000)
sh2_SensorValue_t s_acq_imu_sensor_value_buffer[ACQ_IMU_SENSOR_BUFFER_LENGTH];
static volatile size_t s_acq_imu_sensor_write_position = 0;
static volatile size_t s_acq_imu_sensor_read_position = 0;

void acq_imu_sensor_callback(void *cookie, sh2_SensorEvent_t *pEvent) {
    int status;

    // ToDo: decode based on event type

    // decode event into buffer
    status = sh2_decodeSensorEvent(&s_acq_imu_sensor_value_buffer[s_acq_imu_sensor_write_position], pEvent);
    if (status != SH2_OK) {
        return; // unable to decode report
    }
    size_t next_pos = (s_acq_imu_sensor_write_position + 1) % ACQ_IMU_SENSOR_BUFFER_LENGTH;
    if (next_pos == s_acq_imu_sensor_read_position) {
        // ToDo: Handle overflow
    }
    s_acq_imu_sensor_write_position = next_pos;

}

const struct {
    int sensor_id;
    sh2_SensorConfig_t config;
} s_sensor_config[] = {
    {SH2_ACCELEROMETER, {.reportInterval_us = 20000}},
    {SH2_GYROSCOPE_CALIBRATED, {.reportInterval_us = 20000} },
    {SH2_MAGNETIC_FIELD_CALIBRATED, {.reportInterval_us = 20000}},
    {SH2_ROTATION_VECTOR, {.reportInterval_us = 50000}},
};

const sh2_Quaternion_t s_imu_reorientation_quat = {
    .x = 0.0,
    .y = 0.0,
    .z = 0.0,
    .w = 0.0,
};


void acq_imu_init(void) {
    sh2_ProductIds_t pid;
    acq_imu_disable_interrupts();
	__HAL_RCC_SPI3_CLK_ENABLE();
	MX_SPI1_Init();

    int status = sh2_open(&bno08x, NULL, NULL);
    if (status != SH2_OK) {
        //ToDo: sh2 error handling
    }
    sh2_setSensorCallback(acq_imu_sensor_callback, NULL);

    // ToDo: get product id to verify sensor
    status = sh2_getProdIds(&pid);
    if (status != SH2_OK) {
        // ToDo: sh2 error handling
    }

    // ToDo: configure IMU orientation to tag frame
    // sh2_setReorientation(&s_imu_reorientation_quat);
}

void acq_imu_start(void) {
    for (int i = 0; i < sizeof(s_sensor_config)/ sizeof(s_sensor_config[0]); i++) {
        int status = sh2_setSensorConfig(s_sensor_config[i].sensor_id, &s_sensor_config[i].config);
        if (status != 0) {
            // ToDo: report error
        }
    }
}

void acq_imu_task(void) {
    // process events
    sh2_service();
}
