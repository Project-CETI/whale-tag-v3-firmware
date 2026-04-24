#include "timing.h"

#include <stdint.h>

#include <time.h>

#include "main.h"

extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef uS_htim;

static int s_timing_has_synced = 0;

/* Days from Jan 1 to the first of each month, non-leap year. */
static const uint16_t k_days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
};

/* Convert a broken-down UTC date/time to a Unix epoch in seconds.
 * year = full year (e.g. 2026), mon = 1..12, mday = 1..31.
 * Replaces newlib timegm(), which is unavailable in newlib-nano. */
static uint64_t priv__timegm_utc(int year, int mon, int mday,
                           int hour, int min, int sec) {
    int years_since_1970 = year - 1970;
    /* Leap days from 1970 up to (but not including) this year. */
    int leaps = ((year - 1) / 4) - ((year - 1) / 100) + ((year - 1) / 400)
              - (1969 / 4 - 1969 / 100 + 1969 / 400);

    long days = (long)years_since_1970 * 365 + leaps
              + k_days_before_month[mon - 1]
              + (mday - 1);

    /* Add today's leap day if we're in or past March of a leap year. */
    int is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    if (is_leap && mon > 2) {
        days += 1;
    }

    return (uint64_t)days * 86400 + (uint64_t)hour * 3600
         + (uint64_t)min * 60 + (uint64_t)sec;
}

volatile uint64_t s_timer_sync_rtc_epoch_us = 0;

void us_timer_rollover(TIM_HandleTypeDef *htim) {
    s_timer_sync_rtc_epoch_us = s_timer_sync_rtc_epoch_us + ((uint64_t)1 << 32);
}

uint64_t rtc_get_epoch_s(void) {
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;

    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
    return priv__timegm_utc(2000 + date.Year, date.Month, date.Date,
                        time.Hours, time.Minutes, time.Seconds);
}

__attribute__((no_instrument_function))
uint64_t rtc_get_epoch_ms(void) { return rtc_get_epoch_us() / 1000; }

__attribute__((no_instrument_function))
uint64_t rtc_get_epoch_us(void) {
    // use the systemclock for better accuracy
    return s_timer_sync_rtc_epoch_us + (uint64_t)timing_get_time_since_on_us();
}

__attribute__((no_instrument_function))
uint64_t timing_get_time_since_on_us(void) {
    return (uint64_t)uS_htim.Instance->CNT;
}

void rtc_set_datetime(const RTC_DateTypeDef p_date[static 1], const RTC_TimeTypeDef p_time[static 1]) {
    RTC_TimeTypeDef mut_time = *p_time;
    RTC_DateTypeDef mut_date = *p_date;
    HAL_RTC_SetTime(&hrtc, &mut_time, RTC_FORMAT_BCD);
    HAL_RTC_SetDate(&hrtc, &mut_date, RTC_FORMAT_BCD);
    s_timing_has_synced = 1;
}

static uint8_t priv__dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

void rtc_set_epoch_s(uint64_t epoch) {
    struct tm *dt = gmtime((time_t*)&epoch);
    if (dt == NULL) {
        return;
    }

    RTC_TimeTypeDef time = {
        .Hours = priv__dec_to_bcd(dt->tm_hour),
        .Minutes = priv__dec_to_bcd(dt->tm_min),
        .Seconds = priv__dec_to_bcd(dt->tm_sec),
    };
    RTC_DateTypeDef date = {
        .Year = priv__dec_to_bcd(dt->tm_year - 100),
        .Month = priv__dec_to_bcd(dt->tm_mon + 1),
        .Date = priv__dec_to_bcd(dt->tm_mday),
        .WeekDay = priv__dec_to_bcd(dt->tm_wday == 0 ? 7 : dt->tm_wday),
    };

    __disable_irq();
    rtc_set_datetime(&date, &time);
    uS_htim.Instance->CNT = 0;
    s_timer_sync_rtc_epoch_us = epoch * 1000000;
    __enable_irq();
}

