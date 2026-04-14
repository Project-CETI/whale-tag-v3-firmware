/*****************************************************************************
 *   @file      m10s.h
 *   @brief     This header is the api to communicate with the GPS module
 *              (uBlox M10s)
 *   @project   Project CETI
 *   @date      06/22/2023
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, Kaveet Grewal,
 *****************************************************************************/

#ifndef INC_RECOVERY_INC_M10S_H_
#define INC_RECOVERY_INC_M10S_H_

#include <stddef.h> //for size_t
#include <stdint.h> //for uint8_t

typedef enum {
    PM_OPERATE_MODE_FULL,
    PM_OPERATE_MODE_PSMOO,
    PM_OPERATE_MODE_PSMCT,
} CfgPmOperateMode;

size_t m10s_ubx_cfg_set_msg_rate(uint8_t *buffer, size_t buffer_length, uint8_t data_rate_s);
size_t m10s_enter_software_standby_mode(uint8_t buffer[static 24], size_t buffer_length);
size_t m10s_disable_i2c_output(uint8_t *buffer, size_t buffer_length);
size_t m10s_disable_spi_output(uint8_t *buffer, size_t buffer_length);

#endif /* INC_RECOVERY_INC_M10S_H_ */
