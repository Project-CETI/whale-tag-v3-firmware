/*****************************************************************************
 *   @file      satellite.c
 *   @brief     This code handles communication Arribada ARGOS module
 *   @project   Project CETI
 *   @date      12/10/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "satellite.h"

#include "main.h"

#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef SAT_huart;

#define AT_CMD(cmd, value) "AT+" cmd "=" value "\r\n"

#define LDA2_RCONF "3d678af16b5a572078f3dbc95a1104e7"
#define LDA2L_RCONF "bd176535b394a665bd86f354c5f424fb"
#define VLDA4_RCONF "efd2412f8570581457f2d982e76d44d7"
#define LDK_RCONF "03921fb104b92859209b18abd009de96"

#define SMD_CMD_PING AT_CMD("PING", "?")
#define SMD_CMD_RCONF_LDA2 AT_CMD("RCONF", LDA2_RCONF)
#define SMD_CMD_RCONF_LDA2L AT_CMD("RCONF", LDA2L_RCONF)
#define SMD_CMD_RCONF_VLDA4 AT_CMD("RCONF", VLDA4_RCONF)
#define SMD_CMD_RCONF_LDK AT_CMD("RCONF", LDK_RCONF)
#define SMD_CMD_SAVE_RCONF AT_CMD("SAVE_RCONF", "")
#define SMD_CMD_INIT_MAC_CONFIG AT_CMD("KMAC", "0")
#define SMD_CMD_SAVE_MAC_CONFIG AT_CMD("KMAC", "1")

/******************************************************************************
 *  HARDWARE CODE (replace for given )
 */
static uint8_t satellite_response_buffer[128];
static volatile uint8_t satellite_response_overflowed = 0;
static volatile uint8_t satellite_response_end_position = 0;
static volatile uint8_t satellite_response_ok = 0;
static volatile uint8_t satellite_response_complete = 0;

static inline void __power_en(GPIO_PinState state) {
#ifdef SAT_PWR_EN_GPIO_Output_Pin
    HAL_GPIO_WritePin(SAT_PWR_EN_GPIO_Output_GPIO_Port, SAT_PWR_EN_GPIO_Output_Pin, state);
#endif
}

/// @brief callback to process latest input from UART RX line DMA
/// @param huart uart handle pointer
/// @param position current position in DMA buffer
void satellite_rx_callback(UART_HandleTypeDef *huart, uint16_t position) {
    if (HAL_UART_RXEVENT_IDLE == huart->RxEventType) {
        // response done
        satellite_response_end_position = position;
        satellite_response_complete = 1;

        // unwrap buffer
        if ((position < 5) && (satellite_response_overflowed)) {
            uint8_t tmp_buffer[5];
            memcpy(&tmp_buffer[0], &satellite_response_buffer[sizeof(satellite_response_buffer) - (5 - position)], (5 - position));
            memcpy(&tmp_buffer[(5 - position)], &satellite_response_buffer[0], position);
            satellite_response_ok = (0 == memcmp("+OK\r\n", tmp_buffer, 5));
        } else {
            satellite_response_ok = (0 == memcmp("+OK\r\n", &satellite_response_buffer[position - 5], 5));
        }
    } else if (HAL_UART_RXEVENT_TC == huart->RxEventType) {
        // reached end of buffer, but response is still coming
        satellite_response_overflowed++;
    }
}

/// @brief initializes a read DMA to capture the ARGOS module's response
/// @param
/// @return
static int __satellite_read_dma(void) {
    satellite_response_overflowed = 0;
    satellite_response_ok = 0;
    satellite_response_complete = 0;
    HAL_UART_DMAStop(&SAT_huart);
    return HAL_UARTEx_ReceiveToIdle_DMA(&SAT_huart, satellite_response_buffer, sizeof(satellite_response_buffer));
}

/// @brief Blocking write of satellite command to the ARGOS module
/// @param pCommand pointer to input AT command
/// @param command_len AT command length
/// @return
static int __satellite_write(char *pCommand, uint16_t command_len) {
    return HAL_UART_Transmit(&SAT_huart, (uint8_t *)pCommand, command_len, 1000);
}

/// @brief Waits up to `timeout_ms` for satellite module to respond
/// @param timeout_ms timeout time in millisecond. No timeout if set to 0
/// @return 0 on response received sucessfully, 1 on timeout
static int __satellite_wait_for_response(uint32_t timeout_ms) {
    uint32_t start_ms = HAL_GetTick();
    while ((0 != timeout_ms) && ((HAL_GetTick() - start_ms) < timeout_ms)) {
        if (satellite_response_complete) {
            // end found
            return 0;
        }
        HAL_Delay(1);
    }
    // timeout occured
    return 1;
}

