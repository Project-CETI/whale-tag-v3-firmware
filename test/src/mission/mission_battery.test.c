#include <unity.h>
#include <stdio.h>

#include "mission/mission_battery.c"

CetiTagRuntimeConfiguration tag_config = {
    .hw_config.bms.available = 1,
    .battery.enabled = 1,
    .mission.low_power_release = {
        .enabled = 1,
        .threshold_mV = 3600,
    },
};

// ToDo: move to mock
static CetiBatterySample s_sample = {};
void acq_battery_get(CetiBatterySample *p_sample) {
    *p_sample = s_sample;
}

void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    return UNITY_END();
}
