/*****************************************************************************
 *   @file      log_battery.h
 *   @brief     Mission state logging
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
 #ifndef __CETI_MISSION_LOG_H__
 #define __CETI_MISSION_LOG_H__
 
#include "mission.h" // { MissionState, MissionTransitionCause }

int mission_log_init(void);
void mission_log_deinit(void);
void mission_log_state_transition(MissionState current_state, MissionState next_state, MissionTransitionCause cause);

#endif //  __CETI_MISSION_LOG_H__