/// @brief Writes an AT command to the ARGOS satellite module and waits up to
///        one second for a response
/// @param pCommand pointer to AT command
/// @param command_len AT command length
/// @return 0 on sucess, 1 on timeout
static int __satellite_write_with_response(char *pCommand, uint16_t command_len) {
    __satellite_read_dma();
    __satellite_write(pCommand, command_len);
    return __satellite_wait_for_response(1000);
}

/// @brief Continually writes an AT command to the ARGOS module every second
/// until an OK response is received
/// @param pCommand pointer to AT command
/// @param command_len  AT command length
static int __satellite_write_until_ok(char *pCommand, uint16_t command_len) {
    do {
        if (0 != __satellite_write_with_response(pCommand, command_len)) {
            return 1; // error
        }
    } while (!satellite_response_ok);
    return 0;
}

/// @brief Confiures system so ARGOS module can properlly interface with an
/// stlink programmer
/// @param
void satellite_wait_for_programming(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable power to module
    __power_en(GPIO_PIN_SET);

    // set NRST as input so that stlink can take control
    GPIO_InitStruct.Pin = SAT_NRST_GPIO_Output_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SAT_NRST_GPIO_Output_GPIO_Port, &GPIO_InitStruct);
}

/******************************************************************************
 *  HIGH LEVEL ARRIBADA CONTROL
 */

/// @brief pings ARGOS module
/// @param
/// @return 1 on success, 0 otherwise
int satellite_ping(void) {
    return (0 == __satellite_write_with_response(SMD_CMD_PING, strlen(SMD_CMD_PING))) && satellite_response_ok;
}

/// @brief waits up to `timeout_ms` for an "OK" response
/// @param timeout_ms timeout in milliseconds, 0 if no timeout
/// @param val
/// @return 1 on sucessful OK, 0 otherwise
int waitForOK(uint32_t timeout_ms, char *val) {
    uint32_t start_tick_ms = HAL_GetTick(); // Get current tick count

    while (1) {
        // Check the desired condition
        if (memcmp(val, "+OK", 3) == 0) // Replace with your actual condition
        {
            return 1; // Condition met
        }

        // Check for timeout
        if ((HAL_GetTick() - start_tick_ms) >= timeout_ms) {
            return 0; // Timeout occurred
        }
    }
}

/// @brief saves radio configuration
/// @param
void resetKMACProfile(void) {
    // Need to set KMAC = 0, wait, and then set KMAC to 1 after a radio config change or reboot of the Arribada module
    __satellite_read_dma();
    __satellite_write(SMD_CMD_INIT_MAC_CONFIG, strlen(SMD_CMD_INIT_MAC_CONFIG));
    if ((0 != __satellite_wait_for_response(5000)) || !satellite_response_ok) {
        return;
    }

    __satellite_read_dma();
    __satellite_write(SMD_CMD_SAVE_MAC_CONFIG, strlen(SMD_CMD_SAVE_MAC_CONFIG));
    if ((0 != __satellite_wait_for_response(5000)) || !satellite_response_ok) {
        return;
    }
}

/// @brief sets and saves radio communication protocol
/// @param protocol `SMD_CMD_RCONF_LDA2`, `SMD_CMD_RCONF_VLDA4`, or `SMD_CMD_RCONF_LDK`
void satellite_configure_radio(RecoveryArgoModulation protocol) {
    do {
        __satellite_read_dma();
        switch (protocol) {
            case ARGOS_MOD_LDA2:
                __satellite_write(SMD_CMD_RCONF_LDA2, strlen(SMD_CMD_RCONF_LDA2));
                break;

            case ARGOS_MOD_VLDA4:
                __satellite_write(SMD_CMD_RCONF_VLDA4, strlen(SMD_CMD_RCONF_VLDA4));
                break;

            case ARGOS_MOD_LDK:
                __satellite_write(SMD_CMD_RCONF_LDK, strlen(SMD_CMD_RCONF_LDK));
                break;

            case ARGOS_MOD_LDA2L:
                __satellite_write(SMD_CMD_RCONF_LDA2L, strlen(SMD_CMD_RCONF_LDA2L));
                break;
        }
    } while ((0 != __satellite_wait_for_response(1000)) || !satellite_response_ok);
    do {
        __satellite_write(SMD_CMD_SAVE_RCONF, strlen(SMD_CMD_SAVE_RCONF));
    } while ((0 != __satellite_wait_for_response(1000)) || !satellite_response_ok);
    resetKMACProfile();
}

