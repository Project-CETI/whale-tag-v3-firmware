//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "mission.h"

#include "audio/acq_audio.h"
#include "audio/log_audio.h"
#include "battery/acq_battery.h"
#include "battery/log_battery.h"
#include "burnwire.h"
#include "ecg/acq_ecg.h"
#include "error.h"
#include "gps/acq_gps.h"
#include "gps/log_gps.h"
#include "imu/acq_imu.h"
#include "imu/log_imu.h"
#include "led/led.h"
#include "metadata.h"
#include "pressure/acq_pressure.h"
#include "pressure/log_pressure.h"
#include "satellite/argos_tx_mgr.h"
#include "satellite/satellite.h"
#include "syslog.h"

#include "main.h"
#include <usart.h>

#include "misson/mission_battery.h"
#include "misson/mission_log.h"

extern void SystemClock_Config(void);
extern I2C_HandleTypeDef hi2c3;

#define MISSION_DIVE_THRESHOLD_BAR (5.0)
#define MISSION_SURFACE_THRESHOLD_BAR (1.0)
#define MISSION_BURN_THRESHOLD_CELL_V (3.3)
#define MISSION_CRITICAL_THRESHOLD_CELL_V (3.2)
#define MISSION_BURNWIRE_BURN_PERIOD_MIN (20)

#define ARRAY_LEN(x) (sizeof((x)) / sizeof(*(x)))

typedef void (*MissionTask)(void);

static MissionState s_state = MISSION_STATE_ERROR;
static volatile uint8_t s_update_periodic_mission_tasks = 0;
TIM_HandleTypeDef mission_htim;

static struct {
    uint8_t valid;
    uint8_t year;
    uint8_t month;
    uint8_t day;     // (0..31]
    uint8_t hours;   // (0..24]
    uint8_t minutes; // (0..60]
    uint8_t seconds; // (0..60] // used for RTC sync not logging
    uint8_t latitude_sign;
    uint8_t longitude_sign;
    // uint8_t unused[3];
    float latitude;
    float longitude;
} latest_gps_coord = {
    .valid = 0,
};

/* PRESSURE CONTROLS *********************************************************/
static int __is_low_pressure(void) {
    if (!tag_config.pressure.enabled){
        return 1;
    }
    CetiPressureSample pressure_sample;
    acq_pressure_get_latest(&pressure_sample);
    return (pressure_sample.data.pressure < KELLER4LD_PRESSURE_BAR_TO_RAW(MISSION_SURFACE_THRESHOLD_BAR));
}

static int __is_high_pressure(void) {
    if (!tag_config.pressure.enabled){
        return 0;
    }
    CetiPressureSample pressure_sample;
    acq_pressure_get_latest(&pressure_sample);
    return (pressure_sample.data.pressure >= KELLER4LD_PRESSURE_BAR_TO_RAW(MISSION_DIVE_THRESHOLD_BAR));
}

/* BURN CONTROLS *************************************************************/

static uint8_t s_burn_started = 0;
static uint64_t s_burn_start_timestamp_s = 0;

