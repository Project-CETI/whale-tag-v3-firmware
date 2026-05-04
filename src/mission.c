//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
// #define UNIT_TEST
#include "mission.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

#ifndef UNIT_TEST
#include "audio/acq_audio.h"
#include "audio/log_audio.h"
#include "battery/acq_battery.h"
#include "battery/log_battery.h"
#include "burnwire.h"
#include "ecg/acq_ecg.h"
#include "ecg/log_ecg.h"
#include "error.h"
#include "gps/acq_gps.h"
#include "gps/log_gps.h"
#include "gps/parse_gps.h"
#include "imu/acq_imu.h"
#include "imu/log_imu.h"
#include "led/led.h"
#include "metadata.h"
#endif // UNIT_TEST
#include "pressure/acq_pressure.h"
#ifndef UNIT_TEST
#include "pressure/log_pressure.h"
#include "satellite/argos_tx_mgr.h"
#include "satellite/satellite.h"
#include "satellite/log_argos.h"
#include "syslog.h"
#endif // UNIT_TEST
#include "timing.h"

#ifndef UNIT_TEST
#include "main.h"
#include <usart.h>
#endif // UNIT_TEST

#include "mission/float_detection.h"
#include "mission/mission_battery.h"
#include "mission/mission_log.h"

typedef int (*MissionTransistionConditionFn)(void);

typedef struct {
    MissionState from;
    MissionTransistionConditionFn condition;
    MissionState to;
    MissionTransitionCause cause;
} MissionTransitionRule;

static int priv__is_time_to_burn(void);
static int priv__is_burn_complete(void);
static int priv__is_low_pressure(void);
static int priv__is_high_pressure(void);
static int priv__is_floating(void);
static int priv__is_not_floating(void);

/* STATE MACHINE AS DESCRIBED BY TABLE BELOW
          ┌─────────┐
          │ MISSION │
          │ START   │
          └───┬─────┘
              │ START
              v
          ┌─────────┐   LOW_PRESSURE             ┌─────────┐
          │ RECORD  │<───────────────────────────┤  RECORD │
          │ SURFACE ├────────────┬──────────────>│  DIVE   │
          │         │<───────┐   │ HIGH_PRESSURE │         │
          └─┬──┬──┬─┘ !FLOAT │   │               └──┬───┬──┘
  LOW POWER │  │  │          │   │                  │   │
  (to LPB)<─┘  │  │ FLOAT  ┌─┴───┴─────┐ LOW POWER  │   │
               │  └───────>│  RECORD   ├──>(to LPB) │   │LOW POWER
               ├───────────│  FLOATING │            │   │
               │           └───────────┘            │   │
               ├────────────────────────────────────┘   │
               │ TIMER                                  │
               V                                        V
           ┌────────┐                           ┌───────────┐                      
           │  BURN  │    LOW POWER / FLOAT      │ LOW POWER │<─(from RF)
           │        ├──────────────────────────>│ BURN      │<─(from RS)
           └────┬───┘                           └─────┬─────┘
                │ TIMER                               │ TIMER
                V                                     V
           ┌──────────┐                         ┌───────────┐                      
           │ RETRIEVE │   LOW POWER / FLOAT     │ LOW POWER │
           │          │────────────────────────>│ RETRIEVE  │
           └──────────┘                         └───────────┘
*/
static const MissionTransitionRule s_transition_table[] = {
    /* MISSION_START - handled seperately based on tag_config */
    // {.from = MISSION_STATE_MISSION_START, .condition = NULL, .to = STARTING_STATE, .cause = MISSION_TRANSITION_START},
    
    /* RECORD_SURFACE -- ordered by priority */
    {.from = MISSION_STATE_RECORD_SURFACE, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_BURN,  .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_RECORD_SURFACE, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_BURN,  .cause = MISSION_TRANSITION_BATTERY_ERRORS },
    {.from = MISSION_STATE_RECORD_SURFACE, .condition = priv__is_time_to_burn,              .to = MISSION_STATE_BURN,            .cause = MISSION_TRANSITION_TIMER          },
    {.from = MISSION_STATE_RECORD_SURFACE, .condition = priv__is_high_pressure,             .to = MISSION_STATE_RECORD_DIVE,     .cause = MISSION_TRANSITION_HIGH_PRESSURE  },
    {.from = MISSION_STATE_RECORD_SURFACE, .condition = priv__is_floating,                  .to = MISSION_STATE_RECORD_FLOATING, .cause = MISSION_TRANSITION_FLOAT_DETECTED },

    /* RECORD_FLOATING -- ordered by priority */
    {.from = MISSION_STATE_RECORD_FLOATING, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_RECORD_FLOATING, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_BATTERY_ERRORS },
    {.from = MISSION_STATE_RECORD_FLOATING, .condition = priv__is_time_to_burn,              .to = MISSION_STATE_BURN,           .cause = MISSION_TRANSITION_TIMER          },
    {.from = MISSION_STATE_RECORD_FLOATING, .condition = priv__is_high_pressure,             .to = MISSION_STATE_RECORD_DIVE,    .cause = MISSION_TRANSITION_HIGH_PRESSURE  },
    {.from = MISSION_STATE_RECORD_FLOATING, .condition = priv__is_not_floating,              .to = MISSION_STATE_RECORD_SURFACE, .cause = MISSION_TRANSITION_FLOAT_ENDED    },

    /* RECORD_DIVING -- ordered by priority */
    {.from = MISSION_STATE_RECORD_DIVE, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_RECORD_DIVE, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_BATTERY_ERRORS },
    {.from = MISSION_STATE_RECORD_DIVE, .condition = priv__is_time_to_burn,              .to = MISSION_STATE_BURN,           .cause = MISSION_TRANSITION_TIMER          },
    {.from = MISSION_STATE_RECORD_DIVE, .condition = priv__is_low_pressure,              .to = MISSION_STATE_RECORD_SURFACE, .cause = MISSION_TRANSITION_LOW_PRESSURE   },

    /* BURN -- ordered by priority */
    {.from = MISSION_STATE_BURN, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_BURN, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_BATTERY_ERRORS },
    {.from = MISSION_STATE_BURN, .condition = priv__is_burn_complete,             .to = MISSION_STATE_RETRIEVE,       .cause = MISSION_TRANSITION_TIMER          },
    {.from = MISSION_STATE_BURN, .condition = priv__is_floating,                  .to = MISSION_STATE_LOW_POWER_BURN, .cause = MISSION_TRANSITION_FLOAT_DETECTED },

    /* LOW_POWER_BURN -- ordered by priority */
    {.from = MISSION_STATE_LOW_POWER_BURN, .condition = priv__is_burn_complete, .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_TIMER },
    
    /* RETRIEVE -- ordered by priority */
    {.from = MISSION_STATE_RETRIEVE, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_RETRIEVE, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_BATTERY_ERRORS },
    {.from = MISSION_STATE_RETRIEVE, .condition = priv__is_floating,                  .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_FLOAT_DETECTED },
    
    /* LOW_POWER_RETRIEVE -- ordered by priority */
    {.from = MISSION_STATE_LOW_POWER_RETRIEVE, .condition = mission_battery_is_low_voltage, .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_LOW_VOLTAGE    },
    {.from = MISSION_STATE_LOW_POWER_RETRIEVE, .condition = mission_battery_is_in_error,    .to = MISSION_STATE_LOW_POWER_RETRIEVE, .cause = MISSION_TRANSITION_BATTERY_ERRORS },
};

