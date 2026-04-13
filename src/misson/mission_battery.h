/*****************************************************************************
 *   @file      mission_battery.h
 *   @brief    Mission control battery logic
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
 #ifndef __CETI_MISSION_BATTERY_H__
 #define __CETI_MISSION_BATTERY_H__

void mission_battery_init(void);
void mission_battery_task(void);
int  mission_battery_is_low_voltage(void);
int mission_battery_is_in_error(void);

#endif //  __CETI_MISSION_BATTERY_H__