/// @brief check if burn should be activated based on timeout
/// @param
/// @return bool
static int __is_time_to_burn(void) {
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
static int __is_burn_complete(void) {
    if (!s_burn_started) {
        return 0;
    }
    uint64_t elapsed_time = rtc_get_epoch_us()/1000000 - s_burn_start_timestamp_s;
    return (elapsed_time > MISSION_BURNWIRE_BURN_PERIOD_MIN * 60);
}

/* BURN CONTROLS *************************************************************/

/// @brief checks whether the tag is floating in the water
/// @param
/// @return bool - true tag floating; false tag not floating
static int __is_floating(void) {
#warning "ToDo: implement __is_floating()"
    return 0;
}

/// @brief Transmits an input to satelite, and generates log of message sent
/// @param message pointer to input array
/// @param message_len length of input array
static void __satellite_transmit_from_raw_with_log(RecoveryArgoModulation rconf, const uint8_t *message, uint8_t message_len) {
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

    // buffer for ascii representaion of input
    char tx_message_ascii[(2 * 24) + 1];

    // add trailing zeros to message if message less than max message length
    uint8_t sized_message_buffer[24] = {0};
    memcpy(sized_message_buffer, message, message_len);

    // convert hex to ascii representation of hex
    for (int i = 0; i < max_len; i++) {
        snprintf(&tx_message_ascii[2 * i], 3, "%02X", sized_message_buffer[i]);
    }

    // perform transmission
    satellite_transmit(tx_message_ascii, 2 * max_len); // transmit via argos
    CETI_LOG("Tx Argos: %s", tx_message_ascii);
#warning "ToDo: Log ARGOS transmission" // log transmission
}


/* GPS TASKS *****************************************************************/
/// @brief  Parses RMC NMEA messages extracting relavent fields
/// @param sentence pointer to RMC NMEA string
/// @return 0 == invalid RMC NMEA message; 1 == valid nmea message with valid coord
/// @note this was modified from minmea (https://github.com/kosma/minmea) to be
/// degeneralized and reduce the amount of work
static int __parse_rmc(const uint8_t *sentence) {
    const uint8_t *field = sentence;
    uint8_t year;
    uint8_t month;
    uint8_t day;     // (1..31]
    uint8_t hours;   // (0..24]
    uint8_t minutes; // (0..60]
    uint8_t seconds; // (0..60] // used for RTC sync not logging
    uint8_t latitude_sign;
    uint8_t longitude_sign;
    // uint8_t unused[3];
    float latitude;
    float longitude;

#define isfield(c) (isprint((unsigned char)(c)) && (c) != ',' && (c) != '*')

#define next_field()              \
    do {                          \
        while (isfield(*field)) { \
            field++;              \
        }                         \
        if (',' != *field) {      \
            return 0;             \
        } else {                  \
            field++;              \
        }                         \
    } while (0)

    // t - type
    if ('$' != field[0]) {
        return 0;
    }
    for (int i = 0; i < 5; i++) {
        if (!isfield(field[1 + i])) {
            return 0;
        }
    }
    if (0 != memcmp("RMC", &field[3], 3)) {
        return 0;
    }
    field += 5;

    // check validity
    next_field();
    const char *time_str = (char *)field; // skip time field for now

    next_field();
    if ('A' != *field) {
        return 0;
    }
    next_field();

    // frame is valid RMC. parse!!!

    // parse meaningful values;
    // Minimum required: integer time.
    { // hour and minute
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)time_str[f]))
                return 0;
        }

        char hArr[] = {time_str[0], time_str[1], '\0'};
        char mArr[] = {time_str[2], time_str[3], '\0'};
        char sArr[] = {time_str[4], time_str[5], '\0'};

        hours = strtol(hArr, NULL, 10);
        minutes = strtol(mArr, NULL, 10);
        seconds = strtol(sArr, NULL, 10);
    }

    { // latitude
        for (int f = 0; f < 4; f++) {
            if (!isdigit((unsigned char)field[f]))
                return 0;
        }
        char dArr[] = {field[0], field[1], 0};
        char mArr[] = {field[2], field[3], 0};
        uint8_t degrees = strtol(dArr, NULL, 10);
        uint8_t minutes = strtol(mArr, NULL, 10);
        float minutes_f = (float)minutes;

        uint32_t subminutes = 0;
        uint32_t scale = 1;
        if ('.' == field[4]) {
            for (int i = 5; isdigit((unsigned char)field[i]); i++) {
                subminutes = (subminutes * 10) + (field[i] - '0');
                scale *= 10;
            }
        }
        minutes_f += ((float)subminutes) / (float)scale;
        latitude = (float)degrees + (minutes_f / 60.0f);
    }

    next_field();
    { // latitude sign
        if ('S' == *field) {
            latitude_sign = 1;
        } else if ('N' == *field) {
            latitude_sign = 0;
        } else {
            return 0;
        }
    }

    next_field();
    { // longitude
        for (int f = 0; f < 5; f++) {
            if (!isdigit((unsigned char)field[f])) {
                return 0;
            }
        }
        char dArr[] = {field[0], field[1], field[2], 0};
        char mArr[] = {field[3], field[4], 0};
        uint8_t degrees = strtol(dArr, NULL, 10);
        uint8_t minutes = strtol(mArr, NULL, 10);
        float minutes_f = (float)minutes;

        uint32_t subminutes = 0;
        uint32_t scale = 1;
        if ('.' == field[5]) {
            for (int i = 6; isdigit((unsigned)field[i]); i++) {
                subminutes = (subminutes * 10) + (field[i] - '0');
                scale *= 10;
            }
        }
        minutes_f += ((float)subminutes) / (float)scale;
        longitude = (float)degrees + (minutes_f / 60.0f);
    }

    next_field();
    { // longitude_sign
        if ('W' == *field) {
            longitude_sign = 1;
        } else if ('E' == *field) {
            longitude_sign = 0;
        } else {
            return 0;
        }
    }

    next_field();
    { // speed
    }

    next_field();
    { // course
    }

    next_field();
    { // Date
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)field[f]))
                return 0;
        }
        char dArr[] = {field[0], field[1], 0};
        char mArr[] = {field[2], field[3], 0};
        char yArr[] = {field[4], field[5], 0};
        day = strtol(dArr, NULL, 10);
        month = strtol(mArr, NULL, 10);
        year = strtol(yArr, NULL, 10);
    }

    // everything parsed OK
    latest_gps_coord.valid = 1;
    latest_gps_coord.year = year;
    latest_gps_coord.month = month;
    latest_gps_coord.day = day;         // (0..31]
    latest_gps_coord.hours = hours;     // (0..24]
    latest_gps_coord.minutes = minutes; // (0..60]
    latest_gps_coord.seconds = seconds; // (0..60] // used for RTC sync not logging
    latest_gps_coord.latitude_sign = latitude_sign;
    latest_gps_coord.longitude_sign = longitude_sign;
    latest_gps_coord.latitude = latitude;
    latest_gps_coord.longitude = longitude;
    return 1;
}

