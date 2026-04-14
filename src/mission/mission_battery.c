/*****************************************************************************
 *   @file      mission_battery.c
 *   @brief     Mission control battery logic
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "mission_battery.h"

#include "config.h" // { tag_config }
#include "battery/acq_battery.h" // {CetiBatterySample, acq_battery_get}

#define BATTERY_NOMINAL_CELL_VOLTAGE 3.8
#define LOW_VOLTAGE_WINDOW_SIZE 4
#define BATTERY_ERROR_COUNT_THRESHOLD 3

static double s_battery_v_samples[2][LOW_VOLTAGE_WINDOW_SIZE] = {0};
static size_t s_batter_v_position = 0;

static double s_battery_v_sum[2] = {0};
static uint16_t s_battery_consecutive_error_count = 0;

static inline int __mission_battery_is_tracking_enabled(void) {
    return ( tag_config.hw_config.bms.available 
        && tag_config.battery.enabled
    );
}

/// @brief update bms error count, cell averages, and 
/// @param  
void mission_battery_task(void) {
    // nothing to be done
    if ( !__mission_battery_is_tracking_enabled()) {
        return;
    }

    // get latest sample
    CetiBatterySample battery_sample = {};
    acq_battery_get(&battery_sample);

    // update error count
    if (battery_sample.error) {
        s_battery_consecutive_error_count += 1;
    } else {
        s_battery_consecutive_error_count = 0;
    }

    // check if tracking voltage
    if (!tag_config.mission.low_power_release.enabled) {
        return;
    }

    // update voltage averages
    for (int i = 0; i < 2; i++) {
        s_battery_v_sum[i] += battery_sample.cell_voltage_v[i] - s_battery_v_samples[i][s_batter_v_position];
        s_battery_v_samples[i][s_batter_v_position] = battery_sample.cell_voltage_v[i];
    }
    s_batter_v_position = (s_batter_v_position + 1) % LOW_VOLTAGE_WINDOW_SIZE;
    return;
}

/// @brief initialize battery tracking for mission statem achine
/// @param  
void mission_battery_init(void) {
    if (!__mission_battery_is_tracking_enabled()
        || !tag_config.mission.low_power_release.enabled
    ){
        return;
    }

    // set all cells to current instantanious value
    if(tag_config.mission.low_power_release.enabled)
    for (int cell_index = 0; cell_index < 2; cell_index++) {
        s_battery_v_sum[cell_index] = LOW_VOLTAGE_WINDOW_SIZE * BATTERY_NOMINAL_CELL_VOLTAGE;
        for (int sample_index = 0; sample_index < LOW_VOLTAGE_WINDOW_SIZE; sample_index++) {
            s_battery_v_samples[cell_index][sample_index] = BATTERY_NOMINAL_CELL_VOLTAGE;
        }
    }
}

/// @brief check if average battery voltage is below expected threshold
/// @param  
int  mission_battery_is_low_voltage(void) {
    if (!__mission_battery_is_tracking_enabled()
        || !tag_config.mission.low_power_release.enabled
    ){
        return 0;
    }

    for (int i = 0; i < 2; i++) {
        if (s_battery_v_sum[i] < (((float)tag_config.mission.low_power_release.threshold_mV/1000.0f) * LOW_VOLTAGE_WINDOW_SIZE)) {
            return 1;
        }
    }
    return 0;
}


/// @brief check if bms is continually returning error values
/// @param  
int mission_battery_is_in_error(void) {
    return ( __mission_battery_is_tracking_enabled() 
        && (s_battery_consecutive_error_count + 1 >= BATTERY_ERROR_COUNT_THRESHOLD)
    );
}