/*****************************************************************************
 *   @file      imu/acq_imu.c
 *   @brief     IMU sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *   @note      based on example from https://github.com/ceva-dsp/sh2-demo-nucleo
 *****************************************************************************/
#include "acq_imu.h"

#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"

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
static void __acq_imu_init_spi(void);

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
__attribute__((no_instrument_function))
static void csn(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_NCS_GPIO_Output_GPIO_Port, IMU_NCS_GPIO_Output_Pin, state);
}

__attribute__((no_instrument_function))
static void ps0_waken(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_PS0_GPIO_Output_GPIO_Port, IMU_PS0_GPIO_Output_Pin, state);
}

__attribute__((no_instrument_function))
static void rstn(GPIO_PinState state) {
    HAL_GPIO_WritePin(IMU_NRESET_GPIO_Output_GPIO_Port, IMU_NRESET_GPIO_Output_Pin, state);
}

/*******************************************************************************
 * SPI Interrupt Callback Methods
 */

 __attribute__((no_instrument_function))
static void acq_imu_start_spi_transfer(void) {
    if ((s_spi_state != SPI_IDLE) || (s_rx_buf_len != 0)) {
        return; // spi is busy
    }

    // Check NINT pin directly — edge-triggered EXTI misses assertions
    // when NINT is already low (multiple packets queued by BNO08x)
    if (!s_rx_ready
        && (HAL_GPIO_ReadPin(IMU_NINT_GPIO_EXTI10_GPIO_Port,
                             IMU_NINT_GPIO_EXTI10_Pin) == GPIO_PIN_RESET)) {
        s_rx_ready = 1;
    }

    if (!s_rx_ready) {
        return;
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

__attribute__((no_instrument_function))
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
    HAL_PWR_DisableSleepOnExit();
}

__attribute__((no_instrument_function))
void acq_imu_EXTI_Callback(void) {
    s_rx_timestamp_us = timing_get_time_since_on_us();
    inReset = 0;
    s_rx_ready = 1;
    acq_imu_start_spi_transfer();
    // If SPI couldn't start because rxBuf still has unprocessed data from a
    // prior transfer that completed while the main loop was already running
    // (so DisableSleepOnExit had no effect), wake the main loop now so it
    // can call sh2_service() and drain rxBuf.
    if (s_rx_buf_len != 0) {
        HAL_PWR_DisableSleepOnExit();
    }
}

__attribute__((no_instrument_function))
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    acq_imu_spi_complete_callback();
}

__attribute__((no_instrument_function))
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
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
    uint64_t now = rtc_get_epoch_us();
    uint64_t start = now;
    while ((now - start) < delay) {
        now = rtc_get_epoch_us();
    }
}

/*******************************************************************************
 * SH2 SPI HAL Methods
 */
static uint8_t s_epoch_set = 0;
static uint64_t s_epoch_diff_us = 0;
/// @brief imu packet timestamping method
/// @param self pointer to sh2 object
/// @return timestamp in microseconds
/// @note must be able to call inside of interrupt
/// @note since the library expects uint32_t for us time keeping and then
/// internally tracks the number of rollovers, the timer has an additional 
/// timestamp must be captured and added to the 
__attribute__((no_instrument_function))
static uint32_t acq_imu_get_time_us(sh2_Hal_t *self) {
    if (!s_epoch_set) {
        uint64_t ts = (uint64_t)timing_get_time_since_on_us();
        s_epoch_diff_us = rtc_get_epoch_us() - ts;
        s_epoch_set = 1;
        return ts;
    }
    return timing_get_time_since_on_us();
    
}

/// @brief initializes imu hardware and opens communication with spi bus
/// @param self pointer to sh2 object
/// @return SH2_OK on success
static int acq_imu_spi_open(sh2_Hal_t *self) {
    if (s_spi_is_open) {
        return SH2_ERR; // can't open another instance
    }
    s_spi_is_open = 1;


    // hold in reset
    rstn(GPIO_PIN_RESET);

    // deassert CSN
    csn(GPIO_PIN_SET);

    // initialize spi hardware
    __acq_imu_init_spi();

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

    return SH2_OK;
}