/// @brief transmits message package to ARGOS satellites
/// @param message pointer to message
/// @param message_len message length
void satellite_transmit(const char *message, size_t message_len) {
    // ToDo: check message_len based on current RCONF
    char tx_buffer[128] = "AT+TX=";
    uint16_t len = strlen("AT+TX=");
    memcpy(&tx_buffer[len], message, message_len);
    len += message_len;
    tx_buffer[len++] = '\r';
    tx_buffer[len++] = '\n';
    __satellite_write(tx_buffer, len);
}

/// @brief macro to define a generic get AT command function
/// @param cmd AT cmd as string .e.g. `"ID"`
/// @param dst `char *` pointer mutable response destination
/// @param length `uint16_t *` pointer to response length value
/// @param capacity 'uint16_t' capacity of destination buffer
/// @note  it sucks that metaprogramming isn't a thing in C (MSH)
#define __SATELLITE_GET_COMMAND(cmd, dst, length, capacity)                              \
    do {                                                                                 \
        if (length == NULL || dst == NULL) {                                             \
            return 1;                                                                    \
        }                                                                                \
        __satellite_write_with_response("AT+" cmd "=?\r\n", strlen("AT+" cmd "=?\r\n")); \
        if (!satellite_response_ok) {                                                    \
            return 2;                                                                    \
        }                                                                                \
        if (memcmp("+" cmd "=", satellite_response_buffer, strlen("+" cmd "="))) {       \
            return 3;                                                                    \
        }                                                                                \
        uint8_t *start_ptr = &satellite_response_buffer[strlen("+" cmd "=")];            \
        uint8_t *end_ptr = memchr(start_ptr, '\r', satellite_response_end_position);     \
        if (NULL == end_ptr) {                                                           \
            return 4;                                                                    \
        }                                                                                \
        size_t len = end_ptr - start_ptr;                                                \
        if (len > capacity) {                                                            \
            return 5;                                                                    \
        }                                                                                \
        memcpy(dst, start_ptr, len);                                                     \
        *length = len;                                                                   \
        if (len < capacity) {                                                            \
            dst[len] = 0;                                                                \
        }                                                                                \
    } while (0)

/// @brief gets AGROS ID from module
/// @param dst pointer to array output
/// @param length pointer array output length
/// @param capacity output array capacity
/// @return
int satellite_get_id(char dst[ARGOS_ID_LENGTH + 1], uint16_t *length, uint16_t capacity) {
    __SATELLITE_GET_COMMAND("ID", dst, length, capacity);
    return 0;
}

/// @brief gets AGROS MAC address from module
/// @param dst pointer to array output
/// @param length pointer array output length
/// @param capacity output array capacity
/// @return
int satellite_get_mac_address(char dst[ARGOS_MAC_ADDRESS_LENGTH + 1], uint16_t *length, uint16_t capacity) {
    __SATELLITE_GET_COMMAND("ADDR", dst, length, capacity);
    return 0;
}

/// @brief gets current radio configuration from module
/// @param rconf pointer radio modulation scheme output
/// @return 0 on ok
int satellite_get_rconf(RecoveryArgoModulation *rconf) {
    char rconf_string[33] = {0};
    uint16_t response_length = 0;
    __SATELLITE_GET_COMMAND("RCONF", rconf_string, &response_length, sizeof(rconf_string));
    if (0 == memcmp(&rconf_string[response_length - 4], "LDA2", 4)) {
        *rconf = ARGOS_MOD_LDA2;
    } else if (0 == memcmp(&rconf_string[response_length - 5], "VLDA4", 5)) {
        *rconf = ARGOS_MOD_VLDA4;
    } else if (0 == memcmp(&rconf_string[response_length - 3], "LDK", 3)) {
        *rconf = ARGOS_MOD_LDK;
    } else if (0 == memcmp(&rconf_string[response_length - 5], "LDA2L", 5)) {
        *rconf = ARGOS_MOD_LDA2L;
    } else {
        return 2;
    }

    return 0;
}

