#include "timing.h"

#include <stdint.h>

#include <tim.h>

#include "main.h"

extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim4;


static int s_timing_has_synced = 0;

volatile time_t s_timer_sync_rtc_epoch_us = 0;

void us_timer_rollover(TIM_HandleTypeDef *htim) {
  s_timer_sync_rtc_epoch_us = s_timer_sync_rtc_epoch_us + ((time_t)1 << 32);
}

time_t rtc_get_epoch_s(void) {
  RTC_DateTypeDef date;
  RTC_TimeTypeDef time;
  struct tm datetime;
  time_t timestamp;

  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  datetime.tm_year = date.Year + 100;
  datetime.tm_mday = date.Date;
  datetime.tm_mon = date.Month + 1;
  datetime.tm_hour = time.Hours;
  datetime.tm_min = time.Minutes;
  datetime.tm_sec = time.Seconds;
  timestamp = mktime(&datetime);
  return timestamp;
}

time_t rtc_get_epoch_ms(void) { return rtc_get_epoch_us() / 1000; }

time_t rtc_get_epoch_us(void) {
  // use the systemclock for better accuracy
  return s_timer_sync_rtc_epoch_us + (time_t)timing_get_time_since_on_us();
}

time_t timing_get_time_since_on_us(void) {
  return (time_t) uS_htim.Instance->CNT;
}

void rtc_set_datetime(const RTC_DateTypeDef *p_date, const RTC_TimeTypeDef *p_time) {
  RTC_TimeTypeDef mut_time = *p_time;
  RTC_DateTypeDef mut_date = *p_date;
  HAL_RTC_SetTime(&hrtc, &mut_time, RTC_FORMAT_BCD);
  HAL_RTC_SetDate(&hrtc, &mut_date, RTC_FORMAT_BCD);
  s_timing_has_synced = 1;
}

void rtc_get_datetime(RTC_DateTypeDef *p_date, RTC_TimeTypeDef *p_time) {
  if (NULL != p_date) {
     HAL_RTC_GetDate(&hrtc, p_date, RTC_FORMAT_BCD);
  }
  if (NULL != p_time) {
      HAL_RTC_GetTime(&hrtc, p_time, RTC_FORMAT_BCD);
  }
}

uint32_t rtc_has_been_syncronized(void) {
  return s_timing_has_synced;
}

static HAL_StatusTypeDef __rtc_init(void) {

  /** Initialize RTC Only */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_NONE;
  HAL_StatusTypeDef result = HAL_RTC_Init(&hrtc);

  RTC_PrivilegeStateTypeDef privilegeState = {    
    .rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO,
    .backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE,
    .backupRegisterStartZone2 = RTC_BKP_DR0,
    .backupRegisterStartZone3 = RTC_BKP_DR0,
  };
  if (HAL_OK == result) { result = HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState);}  
  /** Enable the reference Clock input */
  if (HAL_OK == result) { result = HAL_RTCEx_SetRefClock(&hrtc);}
  if (HAL_OK != result) {
    return result;
  }

  /** Sync system clock to RTC */
  RTC_DateTypeDef date;
  RTC_TimeTypeDef time;
  struct tm datetime;
  time_t seconds;
  time_t subseconds_us;

  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  datetime.tm_year = date.Year + 100;
  datetime.tm_mday = date.Date;
  datetime.tm_mon = date.Month + 1;
  datetime.tm_hour = time.Hours;
  datetime.tm_min = time.Minutes;
  datetime.tm_sec = time.Seconds;

  seconds = mktime(&datetime);
  subseconds_us = (1000000 * time.SubSeconds) / (time.SecondFraction + 1);

  s_timer_sync_rtc_epoch_us = (seconds * 1000000) + subseconds_us;
  uS_htim.Instance->CNT = 0;
  HAL_TIM_RegisterCallback(&uS_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, us_timer_rollover);

  HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);

  result = HAL_TIM_Base_Start_IT(&uS_htim);
  return result;
}

HAL_StatusTypeDef timing_init(void) {
  HAL_StatusTypeDef result = HAL_OK;
  
  MX_TIM4_Init();

  result = __rtc_init();

  return result;
}
