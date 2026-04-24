/*****************************************************************************
 *   @file      argos_tx_mgr.c
 *   @brief     This code manages when the recovery board should transmit its
 *              GPS coordinates via argos. It implements both a psuedorandom
 *              timer strategy and a satellite pass prediction algorithm.
 *   @project   Project CETI
 *   @date      12/10/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "argos_tx_mgr.h"

#include "aop.h" // run update_aop.sh to update this header
#include "error.h"
#include "previpass.h"
#include "timing.h"

#include <stdint.h>


/* MACROS */
#define TX_TIMER_PERIOD_MS (30000)
#define TX_BASE_INTERVAL_S (90)
#define TX_VARIENCE_S (TX_BASE_INTERVAL_S / (10))

/* EXTERNAL DECLARATIONS */
extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim2;

/* PUBLIC GLOBAL VARIABLES */
volatile uint8_t s_argos_tx_ready = 0;

/* PRIVATE GLOBAL VARIABLES */
#define NUM_SATS_IN_AOP_TABLE (sizeof(nv_aop_data.aopTable) / sizeof(struct AopSatelliteEntry_t))

static volatile uint8_t s_enabled = 0;
static ArgosTxStrategy s_strategy;
static uint8_t s_second_count = 0;
static uint8_t s_next_tx_interval_s;

static uint8_t s_valid_coordinates = 0;
static float s_latitude = 0.0f;  // (-90.0f to 90.0f)
static float s_longitude = 0.0f; // (0.0f to 360.0f)

/* FUNCTIONS */
#define TX_RAND_MAX (0xFFFF)
#define RAND_PRIME (0x98c3)

/// @brief generates a psudorandom value between 0 and 0xFFFF
/// @param
/// @return
static inline uint16_t priv__rand(void) {
    static uint16_t value;
    value += RAND_PRIME;
    return value;
};

/// @brief generates a psudorandom time interval
/// @param
/// @return
static inline int16_t priv__get_rand_tx_interval_s(void) {
    return TX_BASE_INTERVAL_S - TX_VARIENCE_S + (2 * (int32_t)priv__rand() * TX_VARIENCE_S / TX_RAND_MAX);
}

/// @brief timer callback to time duration between consecutive transmissions
/// @param htim
void argos_tx_mgr_TIM_IRQ(TIM_HandleTypeDef *htim) {
    s_second_count++;
    if (s_second_count >= s_next_tx_interval_s) {
        s_argos_tx_ready = 1;
        s_next_tx_interval_s = priv__get_rand_tx_interval_s();
        s_second_count = 0;
        HAL_PWR_DisableSleepOnExit();
    }
}

/// @brief enables transmit timer used to time duration between consecutive
/// transmissions
/// @param
static inline void priv__timer_enable(void) {
    if (s_enabled) {
        return; // already enabled
    }
    HAL_TIM_Base_Start_IT(&htim2);
    s_next_tx_interval_s = priv__get_rand_tx_interval_s();
    s_enabled = 1;
}

/// @brief disables transmit timer used to time duration between consecutive
/// transmissions
/// @param
static inline void priv__timer_disable(void) {
    if (!s_enabled) {
        return;
    }
    HAL_TIM_Base_Stop_IT(&htim2);
    s_second_count = 0;
    s_argos_tx_ready = 0;
    s_enabled = 0;
}

/// @brief converts a pass prediction datetime to RTC date and time
/// @param pass_date pointer to pass prediction datetime
/// @param rtc_date pointer to RTC date output
/// @param rtc_time pointer to RTC time output
static void priv__pass_date_to_rtc_date(const struct CalendarDateTime_t *pass_date, RTC_DateTypeDef *rtc_date, RTC_TimeTypeDef *rtc_time) {
    if (NULL == pass_date) {
        // error: no input
        return;
    }
    if (NULL != rtc_date) {
        rtc_date->Year = pass_date->year - 2000;
        rtc_date->Month = pass_date->month;
        rtc_date->Date = pass_date->day;
    }
    if (NULL != rtc_time) {
        rtc_time->Hours = pass_date->hour;
        rtc_time->Minutes = pass_date->minute;
        rtc_time->Seconds = pass_date->second;
    }
}

