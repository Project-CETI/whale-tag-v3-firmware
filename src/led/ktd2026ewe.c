#include "ktd2026ewe.h"

#include "main.h"

/*** External Resources ******************************************************/
#define LED_hi2c hi2c3
extern I2C_HandleTypeDef LED_hi2c;

/***  Macros Definitions *****************************************************/
#define KTD2026EWE_DEV_ADDR 0x30

#define KTD2026EWE_REG_RESET 0x00
#define KTD2026EWE_REG_FLASH_PERIOD 0x01
#define KTD2026EWE_REG_DUTY_1 0x02
#define KTD2026EWE_REG_DUTY_2 0x03
#define KTD2026EWE_REG_CHANNEL_CONTROL 0x04
#define KTD2026EWE_REG_RAMP_RATE 0x05
#define KTD2026EWE_REG_IOUT_1 0x06
#define KTD2026EWE_REG_IOUT_2 0x07
#define KTD2026EWE_REG_IOUT_3 0x08

typedef struct {
    uint8_t slot;
    uint8_t control;
    uint8_t period;
    uint8_t pwm_duty[2];
    uint8_t ch_control;
    uint8_t ramp_rate;
    uint8_t current[3];
} ktd2026ewe_HandleTypeDef;

/*** Private Global Variables ************************************************/

static ktd2026ewe_HandleTypeDef s_self = {};

/*** Function Definitions ****************************************************/
static HAL_StatusTypeDef __ktd2026ewe_write(uint8_t register_address, uint8_t value) {
    return HAL_I2C_Mem_Write(&LED_hi2c, (uint16_t)(KTD2026EWE_DEV_ADDR << 1), register_address, 1, &value, 1, 5);
}

HAL_StatusTypeDef ktd2026ewe_set_led_mode(uint8_t led_index, ktd2026ewe_LedMode mode) {
    s_self.ch_control = (mode << (2 * led_index)) | (s_self.ch_control & ~((0b11) << (2 * led_index)));
    return __ktd2026ewe_write(KTD2026EWE_REG_CHANNEL_CONTROL, s_self.ch_control);
}

HAL_StatusTypeDef ktd2026ewe_set_modes(ktd2026ewe_LedMode mode0, ktd2026ewe_LedMode mode1, ktd2026ewe_LedMode mode2) {
    s_self.ch_control = (mode0) | (mode1 << 2) | (mode1 << 4);
    return __ktd2026ewe_write(KTD2026EWE_REG_CHANNEL_CONTROL, s_self.ch_control);
}

HAL_StatusTypeDef ktd2026ewe_set_led_current(uint8_t led_index, float current_mA) {
    current_mA = (current_mA < 0.125) ? 0.125 : current_mA;
    current_mA = (current_mA >= 24.0) ? 24.0 : current_mA;
    s_self.current[led_index] = (uint8_t)((192.0 * current_mA / 24.0) - 1.0);
    return __ktd2026ewe_write(KTD2026EWE_REG_IOUT_1 + led_index, s_self.current[led_index]);
}

HAL_StatusTypeDef ktd2026ewe_set_period_s(float period_s) {
    uint8_t bits = (uint8_t)(period_s / 0.128);
    bits = (bits > 2) ? bits - 2 : 0;
    s_self.period = (s_self.period & ~(0b01111111)) | (bits & 0b01111111);
    return __ktd2026ewe_write(KTD2026EWE_REG_FLASH_PERIOD, s_self.period);
}

HAL_StatusTypeDef ktd2026ewe_set_pwm1_duty(float duty) {
    s_self.pwm_duty[0] = (uint8_t)(duty * 256.0);
    return __ktd2026ewe_write(KTD2026EWE_REG_DUTY_1, s_self.pwm_duty[0]);
}

HAL_StatusTypeDef ktd2026ewe_set_pwm2_duty(float duty) {
    s_self.pwm_duty[1] = (uint8_t)(duty * 256.0);
    return __ktd2026ewe_write(KTD2026EWE_REG_DUTY_2, s_self.pwm_duty[1]);
}

HAL_StatusTypeDef ktd2026ewe_reset(void) {
    HAL_StatusTypeDef result = __ktd2026ewe_write(KTD2026EWE_REG_RESET, 0b111);
    HAL_Delay(1); // wait atleast 200 uS
    // restore handle to default values
    s_self = (ktd2026ewe_HandleTypeDef){
        .slot = 0x00,
        .control = 0x00,
        .period = 0x00,
        .pwm_duty = {0x01, 0x01},
        .ch_control = 0x00,
        .ramp_rate = 0x00,
        .current = {0x4f, 0x4f, 0x4f}};
    // move pwm2 to slot 3
    s_self.slot = 0b010;
    return __ktd2026ewe_write(KTD2026EWE_REG_RESET, s_self.slot);

    return result;
}