/// @brief  Task that updates latest valid GPS coordinates, forwards all gps
/// to a file, and resyncronizes the RTC if it hasn't been yet;
/// @param
static void __parse_gps_task(void) {
    const uint8_t *gps_sentence = gps_pop_sentence();
    while (NULL != gps_sentence) {
        // validate position
        int new_valid_rmc = __parse_rmc(gps_sentence);

        // synchronize RTC
        if (!rtc_has_been_syncronized() && latest_gps_coord.valid) {
            RTC_TimeTypeDef sTime = {
                .Hours = latest_gps_coord.hours,
                .Minutes = latest_gps_coord.minutes,
                .Seconds = latest_gps_coord.seconds,
            };
            RTC_DateTypeDef sDate = {
                .Year = latest_gps_coord.year,
                .Date = latest_gps_coord.day,
                .Month = latest_gps_coord.month,
            };
            rtc_set_datetime(&sDate, &sTime);
        }

        // update argos_tx_mgr's position
        if (new_valid_rmc) {
            float lat = latest_gps_coord.latitude;
            if (latest_gps_coord.latitude_sign) {
                lat = -lat;
            }
            float lon = latest_gps_coord.longitude;
            if (latest_gps_coord.longitude_sign) {
                lon = 360.0f - lon;
            }
            argos_tx_mgr_set_coordinates(lat, lon);
        }

        // forward to host
        // #warning "ToDo: Log GPS Sentences"

        // next
        gps_sentence = gps_pop_sentence();
    }
}

