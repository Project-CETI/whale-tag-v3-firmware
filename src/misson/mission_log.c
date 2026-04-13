/*****************************************************************************
 *   @file      log_battery.c
 *   @brief     mission state logging
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "mission_log.h"

#include "error.h"
#include "metadata.h"
#include "timing.h"
 
#include <app_filex.h>
#include <stdio.h>


#define MISSION_LOG_CSV_FILENAME "tag_mission.csv"
#define MISSION_LOG_CSV_HEADER "Timestamp [us], From State, To State, Cause\n"

extern FX_MEDIA sdio_disk;
static FX_FILE s_mission_log_file = {};

static const char *const MissionStateNames[] = {
    [MISSION_STATE_MISSION_START] = "MISSION_START",
    [MISSION_STATE_RECORD_SURFACE] = "RECORD_SURFACE",
    [MISSION_STATE_RECORD_FLOATING] = "RECORD_FLOATING",
    [MISSION_STATE_RECORD_DIVE] = "RECORD_DIVE",
    [MISSION_STATE_BURN] = "BURN",
    [MISSION_STATE_LOW_POWER_BURN] = "LOW_POWER_BURN",
    [MISSION_STATE_RETRIEVE] = "RETRIEVE",
    [MISSION_STATE_LOW_POWER_RETRIEVE] = "LOW_POWER_RETRIEVE",
    [MISSION_STATE_ERROR] = "ERROR",
};

static const char *const MissionTransitionCauseNames[] = {
    [MISSION_TRANSITION_START] = "MISSION_START",
    [MISSION_TRANSITION_LOW_PRESSURE] = "LOW_PRESSURE",
    [MISSION_TRANSITION_HIGH_PRESSURE] = "HIGH_PRESSURE",
    [MISSION_TRANSITION_FLOAT_DETECTED] = "FLOAT_DETECTED",
    [MISSION_TRANSITION_FLOAT_ENDED] = "FLOAT_ENDED",
    [MISSION_TRANSITION_BATTERY_ERRORS] = "BATTERY_ERRORS",
    [MISSION_TRANSITION_LOW_VOLTAGE] = "LOW_VOLTAGE",
    [MISSION_TRANSITION_TIMER] = "TIMER",
    [MISSION_TRANSITION_TIME_OF_DAY] = "TIME_OF_DAY",
};

/// @brief ititialize mission logging
/// @param
int mission_log_init(void) {

    // try to create file
    UINT fx_create_result = fx_file_create(&sdio_disk, MISSION_LOG_CSV_FILENAME);
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_create_result), mission_log_init);
        return -1;
    }

    // open file
    UINT fx_open_result = fx_file_open(&sdio_disk, &s_mission_log_file, MISSION_LOG_CSV_FILENAME, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS != fx_open_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_open_result), mission_log_init);
        return -1;
    }

    //
    metadata_log_file_creation(MISSION_LOG_CSV_FILENAME, DATA_TYPE_MISSION, DATA_FORMAT_CSV, 0);


    // check that the file has no contents
    if ((0 == s_mission_log_file.fx_file_current_file_size)) {
        UINT fx_write_result = fx_file_write(&s_mission_log_file, MISSION_LOG_CSV_HEADER, strlen(MISSION_LOG_CSV_HEADER));
        if (FX_SUCCESS != fx_write_result) {
            error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_write_result), mission_log_init);
            return -1;
        }
    }

    return 0;
}

void mission_log_deinit(void) {
    fx_file_close(&s_mission_log_file);
}

/// @brief Saves state transition to mission log `data_log.csv`
/// @param current_state state at the start of the transistion
/// @param next_state state at the end of the transistion
void mission_log_state_transition(MissionState current_state, MissionState next_state, MissionTransitionCause cause) {
    uint64_t transistion_time_us = rtc_get_epoch_us();

    // generate log string
    char transition_string[256] = {};
    uint16_t transistion_string_length = snprintf(
        transition_string, sizeof(transition_string) - 1,
        "%lld, %s, %s, %s\n",
        transistion_time_us, MissionStateNames[current_state], MissionStateNames[next_state], MissionTransitionCauseNames[cause]
    );

    // write to file
    UINT fx_result = fx_file_write(&s_mission_log_file, transition_string, transistion_string_length);
    if (FX_SUCCESS != fx_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_result), mission_log_state_transition);
    }

    fx_file_close(&s_mission_log_file);
}