typedef enum {
    EN_AUDIO = (1 << 0),
    EN_BMS = (1 << 1),
    EN_IMU = (1 << 2),
    EN_ECG = (1 << 3),
    EN_PRESSURE = (1 << 4),
    EN_GPS = (1 << 5),
    EN_ARGOS = (1 << 6),
    EN_BURN = (1 << 7),
    EN_FLOAT = (1 << 8),
    EN_FLASH = (1 << 9),
    EN_VHF = (1 << 10),
    EN_WATER_SENSE = (1 << 11),
} SystemComponent;

static uint32_t s_enabled_subsystems = 0xffffffff;
static uint32_t s_active_subsystems = 0;
static const uint32_t s_state_active_subsystems_rule[] = {
    [MISSION_STATE_MISSION_START]       = 0,
    [MISSION_STATE_PREDEPLOYMENT]       =        0 |      0 |      0 |      0 |           0 | EN_GPS |       0 | 0        |        0 |       0 |      0 | EN_WATER_SENSE,
    [MISSION_STATE_RECORD_SURFACE]      = EN_AUDIO | EN_BMS | EN_IMU | EN_ECG | EN_PRESSURE | EN_GPS |       0 | EN_FLOAT |        0 |       0 |      0 |              0,
    [MISSION_STATE_RECORD_FLOATING]     = EN_AUDIO | EN_BMS | EN_IMU | EN_ECG | EN_PRESSURE | EN_GPS |       0 | EN_FLOAT | EN_ARGOS |EN_FLASH | EN_VHF |              0,
    [MISSION_STATE_RECORD_DIVE]         = EN_AUDIO | EN_BMS | EN_IMU | EN_ECG | EN_PRESSURE |      0 |       0 |        0 |        0 |       0 |      0 |              0,
    [MISSION_STATE_BURN]                = EN_AUDIO | EN_BMS | EN_IMU | EN_ECG | EN_PRESSURE | EN_GPS | EN_BURN | EN_FLOAT | EN_ARGOS |EN_FLASH | EN_VHF |              0,
    [MISSION_STATE_RETRIEVE]            = EN_AUDIO | EN_BMS | EN_IMU |      0 | EN_PRESSURE | EN_GPS |       0 | EN_FLOAT | EN_ARGOS |EN_FLASH | EN_VHF |              0,
    [MISSION_STATE_LOW_POWER_BURN]      =        0 | EN_BMS |      0 |      0 |           0 | EN_GPS | EN_BURN |        0 | EN_ARGOS |EN_FLASH | EN_VHF |              0,
    [MISSION_STATE_LOW_POWER_RETRIEVE]  =        0 | EN_BMS |      0 |      0 |           0 | EN_GPS |       0 |        0 | EN_ARGOS |EN_FLASH | EN_VHF |              0,
    [MISSION_STATE_ERROR]               = 0
};