/// @brief Task that performs transmission of GPS coordinates via argos satellite and forwarding
///        transmission to host system as a nmea packet for logging purposes
/// @param
static void __transmit_gps_task(void) {
    static uint32_t tx_count = 0;
    if (!argos_tx_mgr_ready_to_tx()) {
        return;
    }

    // construct tx_message
    uint32_t lat_abs = 0xffffffff;
    uint32_t lon_abs = 0xffffffff;
    if (!latest_gps_coord.valid) {
        RTC_DateTypeDef rtc_date;
        RTC_TimeTypeDef rtc_time;
        rtc_get_datetime(&rtc_date, &rtc_time);
        latest_gps_coord.day = rtc_date.Date;
        latest_gps_coord.hours = rtc_time.Hours;
        latest_gps_coord.minutes = rtc_time.Minutes;
        latest_gps_coord.latitude_sign = 1;
        latest_gps_coord.longitude_sign = 1;
    } else {
        lat_abs = (uint32_t)(latest_gps_coord.latitude * 10000.0f);
        lon_abs = (uint32_t)(latest_gps_coord.longitude * 10000.0f);
    }

    // generate gps packet
    uint64_t gps_packet = 0;
    gps_packet = (((uint64_t)(latest_gps_coord.day & ((1 << 5) - 1))) << (64 - 5)) | (((uint64_t)(latest_gps_coord.hours & ((1 << 5) - 1))) << (64 - 10)) | (((uint64_t)(latest_gps_coord.minutes & ((1 << 6) - 1))) << (64 - 16)) | (((uint64_t)(latest_gps_coord.latitude_sign & ((1 << 1) - 1))) << (64 - 17)) | (((uint64_t)(lat_abs & ((1 << 20) - 1))) << (64 - 37)) | (((uint64_t)(latest_gps_coord.longitude_sign & ((1 << 1) - 1))) << (64 - 38)) | (((uint64_t)(lon_abs & ((1 << 21) - 1))) << (64 - 59));

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

        __satellite_transmit_from_raw_with_log(rconf, tx_message, max_len);
    } else {
        // VLDA4 only consists of 3 bytes, so skip the data info to only transmit GPS coord
        __satellite_transmit_from_raw_with_log(rconf, &tx_message[1], 3);
    }
    argos_tx_mgr_inval_ready_to_tx(); // reset transmission timer
    tx_count++;                       // update transmission count
}

/// @brief this task reverts the recovery board to its "active state" if it is
/// likely the state_machine is stuck in an "unsafe" state with no host
/// communication
static void __wdt_task(void) {
#warning "ToDo: implement __wdt_task"
}

/* MISSION STATE MACHINE *****************************************************/


static struct {
    uint8_t count;
    MissionTask tasks[64];
} s_state_dependent_tasks = {
    .count = 0,
    .tasks = {NULL},
};

static inline void __push_task(MissionTask task) {
    s_state_dependent_tasks.tasks[s_state_dependent_tasks.count] = task; // always check if audio needs to be logged
    s_state_dependent_tasks.count++;
}

static inline void __push_audio_task(MissionState state) {
    if(tag_config.audio.enabled) {
        if ((MISSION_STATE_LOW_POWER_BURN != state)
            && (MISSION_STATE_LOW_POWER_RETRIEVE != state)
        ) {
            __push_task(log_audio_task); // always check if audio needs to be logged
        }
    }
}

static void __update_state_dependent_task_list(MissionState state) {
    s_state_dependent_tasks.count = 0;

    __push_audio_task(state);

    // log_imu
    if ((MISSION_STATE_LOW_POWER_BURN != state)
        && (MISSION_STATE_LOW_POWER_RETRIEVE != state)
    ) {
        if (tag_config.imu.enabled) {
            __push_task(acq_imu_task);
            __push_audio_task(state);

            __push_task(log_imu_task);
            __push_audio_task(state);
        }
    }

    // log_gps/parse_gps
    if ((MISSION_STATE_RECORD_DIVE != state)) {
        if (tag_config.gps.enabled) {
            __push_task(log_gps_task);
            __push_audio_task(state);

            __push_task(__parse_gps_task);
            __push_audio_task(state);
        }
    }

    // log_battery
    if (MISSION_STATE_LOW_POWER_RETRIEVE != state) {
        if (tag_config.battery.enabled) {
            __push_task(log_battery_task);
            __push_audio_task(state);
        }
    }

    // log_pressure
    if (MISSION_STATE_LOW_POWER_RETRIEVE != state) {
        if (tag_config.pressure.enabled) {
            __push_task(log_pressure_task);
            __push_audio_task(state);
        }
    }

    // tx_gps
    if ((MISSION_STATE_RECORD_SURFACE != state)
        && (MISSION_STATE_RECORD_DIVE != state)
    ) {
        if (tag_config.argos.enabled) {
            __push_task(__transmit_gps_task);
            __push_audio_task(state);
        }
    }

}

