/*****************************************************************************
 *   @file      gps.c
 *   @brief     This code handles communication with the GPS module (uBlox
 *              M10s)
 *   @project   Project CETI
 *   @date      12/10/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "gps.h"

#include "error.h"
#include "m10s.h"
#include "main.h"

#include <math.h>
#include <string.h>

/* Extern Declarations */
extern UART_HandleTypeDef GPS_huart;

// === PRIVATE DEFINES ===
#define NMEA_START_CHAR '$'
#define NMEA_END_CHAR '\r'
#define NMEA_MAX_SIZE (82)

#define GPS_BUFFER_SIZE (NMEA_MAX_SIZE + 1)
#define GPS_BUFFER_COUNT (1 + sizeof(gps_raw_buffer) / NMEA_MAX_SIZE)

#define GPS_BUFFER_VALID_START (1 << 0)

// === PRIVATE TYPEDEFS ===
typedef enum {
    GPS_STATE_OFF,
    GPS_STATE_ON,
    GPS_STATE_STANDBY,
} GpsState;

// === PRIVATE VARIABLES ===
static GpsState s_gps_state = GPS_STATE_OFF;

static int rx_buffer_index = 0;

// === PRIVATE METHODS ===

// === PUBLIC METHODS ===
static uint8_t gps_raw_buffer[2 * GPS_BULK_TRANSFER_SIZE];
static volatile uint16_t gps_raw_buffer_read_position = 0;
static volatile uint8_t gps_bulk_write_position = 0;
static volatile uint8_t gps_bulk_read_position = 0;

static uint8_t gps_buffer[GPS_BUFFER_COUNT][GPS_BUFFER_SIZE];
static volatile uint8_t gpsBuffer_write_index = 0;
static volatile uint8_t gpsBuffer_read_index = 0;
int gpsBuffer_overflow = 0;

static void (*s_msg_complete_callback)(const uint8_t * msg_ptr, uint16_t len) = NULL;
static void (*s_raw_rx_callback)(const uint8_t * raw_ptr, uint16_t len) = NULL;

/// @brief seperates a set of bytes into gps nmea messages
/// @param bytes pointer to bytes to process
/// @param byte_len number of bytes to process
static void __gps_rx_process_bytes(const uint8_t *bytes, uint16_t position) {
    static uint8_t partial_position = 0;
    uint8_t *current_sentence = gps_buffer[gpsBuffer_write_index];
    /* call message received callback */
    if (NULL != s_raw_rx_callback) {
            s_raw_rx_callback(bytes, position);
    }
    
    /* seperate bytes into nmea_messages */
    for (int i = 0; i < position; i++) {
        if (0 == partial_position) { // nmea not started
            if (NMEA_START_CHAR == bytes[i]) {
                current_sentence[0] = NMEA_START_CHAR;
                partial_position = 1;
            }
        } else {
            // copy sentence to buffer
            current_sentence[partial_position++] = bytes[i];

            // end sentence
            if (('\n' == bytes[i])) {
                current_sentence[partial_position] = 0;
                gpsBuffer_write_index = (gpsBuffer_write_index + 1) % GPS_BUFFER_COUNT;
                if (gpsBuffer_write_index == gpsBuffer_read_index) {
                    gpsBuffer_overflow = 1;
                }

                /* call message received callback */
                if (NULL != s_msg_complete_callback) {
                    s_msg_complete_callback(current_sentence, partial_position);
                }
                
                /* setup next message */
                current_sentence = gps_buffer[gpsBuffer_write_index];
                partial_position = 0;

            }
        }
    }
}

/// @brief UART interrupt callback
/// @param huart
/// @param pos
void gps_rx_callback(UART_HandleTypeDef *huart, uint16_t pos) {
    if ((pos < gps_raw_buffer_read_position)) {
        /* process to end of buffer */
        __gps_rx_process_bytes(&gps_raw_buffer[gps_raw_buffer_read_position], sizeof(gps_raw_buffer) - gps_raw_buffer_read_position);
        gps_raw_buffer_read_position = 0;
    }
    __gps_rx_process_bytes(&gps_raw_buffer[gps_raw_buffer_read_position], (pos - gps_raw_buffer_read_position));
    gps_raw_buffer_read_position = pos;

    // half transfer boundry cross
    if ((HAL_UART_RXEVENT_TC == huart->RxEventType) || (HAL_UART_RXEVENT_HT == huart->RxEventType)) {
        uint8_t next_bulk_write_position = gps_bulk_write_position ^ 1;
        if (next_bulk_write_position == gps_bulk_read_position) { // report buffer overflow
            error_queue_push(CETI_ERROR(ERR_SUBSYS_GPS, ERR_TYPE_DEFAULT, ERR_BUFFER_OVERFLOW));
            gps_bulk_read_position ^= 1; // advance read position as well
        }
        gps_bulk_write_position = next_bulk_write_position;
    }
}

