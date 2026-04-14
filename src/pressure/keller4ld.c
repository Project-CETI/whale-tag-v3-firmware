/*****************************************************************************
 * @file      pressure/keller4ld.c
 * @brief     Device driver for Keller 4LD pressure transmitter
 * @project   Project CETI
 * @copyright Cummings Electronics Labs, Harvard University Wood Lab,
 *            MIT CSAIL
 * @date      04/13/2026
 * @authors   Matt Cummings, Peter Malkin, Joseph DelPreto,
 *            Michael Salino-Hugg
 *****************************************************************************/
#include "keller4ld.h"

#include "i2c.h"
#include "main.h"

extern I2C_HandleTypeDef KELLER_hi2c;

#define KELLER4LD_DEVICE_ADDR_DEFAULT 0x40

#define KELLER4LD_REG_SLAVE_ADDRESS 0x42

#define KELLER4LD_CMD_COMMAND_MODE 0xA9
#define KELLER4LD_CMD_REQUEST_MEASUREMENT 0xAC

static void (*s_measurement_complete_callback)(uint8_t raw_packet[static 5]) = NULL;

/// @brief blocking read of
/// @param pStatus
/// @return
HAL_StatusTypeDef keller4ld_read_status(uint8_t *pStatus) {
    return HAL_I2C_Master_Receive(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), pStatus, 1, 8);
}

/// @brief initialize a pressure/temperature measurement
/// @param
/// @return
HAL_StatusTypeDef keller4ld_request_measurement(void) {
    uint8_t req = KELLER4LD_CMD_REQUEST_MEASUREMENT;
    return HAL_I2C_Master_Transmit(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), &req, 1, 10);
}

/// @brief  convert a raw pressure sensor measurement in to a
/// `Keller4LD_Measurement` struct.
/// @param p_measurement output measurement pointer
/// @param p_raw input raw measurement
void keller4ld_raw_to_measurement(const uint8_t p_raw[static 5], Keller4LD_Measurement *p_measurement) {
    p_measurement->status = p_raw[0];
    if (((p_measurement->status >> 6) & 0b11) != 0b01) {
        // ToDo: invalid packet
    }
    p_measurement->pressure = (((uint16_t)p_raw[1] << 8) | (uint16_t)p_raw[2]);
    p_measurement->temperature = (((uint16_t)p_raw[3] << 8) | (uint16_t)p_raw[4]);
}

/// @brief Callback to be called at the end of conversion, when data is ready
/// to be read.
/// @param
/// @note Connect keller4ld_eoc_callback to related external interrupt callback
/// or 8mS timer callback, or call manually after after 8mS or after detecting
/// eoc from `keller4ld_read_status()`.
void keller4ld_eoc_callback(void) {
    uint8_t raw_buffer[5] = {};

    // start measurement
    HAL_I2C_Master_Receive(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), raw_buffer, sizeof(raw_buffer), 10);
    if (NULL != s_measurement_complete_callback) {
        s_measurement_complete_callback(raw_buffer);
    }
}

/// @brief Register a function to be called once measurement in complete
/// @param p_callback callback function pointer
void keller4ld_register_measurement_complete_callback(void (*p_callback)(uint8_t raw_packet[static 5])) {
    s_measurement_complete_callback = p_callback;
}