/// @brief gets AGROS secret key from module
/// @param dst pointer to array output
/// @param length pointer array output length
/// @param capacity output array capacity
/// @return
int satellite_get_secret_key(char dst[ARGOS_SECRET_KEY_LENGTH + 1], uint16_t *length, uint16_t capacity) {
    if (length == NULL || dst == NULL) {
        return 1;
    }
    __satellite_write_with_response("AT+SECKEY=?\r\n", strlen("AT+SECKEY=?\r\n"));
    if (memcmp("+SECKEY=", satellite_response_buffer, strlen("+SECKEY="))) {
        return 3;
    }
    uint8_t *start_ptr = &satellite_response_buffer[strlen("+SECKEY=")];
    uint8_t *end_ptr = memchr(start_ptr, '\r', satellite_response_end_position);
    if (NULL == end_ptr) {
        return 4;
    }
    size_t len = end_ptr - start_ptr;
    if (len > capacity) {
        return 5;
    }
    memcpy(dst, start_ptr, len);
    *length = len;
    if (len < capacity) {
        dst[len] = 0;
    }
    return 0;
}

/// @brief macro to define a generic set AT command function
/// @param cmd AT cmd as string .e.g. `"ID"`
/// @param value_array `const char *` pointer to cmd value
/// @param length `uint16_t` value length
/// @note  it sucks that metaprogramming isn't a thing in C (MSH)
#define __SATELLITE_SET_COMMAND(cmd, value_array, length) \
    do {                                                  \
        char tx_buffer[128] = "AT+" cmd "=";              \
        uint16_t len = strlen("AT+" cmd "=");             \
        memcpy(&tx_buffer[len], value_array, length);     \
        len += length;                                    \
        tx_buffer[len++] = '\r';                          \
        tx_buffer[len++] = '\n';                          \
        __satellite_write_until_ok(tx_buffer, len);       \
    } while (0)

/// @brief sets ARGOS ID on module
/// @param id pointer to ID value
/// @param id_len ID value length
/// @return 0 on success
int satellite_set_id(const char *id, size_t id_len) {
    if (id_len != ARGOS_ID_LENGTH) {
        return 1;
    }
    __SATELLITE_SET_COMMAND("ID", id, ARGOS_ID_LENGTH);
    return 0;
}

/// @brief sets ARGOS MAC address on module
/// @param address pointer to ADDR value
/// @param address_len ADDR value length
/// @return 0 on success
int satellite_set_mac_address(const char address[static ARGOS_MAC_ADDRESS_LENGTH]) {
    __SATELLITE_SET_COMMAND("ADDR", address, ARGOS_MAC_ADDRESS_LENGTH);
    return 0;
}

/// @brief sets ARGOS secret key on module
/// @param address pointer to SECKEY value
/// @param address_len SECKEY value length
/// @return 0 on success
int satellite_set_secret_key(const char secret_key[static ARGOS_SECRET_KEY_LENGTH]) {
    __SATELLITE_SET_COMMAND("SECKEY", secret_key, ARGOS_SECRET_KEY_LENGTH);
    return 0;
}

/// @brief configures hardware to be compatible with arribada module
/// @param
void satellite_hw_init(void) {
    // Enable power to module
    __power_en(GPIO_PIN_RESET);
    resetKMACProfile();
}

/// @brief initializes satellite subsystem
/// @param
void satellite_init(void) {
    // read version
    //	__satellite_read_dma();
    //	while((0 != __satellite_wait_for_response(10000)) || !satellite_response_ok){
    //		;
    //	}

    // Reset AArribada module
    __power_en(GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SAT_NRST_GPIO_Output_GPIO_Port, SAT_NRST_GPIO_Output_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    __satellite_read_dma();
    __power_en(GPIO_PIN_SET);
    HAL_GPIO_WritePin(SAT_NRST_GPIO_Output_GPIO_Port, SAT_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_Delay(2000);
    while ((0 != __satellite_wait_for_response(10000)) || !satellite_response_ok) {
        satellite_ping();
    }

    // ping board to verify comms
    do {
        __NOP();
    } while (!satellite_ping());

#ifdef STATIC_CREDENTIALS
    satellite_set_id(ARGOS_CRED_ID, strlen(ARGOS_CRED_ID));
    satellite_set_mac_address(ARGOS_CRED_ADDRESS);
    satellite_set_secret_key(ARGOS_CRED_SECRET);
#endif // STATIC_CREDENTIALS

    // set radio protocol
    satellite_configure_radio(ARGOS_MOD_LDA2);
}
