
//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef CETI_WHALE_TAG_MISSON_H
#define CETI_WHALE_TAG_MISSON_H

typedef enum {
    MISSION_STATE_MISSION_START,
    MISSION_STATE_RECORD_SURFACE,
    MISSION_STATE_RECORD_FLOATING,
    MISSION_STATE_RECORD_DIVE,
    MISSION_STATE_BURN,
    MISSION_STATE_LOW_POWER_BURN,
    MISSION_STATE_RETRIEVE,
    MISSION_STATE_LOW_POWER_RETRIEVE,
    MISSION_STATE_ERROR
} MissionState;

#define STARTING_STATE MISSION_STATE_RECORD_SURFACE

void mission_init(void);
void mission_task(void);
void mission_sleep(void);
void mission_set_state(MissionState next_state);

#endif // CETI_WHALE_TAG_MISSION_H