/// @brief check if gps message queue is empty
/// @param
/// @return 1 if queue is empty, 0 otherwise
uint8_t gps_queue_is_empty(void) {
    return (gpsBuffer_read_index == gpsBuffer_write_index);
}

/// @brief returns pointer to latest gps NMEA message if any
/// @param
/// @return pointer to nest NMEA message, NULL if no NMEA messages to parse.
const uint8_t *gps_pop_sentence(void) {
    if (gps_queue_is_empty()) {
        return NULL;
    }

    const uint8_t *return_buffer = gps_buffer[gpsBuffer_read_index];
    gpsBuffer_read_index = (gpsBuffer_read_index + 1) % GPS_BUFFER_COUNT;
    return return_buffer;
}

/// @brief Turns GPS off (through power FET) and starts buffering thread
void gps_sleep(void) {
    // disable power to gps module
    HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
    s_gps_state = GPS_STATE_OFF;

    // stop the DMA
    HAL_UART_DMAStop(&GPS_huart);
    // Note: the buffers should be checked to be flushed
    //  gpsBuffer_write_index = 0;
    //  gpsBuffer_read_index = 0;
    //  rx_buffer_index= 0;
}

void gps_standby(void) {
    // set module into standby mode
    uint8_t buffer[32] = {};
    uint16_t msg_length = m10s_enter_software_standby_mode(buffer, sizeof(buffer));
    HAL_UART_Transmit(&GPS_huart, buffer, msg_length, 1000);
    s_gps_state = GPS_STATE_STANDBY;

    // stop the DMA
    HAL_UART_DMAStop(&GPS_huart);
}

static void __gps_gpio_init(void) {
    // GPS_PWR_EN
    // GPS_SAFEBOOT_N
    // GPS_NRST
    // GPS_USART1_RX
    // GPS_USART1_TX

    /* GPS_EXT_INT */
    GPIO_InitTypeDef GPIO_InitStruct;

    // toggle external interrupt to exit standby mode
    HAL_GPIO_WritePin(GPS_EXT_INT_GPIO_Input_GPIO_Port, GPS_EXT_INT_GPIO_Input_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = GPS_EXT_INT_GPIO_Input_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPS_EXT_INT_GPIO_Input_GPIO_Port, &GPIO_InitStruct);
}

static HAL_StatusTypeDef __gps_uart_init(void) {
    HAL_StatusTypeDef result = HAL_OK;
    GPS_huart.Instance = USART1;
    GPS_huart.Init.BaudRate = 9600;
    GPS_huart.Init.WordLength = UART_WORDLENGTH_8B;
    GPS_huart.Init.StopBits = UART_STOPBITS_1;
    GPS_huart.Init.Parity = UART_PARITY_NONE;
    GPS_huart.Init.Mode = UART_MODE_TX_RX;
    GPS_huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    GPS_huart.Init.OverSampling = UART_OVERSAMPLING_16;
    GPS_huart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    GPS_huart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    GPS_huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    result = HAL_UART_Init(&GPS_huart);
    if (HAL_OK == result) {
        result = HAL_UARTEx_SetTxFifoThreshold(&GPS_huart, UART_TXFIFO_THRESHOLD_1_8);
    }
    if (HAL_OK == result) {
        result = HAL_UARTEx_SetRxFifoThreshold(&GPS_huart, UART_RXFIFO_THRESHOLD_1_8);
    }
    if (HAL_OK == result) {
        result = HAL_UARTEx_DisableFifoMode(&GPS_huart);
    }
    return result;
}

void gps_init(void) {
    __gps_gpio_init();
    __gps_uart_init();

    uint8_t buffer[512] = {};
    uint16_t msg_length = m10s_disable_i2c_output(buffer, sizeof(buffer));
    HAL_UART_Transmit(&GPS_huart, buffer, msg_length, 1000);

    msg_length = m10s_disable_spi_output(buffer, sizeof(buffer));
    HAL_UART_Transmit(&GPS_huart, buffer, msg_length, 1000);
}

