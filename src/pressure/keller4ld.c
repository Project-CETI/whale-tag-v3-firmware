//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Cummings Electronics Labs, Harvard University Wood Lab,
//               MIT CSAIL
// Contributors: Matt Cummings, Peter Malkin, Joseph DelPreto,
//               Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: PDevice driver for Keller 4LD pressure transmitter
//-----------------------------------------------------------------------------

#include "keller4ld.h"


#include "main.h"
#include "i2c.h"

extern I2C_HandleTypeDef KELLER_hi2c;

typedef enum {
    KELLER4LD_MODE_DELAY,
    KELLER4LD_MODE_POLL,
    KELLER4LD_MODE_INTERRUPT,
} Keller4ldMode;

#define KELLER4LD_DEVICE_ADDR_DEFAULT 0x40

#define KELLER4LD_REG_SLAVE_ADDRESS 0x42

#define KELLER4LD_CMD_COMMAND_MODE 0xA9
#define KELLER4LD_CMD_REQUEST_MEASUREMENT 0xAC

static uint8_t s_raw_buffer[5] = {};
static Keller4LD_Measurement *s_measurement_location = NULL;



static void __keller4ld_read_measurement_it_complete(I2C_HandleTypeDef *hi2c) {
	// unregister callback
	HAL_I2C_UnRegisterCallback(&KELLER_hi2c, HAL_I2C_MASTER_RX_COMPLETE_CB_ID);

	// convert raw to measurement
	if (s_measurement_location != NULL) {
		s_measurement_location->status = s_raw_buffer[0];
		if (((s_measurement_location->status >> 6) & 0b11) != 0b01) {
			// ToDo: invalid packet
		}
		s_measurement_location->pressure = (((uint16_t)s_raw_buffer[1] << 8) | (uint16_t)s_raw_buffer[2]);
		s_measurement_location->temperature = (((uint16_t)s_raw_buffer[3] << 8) | (uint16_t)s_raw_buffer[4]);
	}
}

HAL_StatusTypeDef keller4ld_request_measurement_it(void) {
    uint8_t req = KELLER4LD_CMD_REQUEST_MEASUREMENT;
	return HAL_I2C_Master_Transmit_IT(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), &req, 1);
}

HAL_StatusTypeDef keller4ld_read_status(uint8_t *pStatus){
    return HAL_I2C_Master_Receive(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), pStatus, 1, 8);
}

HAL_StatusTypeDef keller4ld_read_measurement_it(Keller4LD_Measurement *pData){
	s_measurement_location = pData;
    HAL_I2C_RegisterCallback(&KELLER_hi2c, HAL_I2C_MASTER_RX_COMPLETE_CB_ID, __keller4ld_read_measurement_it_complete);
    HAL_I2C_Master_Receive_IT(&KELLER_hi2c, (KELLER4LD_DEVICE_ADDR_DEFAULT << 1), s_raw_buffer, sizeof(s_raw_buffer));
    return HAL_OK;
}