typedef enum {
    MISSION_SLEEP_NONE,
    MISSION_SLEEP_SLEEP,
    MISSION_SLEEP_STOP1,
    MISSION_SLEEP_STOP2,
} MissionSleepDepth;

static const MissionSleepDepth s_state_sleep_depth[] = {
    [MISSION_STATE_MISSION_START]       = MISSION_SLEEP_NONE,
    [MISSION_STATE_PREDEPLOYMENT]       = MISSION_SLEEP_STOP1,
    [MISSION_STATE_RECORD_SURFACE]      = MISSION_SLEEP_SLEEP,
    [MISSION_STATE_RECORD_FLOATING]     = MISSION_SLEEP_SLEEP,
    [MISSION_STATE_RECORD_DIVE]         = MISSION_SLEEP_SLEEP,
    [MISSION_STATE_BURN]                = MISSION_SLEEP_SLEEP,
    [MISSION_STATE_RETRIEVE]            = MISSION_SLEEP_SLEEP,
    [MISSION_STATE_LOW_POWER_BURN]      = MISSION_SLEEP_STOP1,
    [MISSION_STATE_LOW_POWER_RETRIEVE]  = MISSION_SLEEP_STOP1,
    [MISSION_STATE_ERROR]               = MISSION_SLEEP_NONE,
};


#ifndef UNIT_TEST
extern void SystemClock_Config(void);
#endif // UNIT_TEST

static uint16_t s_dive_threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_DIVE_THRESHOLD_BAR);
static uint16_t s_surface_threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_SURFACE_THRESHOLD_BAR);

#define ARRAY_LEN(x) (sizeof((x)) / sizeof(*(x)))

#ifndef UNIT_TEST
typedef void (*MissionTask)(void);

static MissionState s_state = MISSION_STATE_MISSION_START;
static volatile uint8_t s_update_periodic_mission_tasks = 0;
TIM_HandleTypeDef mission_htim;

#endif

/* PRESSURE CONTROLS *********************************************************/

static int priv__is_low_pressure(void) {
    if (!(EN_PRESSURE & s_active_subsystems)){
        return 1;
    }
    CetiPressureSample pressure_sample;
    acq_pressure_get_latest(&pressure_sample);
    return (pressure_sample.pressure < s_surface_threshold_raw);
}

static int priv__is_high_pressure(void) {
    if (!(EN_PRESSURE & s_active_subsystems)){
        return 0;
    }
    CetiPressureSample pressure_sample;
    acq_pressure_get_latest(&pressure_sample);
    return (pressure_sample.pressure >= s_dive_threshold_raw);
}

/* BURN CONTROLS *************************************************************/

static uint8_t s_burn_started = 0;
static uint64_t s_burn_start_timestamp_s = 0;
/// @brief check if burn should be activated based on timeout
/// @param
/// @return bool
static int priv__is_time_to_burn(void) {
    uint64_t now_timestamp_s;
    now_timestamp_s = rtc_get_epoch_us()/1000000;
    return (now_timestamp_s >= s_burn_start_timestamp_s);
}

/// @brief checks whether the burnwire has completely burn
/// @param
/// @return bool - true = burn is complete; false = burn not complete
/// @note the current implementation is time based, but implementing a hardware
///       change to detect when the burnwire has broken would allow for less
///       power wasted on burn
static int priv__is_burn_complete(void) {
    if (!s_burn_started) {
        return 0;
    }
    uint64_t elapsed_time = rtc_get_epoch_us()/1000000 - s_burn_start_timestamp_s;
    return (elapsed_time > tag_config.burnwire.duration_s * 60);
}

/* FLOAT CONTROLS *************************************************************/
static int priv__is_floating(void) {
    if (!(EN_FLOAT & s_active_subsystems)) {
        return 0;
    }
    return float_detection_is_floating();
}

static int priv__is_not_floating(void) {
    if (!(EN_FLOAT & s_active_subsystems)) {
        return 1;
    }
    return !float_detection_is_floating();
}

#ifndef UNIT_TEST
/* ARGOS TRANSMISSION CONTROLS *************************************************************/

/// @brief Transmits an input to satelite, and generates log of message sent
/// @param message pointer to input array
/// @param message_len length of input array
static void priv__satellite_transmit_from_raw_with_log(RecoveryArgoModulation rconf, const uint8_t *message, uint8_t message_len) {
    uint16_t max_len = 24;
    switch (rconf) {
        case ARGOS_MOD_LDA2:
            max_len = 24;
            break;

        case ARGOS_MOD_VLDA4:
            max_len = 3;
            break;

        case ARGOS_MOD_LDK:
            max_len = 16;
            break;

        case ARGOS_MOD_LDA2L:
            max_len = 24;
            break;
    }

    ArgosTxEvent tx_event = {
        .timestamp_us = rtc_get_epoch_us(),
        .tx_type = rconf,
        .message = {0}, // zero message
    };

    // add trailing zeros to message if message less than max message length
    uint8_t sized_message_buffer[24] = {0};
    message_len = (message_len < max_len) ? message_len : max_len;
    memcpy(sized_message_buffer, message, message_len);

    // convert hex to ascii representation of hex
    for (int i = 0; i < max_len; i++) {
        snprintf((char *)&tx_event.message[2 * i], 3, "%02X", sized_message_buffer[i]);
    }

    // perform transmission
    satellite_transmit((char *)tx_event.message, 2 * max_len); // transmit via argos
    
    // log transmission activity
    log_argos_event(tx_event);
}


