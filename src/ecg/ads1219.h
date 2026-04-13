/*****************************************************************************
 *   @file   ads1219.h
 *   @brief  Header file of ADS1219 Driver.
 *   @author Michael Salino-Hugg
 *****************************************************************************/
#ifndef ADS1219_H_
#define ADS1219_H_

#include <stdint.h>

typedef enum {
    ADS1219_MUX_DIFF_0_1 = 0x00,
    ADS1219_MUX_DIFF_2_3 = 0x01,
    ADS1219_MUX_DIFF_1_2 = 0x02,
    ADS1219_MUX_SINGLE_0 = 0x03,
    ADS1219_MUX_SINGLE_1 = 0x04,
    ADS1219_MUX_SINGLE_2 = 0x05,
    ADS1219_MUX_SINGLE_3 = 0x06,
    ADS1219_MUX_SHORTED = 0x07,
} ADS1219_Mux;

typedef enum {
    ADS1219_GAIN_ONE = 0x00,
    ADS1219_GAIN_FOUR = 0x01,
} ADS1219_Gain;

typedef enum {
    ADS1219_DATA_RATE_20 = 0x00,
    ADS1219_DATA_RATE_90 = 0x01,
    ADS1219_DATA_RATE_330 = 0x02,
    ADS1219_DATA_RATE_1000 = 0x03,
} ADS1219_DataRate;

typedef enum {
    ADS1219_MODE_SINGLE_SHOT = 0x00,
    ADS1219_MODE_CONTINUOUS = 0x01,
} ADS1219_Mode;

typedef enum {
    ADS1219_VREF_INTERNAL = 0x00,
    ADS1219_VREF_EXTERNAL = 0x01,
} ADS1219_VoltageReference;

typedef struct {
    ADS1219_VoltageReference vref;
    ADS1219_Mode mode;
    ADS1219_DataRate data_rate;
    ADS1219_Gain gain;
    ADS1219_Mux mux;
} ADS1219_Configuration;

void ads1219_apply_configuration(const ADS1219_Configuration *configuration);
void ads1219_read_data_raw(int32_t *reading);
void ads1219_read_data_raw_it(int32_t *reading, void (*callback)(void));
void ads1219_reset(void);
void ads1219_start(void);
void ads1219_stop(void);

#endif // ADS1219_H_