void rtc_get_datetime(RTC_DateTypeDef *p_date, RTC_TimeTypeDef *p_time) {
    if (NULL != p_time) {
        HAL_RTC_GetTime(&hrtc, p_time, RTC_FORMAT_BCD);
    }
    if (NULL != p_date) {
        HAL_RTC_GetDate(&hrtc, p_date, RTC_FORMAT_BCD);
    }
}

uint32_t rtc_has_been_syncronized(void) {
    return s_timing_has_synced;
}

static HAL_StatusTypeDef priv__rtc_hw_init(void) {

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
    if (HAL_OK != result) {
        return result;
    }

    RTC_PrivilegeStateTypeDef privilegeState = {
        .rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO,
        .backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE,
        .backupRegisterStartZone2 = RTC_BKP_DR0,
        .backupRegisterStartZone3 = RTC_BKP_DR0,
    };
    return HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState);
}

/* Atomically snapshot the RTC into s_timer_sync_rtc_epoch_us and zero the
 * µs timer's count, so that rtc_get_epoch_us() = anchor + CNT yields the
 * correct epoch immediately after this call. Must be invoked AFTER the µs
 * timer is initialized and started. */
static void priv__sync_us_anchor_to_rtc(void) {
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    uint64_t seconds;
    uint64_t subseconds_us;

    __disable_irq();
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
    uS_htim.Instance->CNT = 0;

    seconds = (uint64_t)priv__timegm_utc(2000 + date.Year, date.Month, date.Date,
                                     time.Hours, time.Minutes, time.Seconds);
    subseconds_us = ((uint64_t)1000000 * time.SubSeconds) / (time.SecondFraction + 1);

    s_timer_sync_rtc_epoch_us = (seconds * 1000000ULL) + subseconds_us;
    __enable_irq();
}

static HAL_StatusTypeDef priv__uS_timer_init(void) {
    HAL_StatusTypeDef ret = HAL_OK;

    uS_htim.Instance = uS_TIM;
    uS_htim.Init.Prescaler = 159; // 1 uS
    uS_htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    uS_htim.Init.Period = 0xFFFFFFFF; // max
    uS_htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    uS_htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_RegisterCallback(&uS_htim, HAL_TIM_BASE_MSPINIT_CB_ID, HAL_TIM_Base_MspInit);
    HAL_TIM_RegisterCallback(&uS_htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, HAL_TIM_Base_MspDeInit);
    if (HAL_TIM_Base_Init(&uS_htim) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_RegisterCallback(&uS_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, us_timer_rollover);

    TIM_ClockConfigTypeDef sClockSourceConfig = {
        .ClockSource = TIM_CLOCKSOURCE_INTERNAL,
    };
    ret |= HAL_TIM_ConfigClockSource(&uS_htim, &sClockSourceConfig);

    TIM_MasterConfigTypeDef sMasterConfig = {
        .MasterOutputTrigger = TIM_TRGO_RESET,
        .MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE,
    };
    ret |= HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig);
    if (HAL_OK != ret) {
        // ToDo: handle errors
    }
    return ret;
}

HAL_StatusTypeDef timing_init(void) {
    HAL_StatusTypeDef result;

    /* 1. Bring up the RTC hardware (no time read yet). */
    result = priv__rtc_hw_init();
    if (HAL_OK != result) {
        return result;
    }

    /* 2. Bring up and start the µs timer. CNT will be re-zeroed inside the
     *    atomic snapshot below, so its value here doesn't matter. */
    result = priv__uS_timer_init();
    if (HAL_OK != result) {
        return result;
    }
    HAL_TIM_Base_Start_IT(&uS_htim);

    /* 3. Atomically snapshot the RTC into s_timer_sync_rtc_epoch_us and
     *    reset CNT to 0 in the same critical section, so no LSE ticks are
     *    lost between the read and the anchor write. */
    priv__sync_us_anchor_to_rtc();

    return HAL_OK;
}
