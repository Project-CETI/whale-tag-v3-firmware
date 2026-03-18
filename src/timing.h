
#ifndef UTIL_TIMING_H
#define UTIL_TIMING_H

#include "main.h"
#include <time.h>

time_t rtc_get_epoch_s(void);
time_t rtc_get_epoch_ms(void);
time_t rtc_get_epoch_us(void);
time_t timing_get_time_since_on_us(void);
void rtc_get_datetime(RTC_DateTypeDef *p_date, RTC_TimeTypeDef *p_time);
void rtc_set_datetime(const RTC_DateTypeDef *p_date, const RTC_TimeTypeDef *p_time);
void rtc_set_epoch_s(time_t epoch);
uint32_t rtc_has_been_syncronized(void);

HAL_StatusTypeDef timing_init(void);

#endif // UTIL_TIMING_H