/// @brief Reconfigures tag for next mission state and updates the current state
/// @param next_state
void mission_set_state(MissionState next_state, MissionTransitionCause cause) {
    /*
                          | RS | RF | RD | B | LPB | R | LPR
        audio             | X  | X  | X  | X |     | X |
        imu               | X  | X  | X  | X |     | X |
        burn              |    |    |    | X |  X  |   |
        gps               | X  | X  |    | X |  X  | X | X
        led               | X  | X  |    | X |     | X |
        recovery_tx       |    | X  |    | X |  X  | X | X
        bms               | x  | x  | x  | x |  x  | x |
        pressure          | x  | x  | x  | x |  x  | x |
    */
    if (s_state == next_state) {
        return; // nothing to do
    }

    // Imu
    if (tag_config.imu.enabled) {
        if ((MISSION_STATE_LOW_POWER_BURN == next_state) 
            || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)
        ) {
            acq_imu_stop_all();
            log_imu_deinit();

        } else {
            const ImuCallback cb[] = {
                [IMU_SENSOR_ACCELEROMETER] = log_imu_accel_sample_callback,
                [IMU_SENSOR_GYROSCOPE] = log_imu_gyro_sample_callback,
                [IMU_SENSOR_MAGNETOMETER] = log_imu_mag_sample_callback,
                [IMU_SENSOR_ROTATION] = log_imu_quat_sample_callback,
            };
            for (int sensor_indx = 0; sensor_indx < IMU_SENSOR_COUNT; sensor_indx++) {
                if (tag_config.imu.sensor[sensor_indx].enabled) {
                    continue;
                }
                acq_imu_register_callback(sensor_indx, cb[sensor_indx]);
                int ret = acq_imu_start_sensor(sensor_indx, (uint32_t)tag_config.imu.sensor[sensor_indx].samplerate_ms * 1000);
            }
        }
    }

    // Pressure
    if (tag_config.pressure.enabled) {
        if (MISSION_STATE_LOW_POWER_RETRIEVE == next_state) {
            acq_pressure_deinit();
            log_pressure_deinit();
        } else {
            acq_pressure_start();
        }
    }

    // Burnwire
    if (tag_config.burnwire.enabled) {
        if ((MISSION_STATE_BURN == next_state) || (MISSION_STATE_LOW_POWER_BURN == next_state)) {
            burnwire_on();
        } else {
            burnwire_off();
        }
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
    if (tag_config.argos.enabled) {
        if ((MISSION_STATE_RECORD_SURFACE == next_state) || (MISSION_STATE_RECORD_DIVE == next_state)) {
            argos_tx_mgr_disable();
        } else {
            argos_tx_mgr_enable();
        }
    }

    // ECG
    if (tag_config.ecg.enabled) {
        if ((MISSION_STATE_LOW_POWER_BURN == next_state)
            || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)
        ) {
        	acq_ecg_deinit();
            // ToDo: disable ecg acquisiton
        } else {
            // ToDo: linking ecg longging to acquisition callback
        	acq_ecg_start();
        }
    }

    // Audio
    if (tag_config.audio.enabled) {
        if ((MISSION_STATE_LOW_POWER_BURN == next_state)
            || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)
        ) {
            acq_audio_disable();
            log_audio_disable();
        } else {
            acq_audio_start(log_audio_buffer, AUDIO_LOG_BUFFER_SIZE_BLOCKS, AUDIO_LOG_BLOCK_SIZE);
        }
    }

    // update state dependent tasks
    __update_state_dependent_task_list(next_state);

    // Log state transition
    mission_log_state_transition(s_state, next_state, cause);
    if (MISSION_STATE_LOW_POWER_RETRIEVE == next_state) {
        mission_log_deinit(); // no state transitions after this state
    }

    // periodic state_machine task timer
    if (MISSION_STATE_MISSION_START == next_state) {
    	HAL_TIM_Base_Start_IT(&mission_htim);
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

void TIM5_IRQHandler(void){
  HAL_TIM_IRQHandler(&mission_htim);
}

static void __periodic_timer_msp_deinit(TIM_HandleTypeDef* tim_baseHandle) {
    __HAL_RCC_MISSION_TIM_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(MISSION_TIM_IRQn);
}

static void __periodic_timer_msp_init(TIM_HandleTypeDef* tim_baseHandle) {
    __HAL_RCC_MISSION_TIM_CLK_ENABLE();
    HAL_NVIC_SetPriority(MISSION_TIM_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(MISSION_TIM_IRQn);
}

static void __periodic_timer_complete_callback(TIM_HandleTypeDef* tim_baseHandle) {
    if (s_update_periodic_mission_tasks == 1) {
        // ToDo: Handle Error
    }
    s_update_periodic_mission_tasks = 1;
}

/// @brief set a 1 second timer for periodic tasks
/// @param  
static void __init_periodic_task_timer(void){
    s_update_periodic_mission_tasks = 0;


    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    
    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_BASE_MSPINIT_CB_ID, __periodic_timer_msp_init);
    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, __periodic_timer_msp_deinit);

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

    HAL_TIM_RegisterCallback(&mission_htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, __periodic_timer_complete_callback);
}

