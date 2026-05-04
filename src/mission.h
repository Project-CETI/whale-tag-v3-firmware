
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
#ifndef CETI_WHALE_TAG_MISSION_H
#define CETI_WHALE_TAG_MISSION_H

#include "config.h"

typedef enum {
    MISSION_TRANSITION_NONE,
    MISSION_TRANSITION_START,
    MISSION_TRANSITION_LOW_PRESSURE,
    MISSION_TRANSITION_HIGH_PRESSURE,
    MISSION_TRANSITION_FLOAT_DETECTED,
    MISSION_TRANSITION_FLOAT_ENDED,
    MISSION_TRANSITION_BATTERY_ERRORS,
    MISSION_TRANSITION_LOW_VOLTAGE,
    MISSION_TRANSITION_TIMER,
    MISSION_TRANSITION_TIME_OF_DAY,
    MISSION_TRANSITION_UNKNOWN,
} MissionTransitionCause;

#define STARTING_STATE MISSION_STATE_RECORD_SURFACE

void mission_init(void);
void mission_task(void);
void mission_sleep(void);
void mission_set_state(MissionState next_state, MissionTransitionCause cause);

#endif // CETI_WHALE_TAG_MISSION_H