/// @brief converts a RTC date and time to a pass prediction datetime
/// @param rtc_date pointer to RTC date
/// @param rtc_time pointer to RTC time
/// @param pass_date pointer to pass prediction datetime output
static void priv__rtc_date_to_pass_date(const RTC_DateTypeDef *rtc_date, const RTC_TimeTypeDef *rtc_time, struct CalendarDateTime_t *pass_date) {
    if ((NULL == rtc_date) || (NULL == rtc_time) || (NULL == pass_date)) {
        // error: no input
        return;
    }
    pass_date->year = rtc_date->Year + 2000;
    pass_date->month = rtc_date->Month;
    pass_date->day = rtc_date->Date;
    pass_date->hour = rtc_time->Hours;
    pass_date->minute = rtc_time->Minutes;
    pass_date->second = rtc_time->Seconds;
}

/// @brief performs satellite pass prediction and sets pass start and stop RTC
/// alarms
/// @param
void priv__update_next_satellite_pass_alarms(void) {
    RTC_AlarmTypeDef pass_alarm = {
        .AlarmMask = RTC_ALARMMASK_DATEWEEKDAY,
    };
    RTC_DateTypeDef rtc_date;
    RTC_TimeTypeDef rtc_time;

    HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BCD);
    HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BCD);
    if (!s_valid_coordinates) {
        /* Do not have adequate information to perform pass prediction */
        priv__timer_enable();

        // set end alarm for 15 minutes in the future
        pass_alarm.AlarmTime = rtc_time; // copy rtc_time struct to Alarm
        pass_alarm.AlarmTime.Minutes += 15;
        if (pass_alarm.AlarmTime.Minutes >= 60) {
            pass_alarm.AlarmTime.Minutes -= 60;
            pass_alarm.AlarmTime.Hours = (pass_alarm.AlarmTime.Hours + 1) % 24;
        }
        pass_alarm.Alarm = RTC_ALARM_B;
        HAL_RTC_SetAlarm_IT(&hrtc, &pass_alarm, RTC_FORMAT_BCD); // set time to disable timer and recheck pass prediction
        return;
    }

    struct PredictionPassConfiguration_stu90_t prepasConfiguration = {
        .minElevation = 5.0f,
        .maxElevation = 90.0f,
        .minPassDurationMinute = 5.0f,
        .maxPasses = 1000,
        .timeMarginMinPer6months = 5.0f,
        .computationStepSecond = 30,
    };

    prepasConfiguration.beaconLatitude = s_latitude;
    prepasConfiguration.beaconLongitude = s_longitude;

    struct CalendarDateTime_t tmp_date;
    priv__rtc_date_to_pass_date(&rtc_date, &rtc_time, &tmp_date);
    PREVIPASS_UTIL_date_calendar_stu90(&tmp_date, &prepasConfiguration.start_stu90);
    prepasConfiguration.start_stu90 += 1 * 60;                                     // give ourselves 1 minutes time to setup calculation
    prepasConfiguration.end_stu90 = prepasConfiguration.start_stu90 + 6 * 60 * 60; // calculate passes for the next 6 hours

    // Perform next pass prediction for each satellite
    uint32_t pass_start_epoch = 0xFFFFFFFF;
    uint32_t pass_end_epoch = 0;
    uint32_t pass_start[nv_aop_data.table_count];
    uint32_t pass_end[nv_aop_data.table_count];
    for (int sat_num = 0; sat_num < nv_aop_data.table_count; sat_num++) {
        struct SatelliteNextPassPrediction_t current_sat;
        bool memoryPoolOverflow;
        memoryPoolOverflow = PREVIPASS_estimate_next_pass_with_status(
            &prepasConfiguration,
            &nv_aop_data.aopTable[sat_num],
            &current_sat);

        // this is required as current_sat.epoch can be a very small number if no valid pass in 24 hour is found
        if ((false == memoryPoolOverflow) || ((current_sat.epoch - EPOCH_90_TO_70_OFFSET) < prepasConfiguration.start_stu90)) {
            pass_start[sat_num] = 0xFFFFFFFF;
            pass_end[sat_num] = 0;
            continue;
        }

        pass_start[sat_num] = current_sat.epoch - EPOCH_90_TO_70_OFFSET;
        pass_end[sat_num] = pass_start[sat_num] + current_sat.duration;
    }

    // sort satellites by pass start time
    for (int i_sat = 0; i_sat < NUM_SATS_IN_AOP_TABLE - 1; i_sat++) {
        for (int j_sat = i_sat; j_sat < NUM_SATS_IN_AOP_TABLE; j_sat++) {
            // swap passes
            if (pass_start[i_sat] > pass_start[j_sat]) {
                pass_start[i_sat] ^= pass_start[j_sat];
                pass_start[j_sat] ^= pass_start[i_sat];
                pass_start[i_sat] ^= pass_start[j_sat];

                pass_end[i_sat] ^= pass_end[j_sat];
                pass_end[j_sat] ^= pass_end[i_sat];
                pass_end[i_sat] ^= pass_end[j_sat];
            }
        }

        if (0 == i_sat) {
            // initialize passes
            pass_start_epoch = pass_start[i_sat];
            pass_end_epoch = pass_end[i_sat];
        } else {
            // detection of break in passes longer than 1 transmission period
            if ((pass_end_epoch + 90) < pass_start[i_sat]) {
                break;
            }

            // combine passes
            if (pass_end_epoch < pass_end[i_sat]) {
                pass_end_epoch = pass_end[i_sat];
            }
        }
    }

    // set alarm to recalculate passes in 6 hours if no passes in the next 6 hours
    if (0 == pass_end_epoch) {
        pass_alarm.AlarmTime = rtc_time;
        pass_alarm.AlarmTime.Hours = ((rtc_time.Hours + 6) % 24);
        pass_alarm.Alarm = RTC_ALARM_B;
        HAL_RTC_SetAlarm_IT(&hrtc, &pass_alarm, RTC_FORMAT_BCD); // set time to disable timer and recheck pass prediction
        return;
    }

    // get next pass start time
    PREVIPASS_UTIL_date_stu90_calendar(pass_start_epoch, &tmp_date);
    priv__pass_date_to_rtc_date(&tmp_date, NULL, &pass_alarm.AlarmTime);
    pass_alarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(&hrtc, &pass_alarm, RTC_FORMAT_BCD); // set expected pass time to enable timer

    // get next pass end time
    PREVIPASS_UTIL_date_stu90_calendar(pass_end_epoch, &tmp_date);
    priv__pass_date_to_rtc_date(&tmp_date, NULL, &pass_alarm.AlarmTime);
    pass_alarm.Alarm = RTC_ALARM_B;
    HAL_RTC_SetAlarm_IT(&hrtc, &pass_alarm, RTC_FORMAT_BCD); // set time to disable timer and recheck pass prediction
}

