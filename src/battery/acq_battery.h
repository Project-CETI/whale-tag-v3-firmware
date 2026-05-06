/*****************************************************************************
 *   @file      battery/acq_battery.h
 *   @brief     Battery sample acquisition and buffering code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_ACQ_BATTERY_H
#define CETI_ACQ_BATTERY_H

#include <stdint.h>

typedef struct {
    uint64_t time_us;
    uint32_t error;
    double cell_voltage_v[2];
    double cell_temperature_c[2];
    double current_mA;
    double state_of_charge_percent;
    double average_power_mw;
    uint16_t status;
    uint16_t protection_alert;
} CetiBatterySample;

void acq_battery_get(CetiBatterySample *p_sample);
void acq_battery_init(void);
void acq_battery_register_callback(void (*callback)(const CetiBatterySample *));
void acq_battery_start(void);
void acq_battery_stop(void);

#endif // CETI_ACQ_BATTERY_H