/* GPS TASKS *****************************************************************/

/// @brief periodic task to resynchronize the RTC to GPS.
/// @param  
static void priv__resyncronize_rtc_task(void) {
    if (rtc_has_been_syncronized()) {
        return;
    }

    // get latest RMC message
    GpsCoord coord = parse_gps_get_latest_coordinates();
    if (!coord.valid) {
        return;
    }

    // update rtc
    RTC_TimeTypeDef sTime = {
        .Hours = coord.hours,
        .Minutes = coord.minutes,
        .Seconds = coord.seconds,
    };
    RTC_DateTypeDef sDate = {
        .Year = coord.year,
        .Date = coord.day,
        .Month = coord.month,
    };
    rtc_set_datetime(&sDate, &sTime);
}

/// @brief update known coordinates for argos tx manager
/// @param  
void priv__update_argos_coordinates(void) {
    // get latest RMC message
    GpsCoord coord = parse_gps_get_latest_coordinates();
    if (!coord.valid) {
        return;
    }

    float lat = coord.latitude;
    if (coord.latitude_sign) {
        lat = -lat;
    }
    float lon = coord.longitude;
    if (coord.longitude_sign) {
        lon = 360.0f - lon;
    }
    argos_tx_mgr_set_coordinates(lat, lon);
}

/// @brief Task that performs transmission of GPS coordinates via argos satellite and forwarding
///        transmission to host system as a nmea packet for logging purposes
/// @param
static void priv__transmit_gps_task(void) {
    static uint32_t tx_count = 0;
    if (!argos_tx_mgr_ready_to_tx()) {
        return;
    }

    GpsCoord coords = parse_gps_get_latest_coordinates();

    // construct tx_message
    uint32_t lat_abs = 0xffffffff;
    uint32_t lon_abs = 0xffffffff;
    if (!coords.valid) {
        RTC_DateTypeDef rtc_date;
        RTC_TimeTypeDef rtc_time;
        rtc_get_datetime(&rtc_date, &rtc_time);
        coords.day = rtc_date.Date;
        coords.hours = rtc_time.Hours;
        coords.minutes = rtc_time.Minutes;
        coords.latitude_sign = 1;
        coords.longitude_sign = 1;
    } else {
        lat_abs = (uint32_t)(coords.latitude * 10000.0f);
        lon_abs = (uint32_t)(coords.longitude * 10000.0f);
    }

    // generate gps packet
    uint64_t gps_packet = 0;
    gps_packet = (((uint64_t)(coords.day & ((1 << 5) - 1))) << (64 - 5)) 
        | (((uint64_t)(coords.hours & ((1 << 5) - 1))) << (64 - 10)) 
        | (((uint64_t)(coords.minutes & ((1 << 6) - 1))) << (64 - 16)) 
        | (((uint64_t)(coords.latitude_sign & ((1 << 1) - 1))) << (64 - 17)) 
        | (((uint64_t)(lat_abs & ((1 << 20) - 1))) << (64 - 37)) 
        | (((uint64_t)(coords.longitude_sign & ((1 << 1) - 1))) << (64 - 38)) 
        | (((uint64_t)(lon_abs & ((1 << 21) - 1))) << (64 - 59))
        ;

    // move gps packet into tx_message_buffer
    uint8_t tx_message[24] = {};
    for (int i = 0; i < 8; i++) {
        tx_message[i] = (uint8_t)((gps_packet >> 8 * (8 - (i + 1))) & 0xff);
    }

    RecoveryArgoModulation rconf;
    satellite_get_rconf(&rconf);

    uint16_t max_len = 24;
    switch (rconf) {
        case ARGOS_MOD_LDA2:
            max_len = 24;
            break;

        case ARGOS_MOD_VLDA4:
            max_len = 3;
            break;

        case ARGOS_MOD_LDK:
            max_len = 16;
            break;

        case ARGOS_MOD_LDA2L:
            max_len = 24;
            break;
    }

    // number_packet
    if (rconf != ARGOS_MOD_VLDA4) {
        tx_message[max_len - 4] = (uint8_t)((tx_count >> 24) & ((1 << 8) - 1));
        tx_message[max_len - 3] = (uint8_t)((tx_count >> 16) & ((1 << 8) - 1));
        tx_message[max_len - 2] = (uint8_t)((tx_count >> 8) & ((1 << 8) - 1));
        tx_message[max_len - 1] = (uint8_t)((tx_count >> 0) & ((1 << 8) - 1));

        priv__satellite_transmit_from_raw_with_log(rconf, tx_message, max_len);
    } else {
        // VLDA4 only consists of 3 bytes, so skip the data info to only transmit GPS coord
        priv__satellite_transmit_from_raw_with_log(rconf, &tx_message[1], 3);
    }
    argos_tx_mgr_inval_ready_to_tx(); // reset transmission timer
    tx_count++;                       // update transmission count
}