/// @brief Initialize mission state machine
/// @param
void mission_init(void) {
    if (tag_config.audio.enabled) {
        CETI_LOG("Initializing Audio Logging");
        // link log_audio to acq_audio
        log_audio_init(&tag_config.audio);
        acq_audio_register_block_complete_callback(log_audio_block_complete_callback);
    }

    /* perform runtime system hardware test to detect available systems */
    if (tag_config.battery.enabled){
        CETI_LOG("Initializing BMS Logging");
        log_battery_init();
        acq_battery_register_callback(log_battery_buffer_sample); // link logging to acquisition
    }

    if (tag_config.pressure.enabled) {
        CETI_LOG("Initializing Pressure Logging");
        log_pressure_init();
        acq_pressure_register_sample_callback(log_pressure_buffer_sample);
    }

    if (tag_config.imu.enabled) {
        CETI_LOG("Initializing IMU Logging");
        log_imu_init();
    }

    if (tag_config.ecg.enabled) {
        CETI_LOG("Initializing ECG Logging");
        // acq_ecg_start();
    }

    if (tag_config.argos.enabled) {
        CETI_LOG("Initializing ARGOS");
        MX_USART2_UART_Init();
        satellite_init();
    }

    if (tag_config.flasher.enabled) {
        CETI_LOG("Enabling Antenna Flasher");
    }

    error_queue_init(); // initialize error queue so we can log when issues occur

    mission_battery_init();

    uint64_t now_timestamp_s = rtc_get_epoch_us()/1000000;
    // start burn timer
    s_burn_start_timestamp_s = now_timestamp_s + 4 * 60 * 60;

    // initialize gps log
    log_gps_init();

    __init_periodic_task_timer();

    // force state transition
    mission_log_init(); // initialize mission log prior to performing initial state transistion.
    s_state = MISSION_STATE_ERROR;
    mission_set_state(STARTING_STATE, MISSION_TRANSITION_START);

    // Battery acquisition performed for all states
    acq_battery_start();
}

