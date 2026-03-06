//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description:  High-level LED controller
//-----------------------------------------------------------------------------
#include "led.h"
#include "ktd2026ewe.h"

#define LED_RED_CURRENT_mA (20.0)
#define LED_YELLOW_CURRENT_mA (20.0)
#define LED_GREEN_CURRENT_mA (20.0)

void led_init(void){
    ktd2026ewe_reset();
    ktd2026ewe_set_led_current(LED_RED, LED_RED_CURRENT_mA);
    ktd2026ewe_set_led_current(LED_YELLOW, LED_YELLOW_CURRENT_mA);
    ktd2026ewe_set_led_current(LED_GREEN, LED_GREEN_CURRENT_mA);
}

void led_off(LedColor color){
    ktd2026ewe_set_led_mode(color, KTD2026EWE_MODE_OFF);
}

void led_on(LedColor color){
    ktd2026ewe_set_led_mode(color, KTD2026EWE_MODE_ON);
}

void led_blink(LedColor color){
    ktd2026ewe_set_led_mode(color, KTD2026EWE_MODE_PWM1);
}

void led_blink_alternate(LedColor color) {
    ktd2026ewe_set_led_mode(color, KTD2026EWE_MODE_PWM2);
}

void led_set_blink_period(float period_s) {
    ktd2026ewe_set_period_s(period_s);
}

void led_burn(void) {
    ktd2026ewe_set_led_current(LED_GREEN, LED_GREEN_CURRENT_mA);
    ktd2026ewe_set_led_current(LED_RED, LED_RED_CURRENT_mA);
//    ktd2026ewe_set_modes(KTD2026EWE_MODE_PWM2, KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_PWM1);
     led_blink(LED_GREEN);
     led_off(LED_YELLOW);
     led_blink_alternate(LED_RED);
    ktd2026ewe_set_pwm1_duty(0.25); // 0.25s on
    ktd2026ewe_set_period_s(1.0); // 4.5s off
} 

void led_error(void) {
    ktd2026ewe_set_led_current(LED_RED, LED_RED_CURRENT_mA);
//    ktd2026ewe_set_modes(KTD2026EWE_MODE_PWM1, KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_OFF);
     led_off(LED_GREEN);
     led_off(LED_YELLOW);
     led_blink(LED_RED);
    ktd2026ewe_set_pwm1_duty(0.5); // 0.25s on
    ktd2026ewe_set_period_s(0.33); // 4.5s off
}

void led_heartbeat(void) {
    ktd2026ewe_set_led_current(LED_GREEN, LED_GREEN_CURRENT_mA);
//    ktd2026ewe_set_modes(KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_PWM1);
     led_off(LED_RED);
     led_off(LED_YELLOW);
     led_blink(LED_GREEN);
    ktd2026ewe_set_pwm1_duty(0.25); // 0.25s on
    ktd2026ewe_set_period_s(1.0); // 4.5s off
}

void led_heartbeat_dim(void) {
    ktd2026ewe_set_led_current(LED_GREEN, 0.2);
//    ktd2026ewe_set_modes(KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_PWM1);
     led_off(LED_RED);
     led_off(LED_YELLOW);
     led_blink(LED_GREEN);
    ktd2026ewe_set_pwm1_duty(0.05); // 0.25s on
    ktd2026ewe_set_period_s(5.0); // 4.5s off
}

void led_idle(void) {
    ktd2026ewe_set_led_current(LED_YELLOW, LED_YELLOW_CURRENT_mA);
    led_off(LED_RED);
    led_on(LED_YELLOW);
    led_off(LED_GREEN);
//    ktd2026ewe_set_modes(KTD2026EWE_MODE_OFF, KTD2026EWE_MODE_ON, KTD2026EWE_MODE_OFF);
}