/// @brief this task reverts the recovery board to its "active state" if it is
/// likely the state_machine is stuck in an "unsafe" state with no host
/// communication
static void priv__wdt_task(void) {
#warning "ToDo: implement priv__wdt_task"
}


static void priv__gps_msg_callback(const GpsSentence *p_sentence) {
    log_gps_push_sentence(p_sentence);
    parse_gps_push_sentence(p_sentence);
}
/* MISSION STATE MACHINE *****************************************************/


static struct {
    uint8_t count;
    MissionTask tasks[64];
} s_state_dependent_tasks = {
    .count = 0,
    .tasks = {NULL},
};

static inline void priv__push_task(MissionTask task) {
    s_state_dependent_tasks.tasks[s_state_dependent_tasks.count] = task; // always check if audio needs to be logged
    s_state_dependent_tasks.count++;
}

static inline void priv__push_audio_task(MissionState state) {
    if (EN_AUDIO & s_state_active_subsystems_rule[state] & s_enabled_subsystems) {
        priv__push_task(log_audio_task); // always check if audio needs to be logged
    }
}

static void priv__update_state_dependent_task_list(MissionState state) {
    s_state_dependent_tasks.count = 0;

    priv__push_audio_task(state);

    if (s_state_active_subsystems_rule[state] & EN_ECG) {
        if (tag_config.ecg.enabled) {
            priv__push_task(log_ecg_task);
            priv__push_audio_task(state);
        }
    }

    // log_imu
    if ((MISSION_STATE_LOW_POWER_BURN != state)
        && (MISSION_STATE_LOW_POWER_RETRIEVE != state)
    ) {
        if (tag_config.imu.enabled) {
            priv__push_task(acq_imu_task);
            priv__push_audio_task(state);

            priv__push_task(log_imu_task);
            priv__push_audio_task(state);
        }
    }

    // log_gps/parse_gps
    if ((MISSION_STATE_RECORD_DIVE != state)) {
        if (tag_config.gps.enabled) {
            priv__push_task(log_gps_task);
            priv__push_audio_task(state);
        }
    }

    // log_battery
    if (MISSION_STATE_LOW_POWER_RETRIEVE != state) {
        if (tag_config.battery.enabled) {
            priv__push_task(log_battery_task);
            priv__push_audio_task(state);
        }
    }

    // log_pressure
    if (MISSION_STATE_LOW_POWER_RETRIEVE != state) {
        if (tag_config.pressure.enabled) {
            priv__push_task(log_pressure_task);
            priv__push_audio_task(state);
        }
    }

    // tx_gps
    if ((MISSION_STATE_RECORD_SURFACE != state)
        && (MISSION_STATE_RECORD_DIVE != state)
    ) {
        if (tag_config.argos.enabled) {
            priv__push_task(priv__transmit_gps_task);
            priv__push_audio_task(state);
        }
    }

}

