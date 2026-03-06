//-----------------------------------------------------------------------------
// Project:      CETI Tag Electronics
// Version:      Refer to _versioning.h
// Copyright:    Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
// Description: High-level LED controller
//-----------------------------------------------------------------------------
#ifndef __CETI_LED_H__
#define __CETI_LED_H__

typedef enum {
    LED_RED = 0,
    LED_YELLOW = 1,
    LED_GREEN = 2,
}LedColor;

void led_init(void);
void led_off(LedColor color);
void led_on(LedColor color);
void led_blink(LedColor color);
void led_blink_alternate(LedColor color);
void led_set_blink_period(float period_s);
void led_burn(void);
void led_error(void);
void led_heartbeat(void);
void led_heartbeat_dim(void);
void led_idle(void);

#endif // __CETI_LED_H__