/// @brief pass start RTC alarm callback
/// @param hrtc
void argos_tx_mgr_pass_start_alarm_callback(RTC_HandleTypeDef *hrtc) {
    priv__timer_enable(); // start transmitting
}

/// @brief pass stop RTC alarm callback
/// @param hrtc
void HAL_RTC_AlarmBEventCallback(RTC_HandleTypeDef *hrtc) {
    priv__timer_disable();                     // stop transmitting
    priv__update_next_satellite_pass_alarms(); // update pass prediction to next pass
}

/// @brief enables the argos transmission manager
/// @param
void argos_tx_mgr_enable(ArgosTxStrategy strategy) {
    // revert to timed tx if aop table older than 1 month
    if ((ARGOS_TX_STRATEGY_PATH_PREDICTOR == strategy)  
        && (nv_aop_data.timestamp_s + (30*24*60*60) < rtc_get_epoch_s())
    ) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_ARGOS, ERR_TYPE_DEFAULT, ERR_OUTDATED_AOP_TABLE), argos_tx_mgr_enable);
        s_strategy = ARGOS_TX_STRATEGY_TIMER;
    } else {
        s_strategy = strategy;
    }

    
    switch( s_strategy ) {
        case ARGOS_TX_STRATEGY_TIMER: {
            s_argos_tx_ready = 1;
            priv__timer_enable();
        }
        case ARGOS_TX_STRATEGY_PATH_PREDICTOR: {
            priv__update_next_satellite_pass_alarms(); // get next pass
        }
    }
}

/// @brief disables the argos transmission manager
/// @param
void argos_tx_mgr_disable(void) {
    if (ARGOS_TX_STRATEGY_PATH_PREDICTOR == s_strategy){
        HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A); // disable pass alarms
        HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_B);
    }
    priv__timer_disable(); // disable tx timer
}

/// @brief checks if a transmission should happen
/// @param
/// @return 1 if a transmission should happen, 0 otherwise
uint8_t argos_tx_mgr_ready_to_tx(void) {
    return s_argos_tx_ready;
}

/// @brief invalidates the flag indicating that a transmission should happen
/// @param
void argos_tx_mgr_inval_ready_to_tx(void) {
    s_argos_tx_ready = 0;
}

/// @brief sets the coordinates for the transmission manager to perform pass
/// prediction
/// @param latitude
/// @param longitude
void argos_tx_mgr_set_coordinates(float latitude, float longitude) {
    s_latitude = latitude;
    s_longitude = longitude;
    s_valid_coordinates = 1;
}