/// @brief closes spi bus and deinitializes imu hardware
/// @param self pointer to sh2 object
static void acq_imu_spi_close(sh2_Hal_t *self) {
    acq_imu_disable_interrupts();

    s_spi_state = SPI_INIT;

    rstn(GPIO_PIN_RESET);

    csn(GPIO_PIN_SET);

    HAL_SPI_DeInit(&IMU_hspi);

    s_spi_is_open = 0;
    return;
}

/// @brief spi method sh2_Hal_t object calls inorder to read data from sensor
/// @param self pointer to sh2 object
/// @param pBuffer pointer to receiving buffer
/// @param len expected number of bytes to receive
/// @param t pointer to packet timestamp
/// @return number of bytes received
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

/// @brief spi method sh2_Hal_t object calls inorder to write data to sensor
/// @param self pointer to sh2 object
/// @param pBuffer pointer to data to transmit to sensor
/// @param len length of transmission
/// @return actual number of bytes transmitted
static int acq_imu_spi_hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    int retval = SH2_OK;

    // Validate parameters
    if ((self == 0) || (len > sizeof(txBuf)) ||
        ((len > 0) && (pBuffer == 0))) {
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


static struct {
    int sensor_id;
    sh2_SensorConfig_t config;
} s_sensor_config[IMU_SENSOR_COUNT] = {
    [IMU_SENSOR_ACCELEROMETER] = {SH2_ACCELEROMETER, {.reportInterval_us = 20000}},
    [IMU_SENSOR_GYROSCOPE] = {SH2_GYROSCOPE_CALIBRATED, {.reportInterval_us = 20000}},
    [IMU_SENSOR_MAGNETOMETER] = {SH2_MAGNETIC_FIELD_CALIBRATED, {.reportInterval_us = 20000}},
    [IMU_SENSOR_ROTATION] = {SH2_ROTATION_VECTOR, {.reportInterval_us = 50000}},
};

static ImuCallback s_callback[] ={
    [IMU_SENSOR_ACCELEROMETER] = NULL,
    [IMU_SENSOR_GYROSCOPE] = NULL,
    [IMU_SENSOR_MAGNETOMETER] = NULL,
    [IMU_SENSOR_ROTATION] = NULL,
};

sh2_SensorValue_t s_quat_sample;
sh2_SensorValue_t s_accel_sample;
sh2_SensorValue_t s_gyro_sample;
sh2_SensorValue_t s_mag_sample;

/// @brief callback callled by sh2 library when an imu event is received on 
/// the corresponding spi bus. 
/// @param cookie 
/// @param pEvent 
void acq_imu_sensor_callback(void *cookie, sh2_SensorEvent_t *pEvent) {
    int status;

    // correct timestamp to be epoch time
    pEvent->timestamp_uS += s_epoch_diff_us;

    switch (pEvent->reportId) {
        case SH2_ACCELEROMETER:
            status = sh2_decodeSensorEvent(&s_accel_sample, pEvent);
            if (NULL != s_callback[IMU_SENSOR_ACCELEROMETER]) {
                s_callback[IMU_SENSOR_ACCELEROMETER](&s_accel_sample);
            }
            break;

        case SH2_GYROSCOPE_CALIBRATED:
            status = sh2_decodeSensorEvent(&s_gyro_sample, pEvent);
            if (NULL != s_callback[IMU_SENSOR_GYROSCOPE]) {
                s_callback[IMU_SENSOR_GYROSCOPE](&s_gyro_sample);
            }
            break;
        
        case SH2_MAGNETIC_FIELD_CALIBRATED:
            status = sh2_decodeSensorEvent(&s_mag_sample, pEvent);
            if (NULL != s_callback[IMU_SENSOR_MAGNETOMETER]) {
                s_callback[IMU_SENSOR_MAGNETOMETER](&s_mag_sample);
            }
            break;

        case SH2_ROTATION_VECTOR:
            status = sh2_decodeSensorEvent(&s_quat_sample, pEvent);
            if (NULL != s_callback[IMU_SENSOR_ROTATION]) {
                s_callback[IMU_SENSOR_ROTATION](&s_quat_sample);
            }
            break;

        default:
            break;
    }
}

/// @brief initializes spi hardware associated with imu
/// @param  
static void __acq_imu_init_spi(void){
    SPI_AutonomousModeConfTypeDef HAL_SPI_AutonomousMode_Cfg_Struct = {0};

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 0x7;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
    hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
    hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        //Error_Handler();
    }
    HAL_SPI_AutonomousMode_Cfg_Struct.TriggerState = SPI_AUTO_MODE_DISABLE;
    HAL_SPI_AutonomousMode_Cfg_Struct.TriggerSelection = SPI_GRP1_GPDMA_CH0_TCF_TRG;
    HAL_SPI_AutonomousMode_Cfg_Struct.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
    if (HAL_SPIEx_SetConfigAutonomousMode(&hspi1, &HAL_SPI_AutonomousMode_Cfg_Struct) != HAL_OK)
    {
        // Error_Handler();
    }
}