/// @brief Reconfigures tag for next mission state and updates the current state
/// @param next_state
void mission_set_state(MissionState next_state, MissionTransitionCause cause) {
    if (s_state == next_state) {
        return; // nothing to do
    }
    
    uint32_t sys_to_enable = (s_enabled_subsystems & s_state_active_subsystems_rule[next_state]) & ~(s_active_subsystems);
    uint32_t sys_to_disable = s_active_subsystems & ~(s_enabled_subsystems & s_state_active_subsystems_rule[next_state]);
    
    // Imu
    if (EN_IMU & sys_to_disable) {
        acq_imu_stop_all();
        log_imu_deinit();
        s_active_subsystems &= ~EN_IMU;
    } else if (EN_IMU & sys_to_enable) {
        for (int sensor_indx = 0; sensor_indx < IMU_SENSOR_COUNT; sensor_indx++) {
            if (!tag_config.imu.sensor[sensor_indx].enabled) {
                continue;
            }
            [[maybe_unused]]int ret;
            ret = acq_imu_start_sensor(sensor_indx, (uint32_t)tag_config.imu.sensor[sensor_indx].samplerate_ms * 1000);
        }
        s_active_subsystems |= EN_IMU;
    }

    // Pressure
    if (EN_PRESSURE & sys_to_disable) {
        acq_pressure_deinit();
        log_pressure_deinit();
        s_active_subsystems &= ~EN_PRESSURE;
    } else if (EN_PRESSURE & sys_to_enable) {
        acq_pressure_start();
        s_active_subsystems |= EN_PRESSURE;
    }

    // Burnwire
    if (EN_BURN & sys_to_disable) {
        burnwire_off();
        s_active_subsystems &= ~EN_BURN;
    } else if (EN_BURN & sys_to_enable) {
        burnwire_on();
        s_active_subsystems |= EN_BURN;
    }

    // GPS
    if (tag_config.gps.enabled) {
        switch (next_state) {
            case MISSION_STATE_RECORD_DIVE:
                // No GPS during dive
                gps_standby();
                break;

            case MISSION_STATE_LOW_POWER_RETRIEVE:
                // low data rate GPS (only need valid navigation solution every ~90 seconds)
                gps_wake();
                gps_low_data_rate();
                break;

            default:
                // record GPS
                gps_wake();
                gps_high_data_rate();
                break;
        }
    }

    // Satellite
    if (EN_ARGOS & sys_to_disable) {
        argos_tx_mgr_disable();
        s_active_subsystems &= ~EN_ARGOS;
    } else if (EN_ARGOS & sys_to_enable) {
        ArgosTxStrategy strategy = (tag_config.argos.pass_prediction_enabled) ? ARGOS_TX_STRATEGY_PATH_PREDICTOR: ARGOS_TX_STRATEGY_TIMER; 
        argos_tx_mgr_enable(strategy, tag_config.argos.transmission_interval_s, tag_config.argos.transmission_variance_percentage);
        s_active_subsystems |= EN_ARGOS;
    }

    // ECG
    if (EN_ECG & sys_to_disable) {
        acq_ecg_deinit();
        log_ecg_deinit();
        s_active_subsystems &= ~EN_ECG;
    } else if (EN_ECG & sys_to_enable) {
        acq_ecg_start();
        s_active_subsystems |= EN_ECG;
    }

    // Audio
    if (EN_AUDIO & sys_to_disable) {
        acq_audio_disable();
        log_audio_disable();
        s_active_subsystems &= ~EN_AUDIO;
    } else if (EN_AUDIO & sys_to_enable){
        acq_audio_start(log_audio_buffer, AUDIO_LOG_BUFFER_SIZE_BLOCKS, AUDIO_LOG_BLOCK_SIZE);
        s_active_subsystems |= EN_AUDIO;
    }

    // update state dependent tasks
    priv__update_state_dependent_task_list(next_state);

    // Log state transition
    mission_log_state_transition(s_state, next_state, cause);
    if (MISSION_STATE_LOW_POWER_RETRIEVE == next_state) {
        mission_log_deinit(); // no state transitions after this state
    }

    // update state
    s_state = next_state;
    // LED
    if ((MISSION_STATE_RECORD_DIVE == next_state) || (MISSION_STATE_RETRIEVE == next_state) || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)) {
        led_heartbeat_dim();
    } else if ((MISSION_STATE_BURN == next_state) || (MISSION_STATE_LOW_POWER_BURN == next_state)) {
        led_burn();
    } else {
        led_heartbeat();
    }

}

/// @brief Mission 1 second periodic timer IRQ
void TIM5_IRQHandler(void){
  HAL_TIM_IRQHandler(&mission_htim);
}

static void priv__periodic_timer_msp_deinit(TIM_HandleTypeDef* tim_baseHandle) {
    __HAL_RCC_MISSION_TIM_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(MISSION_TIM_IRQn);
}

static void priv__periodic_timer_msp_init(TIM_HandleTypeDef* tim_baseHandle) {
    __HAL_RCC_MISSION_TIM_CLK_ENABLE();
    HAL_NVIC_SetPriority(MISSION_TIM_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(MISSION_TIM_IRQn);
}

static void priv__periodic_timer_complete_callback(TIM_HandleTypeDef* tim_baseHandle) {
    if (s_update_periodic_mission_tasks == 1) {
        // ToDo: Handle Error
    }
    s_update_periodic_mission_tasks = 1;
}

/// @brief set a 1 second timer for periodic tasks
/// @param  
static void priv__init_periodic_task_timer(void){
    s_update_periodic_mission_tasks = 0;


    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_BASE_MSPINIT_CB_ID, priv__periodic_timer_msp_init);
    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, priv__periodic_timer_msp_deinit);

    mission_htim.Instance = TIM5;
    mission_htim.Init.Prescaler = 15999;
    mission_htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    mission_htim.Init.Period = 10000;
    mission_htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    mission_htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&mission_htim) != HAL_OK)
    {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&mission_htim, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&mission_htim, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, priv__periodic_timer_complete_callback);
}