/// @brief determines the next state the tag should transition into
/// @param current_state
/// @return
static MissionState __mission_get_next_state(MissionState current_state, MissionTransitionCause transition_cause[static 1]) {
    switch (current_state) {
        case MISSION_STATE_MISSION_START: {
            *transition_cause = MISSION_TRANSITION_START;
            return MISSION_STATE_RECORD_SURFACE;
        }

        case MISSION_STATE_RECORD_SURFACE: {
            if (mission_battery_is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                *transition_cause = MISSION_TRANSITION_LOW_VOLTAGE;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (mission_battery_is_in_error()) {
                CETI_LOG("Entering burn due to BMS errors!!!");
                *transition_cause = MISSION_TRANSITION_BATTERY_ERRORS;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_time_to_burn()) {
                CETI_LOG("Entering burn due to timeout!!!");
                *transition_cause = MISSION_TRANSITION_TIMER ;
                return MISSION_STATE_BURN;
            }

            if (__is_high_pressure()) {
                *transition_cause = MISSION_TRANSITION_HIGH_PRESSURE;
                return MISSION_STATE_RECORD_DIVE;
            }

            if (__is_floating()) {
                *transition_cause = MISSION_TRANSITION_FLOAT_DETECTED;
                return MISSION_STATE_RECORD_FLOATING;
            }

            return MISSION_STATE_RECORD_SURFACE; // remain in current state
        } // case MISSION_STATE_RECORD_SURFACE

        case MISSION_STATE_RECORD_FLOATING: {
            if (mission_battery_is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                *transition_cause = MISSION_TRANSITION_LOW_VOLTAGE;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (mission_battery_is_in_error()) {
                CETI_LOG("Entering burn due to BMS errors!!!");
                *transition_cause = MISSION_TRANSITION_BATTERY_ERRORS;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_time_to_burn()) {
                CETI_LOG("Entering burn due to timeout!!!");
                *transition_cause = MISSION_TRANSITION_TIMER;
                return MISSION_STATE_BURN;
            }

            if (__is_high_pressure()) {
                *transition_cause = MISSION_TRANSITION_HIGH_PRESSURE;
                return MISSION_STATE_RECORD_DIVE;
            }

            if (!__is_floating()) {
                return MISSION_STATE_RECORD_SURFACE;
            }

            return MISSION_STATE_RECORD_FLOATING; // remain in current state
        } // case MISSION_STATE_RECORD_SURFACE

        case MISSION_STATE_RECORD_DIVE: {
            if (mission_battery_is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                *transition_cause = MISSION_TRANSITION_LOW_VOLTAGE;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (mission_battery_is_in_error()) {
                CETI_LOG("Entering burn due to BMS errors!!!");
                *transition_cause = MISSION_TRANSITION_BATTERY_ERRORS;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_time_to_burn()) {
                CETI_LOG("Entering burn due to timeout!!!");
                *transition_cause = MISSION_TRANSITION_TIMER;
                return MISSION_STATE_BURN;
            }

            if (__is_low_pressure()) {
                *transition_cause = MISSION_TRANSITION_LOW_PRESSURE;
                return MISSION_STATE_RECORD_SURFACE;
            }

            return MISSION_STATE_RECORD_DIVE; // remain in current state
        } // case MISSION_STATE_RECORD_DIVE

        case MISSION_STATE_BURN: {
            if (mission_battery_is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                *transition_cause = MISSION_TRANSITION_LOW_VOLTAGE;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (mission_battery_is_in_error()) {
                CETI_LOG("Entering burn due to BMS errors!!!");
                *transition_cause = MISSION_TRANSITION_BATTERY_ERRORS;
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_burn_complete()) {
                *transition_cause = MISSION_TRANSITION_TIMER;
                return MISSION_STATE_RETRIEVE;
            }
            return MISSION_STATE_BURN; // remain in current state
        } // case MISSION_STATE_BURN

        case MISSION_STATE_LOW_POWER_BURN: {
            if (__is_burn_complete()) {
                *transition_cause = MISSION_TRANSITION_TIMER;
                return MISSION_STATE_LOW_POWER_RETRIEVE;
            }

            return MISSION_STATE_LOW_POWER_BURN;
        } // case MISSION_STATE_LOW_POWER_BURN

        case MISSION_STATE_RETRIEVE: {
            if (mission_battery_is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                *transition_cause = MISSION_TRANSITION_LOW_VOLTAGE;
                return MISSION_STATE_LOW_POWER_RETRIEVE;
            }

            if (mission_battery_is_in_error()) {
                CETI_LOG("Entering burn due to BMS errors!!!");
                *transition_cause = MISSION_TRANSITION_BATTERY_ERRORS;
                return MISSION_STATE_LOW_POWER_RETRIEVE;
            }
            return MISSION_STATE_RETRIEVE;
        } // case MISSION_STATE_RETRIEVE

        case MISSION_STATE_LOW_POWER_RETRIEVE: {
            return MISSION_STATE_LOW_POWER_RETRIEVE;
        }

        case MISSION_STATE_ERROR: {
            // ToDo: Handle error
            return MISSION_STATE_RECORD_SURFACE;
        }
        default: {
            return MISSION_STATE_ERROR;
        }
    }
}

/// @brief updates the current system state based on latest system interaction
/// @param
void mission_task(void) {
    /* state dependent tasks */
    for(int i = 0; i < s_state_dependent_tasks.count; i++) {
        s_state_dependent_tasks.tasks[i]();
    }
    
    /* state independent tasks */
    // Check if tag is stuck not transmitting GPS via ARGOS
    __wdt_task();

    if(s_update_periodic_mission_tasks) {
        mission_battery_task();

        // Update the mission state
        MissionTransitionCause cause = MISSION_TRANSITION_NONE;
        MissionState next_state = __mission_get_next_state(s_state, &cause);
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

/// @brief call after mission_task to puts the system to sleep until
///        the system receives another input signal
/// @param
void mission_sleep(void) {
    switch (s_state) {
        case MISSION_STATE_MISSION_START:
            break;

        case MISSION_STATE_RECORD_SURFACE:{
            __disable_irq();
            if (gps_bulk_queue_is_empty()
                && !log_battery_sample_buffer_is_half_full()
                && !log_pressure_sample_buffer_is_half_full()
                && !log_imu_any_buffer_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_RECORD_FLOATING: {
            __disable_irq();
            if (gps_bulk_queue_is_empty() 
                && gps_queue_is_empty() 
                && !argos_tx_mgr_ready_to_tx()
                && !log_battery_sample_buffer_is_half_full()
                && !log_pressure_sample_buffer_is_half_full()
                && !log_imu_any_buffer_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_RECORD_DIVE:
            __disable_irq();
            if ( !log_battery_sample_buffer_is_half_full()
                && !log_pressure_sample_buffer_is_half_full()
                && !log_imu_any_buffer_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                HAL_ResumeTick();
            }
            __enable_irq();
            break;

        case MISSION_STATE_BURN:
            __disable_irq();
            if ( gps_bulk_queue_is_empty() 
                 && gps_queue_is_empty() 
                 && !argos_tx_mgr_ready_to_tx()
                 && !log_battery_sample_buffer_is_half_full()
                 && !log_pressure_sample_buffer_is_half_full()
                 && !log_imu_any_buffer_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                HAL_ResumeTick();
            }
            __enable_irq();
            break;

        case MISSION_STATE_LOW_POWER_BURN:
            __disable_irq();
            if ( gps_bulk_queue_is_empty() 
                 && gps_queue_is_empty() 
                 && !argos_tx_mgr_ready_to_tx()
                 && !log_battery_sample_buffer_is_half_full()
                 && !log_pressure_sample_buffer_is_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                
                // ToDo: enter STOP1 mode instead
                HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);

                // HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                SystemClock_Config();
                HAL_ResumeTick();
            }
            __enable_irq();
            break;

        case MISSION_STATE_RETRIEVE: {
            __disable_irq();
            if ( gps_bulk_queue_is_empty() 
                 && gps_queue_is_empty() 
                 && !argos_tx_mgr_ready_to_tx()
                 && !log_battery_sample_buffer_is_half_full()
                 && !log_pressure_sample_buffer_is_half_full()
                 && !log_imu_any_buffer_half_full()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnableSleepOnExit();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_LOW_POWER_RETRIEVE: {
            __disable_irq();
            if (gps_bulk_queue_is_empty() 
                && gps_queue_is_empty() 
                && !argos_tx_mgr_ready_to_tx()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();

                // ToDo: enter STOP1 mode instead
                HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);

                // HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                SystemClock_Config();

                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_ERROR:
            break;
    }
}