/******BLOCKING MODE TESTS*************************************************************************/

/// @brief initialize imu hardware
/// @param  
void acq_imu_init(void) {
    acq_imu_disable_interrupts();
    __HAL_RCC_SPI1_CLK_ENABLE();

    int status = sh2_open(&bno08x, NULL, NULL);
    if (status != SH2_OK) {
        // ToDo: sh2 error handling
    }
    sh2_setSensorCallback(acq_imu_sensor_callback, NULL);
    // get product id to verify sensor

    acq_imu_enable_interrupts();
//    HAL_Delay(20);

    // Configure IMU orientation to tag frame
}

/// @brief start data capture of specific imu sensor
/// @param sensor sensor to start
/// @param time_interval_us time interval between consecutive sensors
/// @return 
int acq_imu_start_sensor(ImuSensor sensor, uint32_t time_interval_us) {
    s_sensor_config[sensor].config.reportInterval_us = time_interval_us;
    s_sensor_config[sensor].config.batchInterval_us = 50000; // grab reports every second
    return sh2_setSensorConfig(s_sensor_config[sensor].sensor_id, &s_sensor_config[sensor].config);

}

/// @brief stop acquisition of specific imu sensor data 
/// @param sensor sensor to stop
/// @return 
int acq_imu_stop_sensor(ImuSensor sensor) {
    return acq_imu_start_sensor(sensor, 0);
}


/// @brief stop acquisition of all imu sensor data
/// @param  
void acq_imu_stop_all(void) {
    for (int i = 0; i < IMU_SENSOR_COUNT; i++) {
        s_sensor_config[i].config.reportInterval_us  = 0;
        int status = sh2_setSensorConfig(s_sensor_config[i].sensor_id, &s_sensor_config[i].config);
        if (status != 0) {
            // ToDo: report error
        }
    }
    #warning ToDo: implement acq_imu_stop()
}

/// @brief stops imu data acquisition and deinitalizes imu hardware
/// @param  
void acq_imu_deinit(void) {
    acq_imu_stop_all();
    #warning ToDo: implement acq_imu_deinit()
}

/// @brief imu acquisition task
/// @param 
/// @note call periodically in main loop
void acq_imu_task(void) {
    // process events
    sh2_service();
}

/// @brief register a call back for a give supported sensor type
/// @param sensor_kind sensor to register callback to
/// @param callback callback method
void acq_imu_register_callback(ImuSensor sensor_kind, ImuCallback callback) {
    s_callback[sensor_kind] = callback;
}

/// @brief Returns latest rotation estimation
/// @param dst 
void acq_imu_get_rotation(sh2_SensorValue_t * dst) {
    *dst = s_quat_sample;
}

/// @brief Returns latest accelerometer sample
/// @param dst 
void acq_imu_get_accelerometer(sh2_SensorValue_t * dst) {
    *dst = s_accel_sample;
}

/// @brief Returns latest gyroscope sample
/// @param dst 
void acq_imu_get_gyroscope(sh2_SensorValue_t * dst) {
    *dst = s_gyro_sample;
}

/// @brief Returns latest magnetometer sample
/// @param dst 
void acq_imu_get_magnetometer(sh2_SensorValue_t * dst) {
    *dst = s_mag_sample;
}