/// @brief Initialize mission state machine
/// @param
void mission_init(void) {
    // hardware_mask
    s_enabled_subsystems = (tag_config.hw_config.audio.available * EN_AUDIO)
        | (tag_config.hw_config.argos.available * EN_ARGOS)
        | (tag_config.hw_config.bms.available * EN_BMS)
        | (tag_config.hw_config.burnwire.available * EN_BURN)
        | (tag_config.hw_config.ecg.available * EN_ECG)
        | (tag_config.hw_config.flasher.available * EN_FLASH)
        | (tag_config.hw_config.water_sensor.available * EN_WATER_SENSE)
        | (tag_config.hw_config.gps.available * EN_GPS)
        | (tag_config.hw_config.imu.available * EN_IMU)
        | (tag_config.hw_config.pressure.available * EN_PRESSURE)
        | (tag_config.hw_config.vhf_pinger.available * EN_VHF)
        ;

    // software mask
    s_enabled_subsystems &= ((tag_config.audio.enabled * EN_AUDIO)
            | (tag_config.argos.enabled * EN_ARGOS)
            | (tag_config.battery.enabled * EN_BMS)
            | (tag_config.burnwire.enabled * EN_BURN)
            | (tag_config.ecg.enabled * EN_ECG)
            | (tag_config.flasher.enabled * EN_FLASH)
            | (tag_config.hw_config.water_sensor.available * EN_WATER_SENSE)
            | (tag_config.gps.enabled * EN_GPS)
            | (tag_config.imu.enabled * EN_IMU)
            | (tag_config.pressure.enabled * EN_PRESSURE)
            | (tag_config.vhf.enabled * EN_VHF)
        )
        ;

    if (s_enabled_subsystems & EN_AUDIO) {
        CETI_LOG("Initializing Audio Logging");
        // link log_audio to acq_audio
        log_audio_init(&tag_config.audio);
        acq_audio_register_block_complete_callback(log_audio_block_complete_callback);
    }

    /* perform runtime system hardware test to detect available systems */
    if (s_enabled_subsystems & EN_BMS){
        CETI_LOG("Initializing BMS Logging");
        log_battery_init();
        acq_battery_register_callback(log_battery_buffer_sample); // link logging to acquisition
    }

    if (s_enabled_subsystems & EN_PRESSURE) {
        CETI_LOG("Initializing Pressure Logging");
        s_dive_threshold_raw = acq_pressure_bar_to_pressure_raw(tag_config.pressure.dive_threshold_bar);
        s_surface_threshold_raw = acq_pressure_bar_to_pressure_raw(tag_config.pressure.surface_threshold_bar);
        log_pressure_init();
        acq_pressure_register_sample_callback(log_pressure_buffer_sample);
    }

    if (s_enabled_subsystems & EN_IMU) {
        CETI_LOG("Initializing IMU Logging");
        log_imu_init();
        const ImuCallback cb[] = {
            [IMU_SENSOR_ACCELEROMETER] = log_imu_accel_sample_callback,
            [IMU_SENSOR_GYROSCOPE] = log_imu_gyro_sample_callback,
            [IMU_SENSOR_MAGNETOMETER] = log_imu_mag_sample_callback,
            [IMU_SENSOR_ROTATION] = log_imu_quat_sample_callback,
        };
        for (int sensor_indx = 0; sensor_indx < IMU_SENSOR_COUNT; sensor_indx++) {
            if (!tag_config.imu.sensor[sensor_indx].enabled) {
                continue;
            }
            acq_imu_register_callback(sensor_indx, cb[sensor_indx]);
        }
    }

    if (s_enabled_subsystems & EN_ECG) {
        CETI_LOG("Initializing ECG Logging");
        log_ecg_init();
        acq_ecg_register_sample_callback(log_ecg_push_sample);
    }

    if (s_enabled_subsystems & EN_GPS) {
        CETI_LOG("Initializing GPS Logging");
        log_gps_init();
        gps_register_msg_complete_callback(priv__gps_msg_callback);
    }

    if (s_enabled_subsystems & EN_ARGOS) {
        CETI_LOG("Initializing ARGOS");
        satellite_start(&tag_config.argos);
        log_argos_init();
    }

    if (s_enabled_subsystems & EN_FLASH) {
        CETI_LOG("Enabling Antenna Flasher");
    }

    if (s_enabled_subsystems & EN_WATER_SENSE) {
        CETI_LOG("Enabling Antenna Flasher");
    }

    error_queue_init(); // initialize error queue so we can log when issues occur

    mission_battery_init();

    if (s_enabled_subsystems & EN_BURN) {
        CETI_LOG("Initializing Burnwire");
        uint64_t now_timestamp_s = rtc_get_epoch_us()/1000000;
        uint64_t seconds_until_timer_release = -1; // set to max
        uint64_t seconds_until_time_of_day_release = -1;  // set to max
        s_burn_start_timestamp_s = -1; //set to mx

        // calculate time of day release
        if (tag_config.mission.time_of_day_release_utc.enabled) {
            RTC_TimeTypeDef rtc_time;
            rtc_get_datetime(NULL, &rtc_time);
            uint64_t target_tod_hours = tag_config.mission.time_of_day_release_utc.hour;
            uint64_t target_tod_minutes = ( 60 * target_tod_hours ) +  tag_config.mission.time_of_day_release_utc.minute;
            uint64_t target_tod_seconds = ( 60 * target_tod_minutes );

            uint64_t actual_tod_seconds = ( 60 * ((60 * rtc_time.Hours) + rtc_time.Minutes) + rtc_time.Seconds);
            if (actual_tod_seconds > target_tod_seconds) {
                target_tod_seconds += 24*60*60;
            }
            
            seconds_until_time_of_day_release = (target_tod_seconds - actual_tod_seconds);            
        }

        // calculate timer release
        if (tag_config.mission.timer_release.enabled) {
            uint64_t hours = tag_config.mission.timer_release.hours;
            uint64_t minutes = ( 60 * hours ) + tag_config.mission.timer_release.minutes;
            seconds_until_timer_release = 60 * minutes;
        }

        // start burn timer
        if ((-1 == seconds_until_time_of_day_release) && (-1 == seconds_until_timer_release)) {
            s_burn_start_timestamp_s = -1; // no burner set
            CETI_LOG("Burn Timer: Not set");
        } else {
            if (seconds_until_time_of_day_release < seconds_until_timer_release) {
                s_burn_start_timestamp_s = now_timestamp_s + seconds_until_time_of_day_release;
                CETI_LOG("Burn Timer: Set to Time of Day Release: %lld (%lld s)", s_burn_start_timestamp_s, seconds_until_time_of_day_release);
            } else {
                s_burn_start_timestamp_s = now_timestamp_s + seconds_until_timer_release;
                CETI_LOG("Burn Timer: Set to Timed Release: %lld (%lld s)", s_burn_start_timestamp_s, seconds_until_timer_release);
            }
        }
    }

    priv__init_periodic_task_timer();

    // force state transition
    mission_log_init(); // initialize mission log prior to performing initial state transistion.
    s_state = MISSION_STATE_MISSION_START;
    mission_set_state(STARTING_STATE, MISSION_TRANSITION_START);

    // Battery acquisition performed for all states
    acq_battery_start();
    HAL_TIM_Base_Start_IT(&mission_htim);
}
#endif

