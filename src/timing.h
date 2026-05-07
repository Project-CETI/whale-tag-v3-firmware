
#ifndef UTIL_TIMING_H
#define UTIL_TIMING_H

#ifndef UNIT_TEST
#include "main.h"
#else
typedef struct RTC_DateTypeDef RTC_DateTypeDef;
typedef struct RTC_TimeTypeDef RTC_TimeTypeDef;
typedef enum HAL_StatusTypeDef HAL_StatusTypeDef;
#endif

#include <time.h>

uint64_t rtc_get_epoch_s(void);
uint64_t rtc_get_epoch_ms(void);
uint64_t rtc_get_epoch_us(void);
uint64_t timing_get_time_since_on_us(void);
void rtc_get_datetime(RTC_DateTypeDef *p_date, RTC_TimeTypeDef *p_time);
void rtc_set_datetime(const RTC_DateTypeDef p_date[static 1], const RTC_TimeTypeDef p_time[static 1]);
void rtc_set_epoch_s(uint64_t epoch);
uint32_t rtc_has_been_syncronized(void);

HAL_StatusTypeDef timing_init(void);

#endif // UTIL_TIMING_H