/// @brief wakes up gps module and begins receiving NMEA message
/// @param
void gps_wake(void) {
    if (GPS_STATE_ON == s_gps_state) {
        /* Nothing to do */
        return;
    }

    // initiate UART DMA
    gpsBuffer_write_index = 0;
    gpsBuffer_read_index = 0;
    rx_buffer_index = 0;
    // HAL_UART_Transmit(&GPS_huart, (uint8_t *)"$PUBX,00*33\r\n", strlen("$PUBX,00*33\r\n"), 1000);
    HAL_UART_DMAStop(&GPS_huart);
    // Disable receiver to prevent overrun while clearing flags
    ATOMIC_CLEAR_BIT(GPS_huart.Instance->CR1, USART_CR1_RE);
    __HAL_UART_CLEAR_OREFLAG(&GPS_huart);
    __HAL_UART_CLEAR_FEFLAG(&GPS_huart);
    __HAL_UART_CLEAR_NEFLAG(&GPS_huart);
    __HAL_UART_CLEAR_PEFLAG(&GPS_huart);
    __HAL_UART_FLUSH_DRREGISTER(&GPS_huart);
    // Re-enable receiver before starting DMA
    ATOMIC_SET_BIT(GPS_huart.Instance->CR1, USART_CR1_RE);
    HAL_UARTEx_ReceiveToIdle_DMA(&GPS_huart, gps_raw_buffer, sizeof(gps_raw_buffer));

    if (GPS_STATE_OFF == s_gps_state) {
        // Enable power to GPS module and starts buffering thread
        HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_RESET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
    } else if (GPS_STATE_STANDBY == s_gps_state) {
        GPIO_InitTypeDef GPIO_InitStruct;

        // toggle external interrupt to exit standby mode
        HAL_GPIO_WritePin(GPS_EXT_INT_GPIO_Input_GPIO_Port, GPS_EXT_INT_GPIO_Input_Pin, GPIO_PIN_SET);

        GPIO_InitStruct.Pin = GPS_EXT_INT_GPIO_Input_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPS_EXT_INT_GPIO_Input_GPIO_Port, &GPIO_InitStruct);
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPS_EXT_INT_GPIO_Input_GPIO_Port, GPS_EXT_INT_GPIO_Input_Pin, GPIO_PIN_RESET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPS_EXT_INT_GPIO_Input_GPIO_Port, GPS_EXT_INT_GPIO_Input_Pin, GPIO_PIN_SET);
    }

    s_gps_state = GPS_STATE_ON;
}

/// @brief check if gps message queue is empty
/// @param
/// @return 1 if queue is empty, 0 otherwise
uint8_t gps_bulk_queue_is_empty(void) {
    return (gps_bulk_write_position == gps_bulk_read_position);
}

const uint8_t *gps_pop_bulk_transfer(void) {
    if (gps_bulk_queue_is_empty()) {
        return NULL;
    }
    uint8_t nv_read_position = gps_bulk_read_position;
    gps_bulk_read_position = nv_read_position ^ 1;
    return &gps_raw_buffer[nv_read_position * GPS_BULK_TRANSFER_SIZE];
}

/// @brief Set the GPS NMEA message period to 30 seconds
/// @param
void gps_low_data_rate(void) {
    uint8_t buffer[512] = {};
    uint16_t msg_len = m10s_ubx_cfg_set_msg_rate(buffer, sizeof(buffer), 30);
    HAL_UART_Transmit(&GPS_huart, buffer, msg_len, 1000);
}

/// @brief Set the GPS NMEA message period to 1 seconds
/// @param
void gps_high_data_rate(void) {
    uint8_t buffer[512] = {};
    uint16_t msg_len = m10s_ubx_cfg_set_msg_rate(buffer, sizeof(buffer), 1);
    HAL_UART_Transmit(&GPS_huart, buffer, msg_len, 1000);
}

void gps_register_msg_complete_callback(void (*callback)(const uint8_t *, uint16_t)) {
    s_msg_complete_callback = callback;
}

void gps_register_bytes_received_callback(void (*callback)(const uint8_t *, uint16_t)){
    s_raw_rx_callback = callback;
}