/// @brief determines the next state the tag should transition into
/// @param current_state
/// @return
static MissionState priv__mission_get_next_state(MissionState current_state, MissionTransitionCause *transition_cause) {
    if (current_state == MISSION_STATE_MISSION_START) {
        if (NULL != transition_cause) {
            *transition_cause = MISSION_TRANSITION_START;
        }
        return tag_config.mission.starting_state;
    }
    
    MissionState next_state = current_state;
    MissionTransitionCause internal_cause = MISSION_TRANSITION_NONE;
    for (size_t i = 0; i < ARRAY_LEN(s_transition_table); i++) {
        const MissionTransitionRule *rule = &s_transition_table[i];
        if (rule->from != current_state) {
            continue;
        }

        if (rule->condition == NULL || rule->condition()) {
            internal_cause = rule->cause;
            next_state = rule->to;
            break;
        }
    }

    if (NULL != transition_cause) {
        *transition_cause = internal_cause;
    }
    return next_state;
}

#ifndef UNIT_TEST
/// @brief updates the current system state based on latest system interaction
/// @param
void mission_task(void) {
    /* state dependent tasks */
    for(int i = 0; i < s_state_dependent_tasks.count; i++) {
        s_state_dependent_tasks.tasks[i]();
    }
    
    /* state independent tasks */
    // Check if tag is stuck not transmitting GPS via ARGOS
    priv__wdt_task();

    if(s_update_periodic_mission_tasks) {
        priv__resyncronize_rtc_task();

        priv__update_argos_coordinates();

        mission_battery_task();

        if (EN_FLOAT & s_active_subsystems) {
            // Update float detection
            sh2_SensorValue_t rotation;
            acq_imu_get_rotation(&rotation); 
            float_detection_push_rotation(&rotation.un.rotationVector);
        }

        // Update the mission state
        MissionTransitionCause cause = MISSION_TRANSITION_NONE;
        MissionState next_state = priv__mission_get_next_state(s_state, &cause);
        mission_set_state(next_state, cause);
        s_update_periodic_mission_tasks = 0;
    }

    // Store any errors to file
    error_queue_flush();

    syslog_flush(); // flush the syslog
#ifdef BENCHMARK
    profile_flush();
#endif
}

/// @brief call after mission_task to put the system to sleep until
///        the system receives another input signal
/// @param
void mission_sleep(void) {
    if (MISSION_SLEEP_NONE == s_state_sleep_depth[s_state]){
        return;
    }

    __disable_irq();
    if (   !((s_active_subsystems & EN_BMS) && log_battery_sample_buffer_is_half_full())
        && !((s_active_subsystems & EN_IMU) && log_imu_any_buffer_half_full())
        && !((s_active_subsystems & EN_ECG) && log_ecg_sample_buffer_is_half_full())
        && !((s_active_subsystems & EN_PRESSURE) && log_pressure_sample_buffer_is_half_full())
        && !((s_active_subsystems & EN_ARGOS) && argos_tx_mgr_ready_to_tx())
        && !((s_active_subsystems & EN_GPS) && log_gps_buffer_is_half_full())
        && !s_update_periodic_mission_tasks
    ) {
        // nothing to currently do!
        // go to sleep until next interrupt
        switch(s_state_sleep_depth[s_state]) {
            case MISSION_SLEEP_NONE:
                break;

            case MISSION_SLEEP_SLEEP:
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                HAL_ResumeTick();
                break;

            case MISSION_SLEEP_STOP1:
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);
                SystemClock_Config();
                HAL_ResumeTick();
                break;

            case MISSION_SLEEP_STOP2:
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
                SystemClock_Config();
                HAL_ResumeTick();
                break;
        }
    }
    __enable_irq();
}

#endif