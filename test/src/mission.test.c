#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CETI_LOG(T)

#include "mission.c"

CetiTagRuntimeConfiguration tag_config = {
    .hw_config.bms.available = 1,
    .battery.enabled = 1,
    .pressure.enabled = 1,
    .mission = {
        .starting_state = MISSION_STATE_RECORD_SURFACE,
        .low_power_release = {
            .enabled = 1,
            .threshold_mV = 3600,
        },
    },
};

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

/*** FAKES ***/
CetiPressureSample fake_pressure_sample = {};
void acq_pressure_get_latest(CetiPressureSample *p_sample) {
    *p_sample = fake_pressure_sample;
}
/***/

uint64_t fake_time_value = 0;
uint64_t rtc_get_epoch_us(void){
    return fake_time_value;
}

/***/
int fake_mission_battery_is_low_voltage_return_value;
int mission_battery_is_low_voltage(void) {
    return fake_mission_battery_is_low_voltage_return_value;
}

int fake_mission_battery_is_in_error_return_value;
int mission_battery_is_in_error(void) {
    return fake_mission_battery_is_in_error_return_value;
}

/***/
int fake_float_detection_is_floating_return_value;
int float_detection_is_floating(void) {
    return fake_float_detection_is_floating_return_value;
}

/*** TEST CODE ***/
#define FUZZ_ITERATIONS 1000
void test_low_pressure_fuzz(void) {
    uint16_t threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_SURFACE_THRESHOLD_BAR);

    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint16_t pressure_raw = (uint16_t)(rand() % UINT16_MAX);
        fake_pressure_sample.pressure = pressure_raw;

        int result = priv__is_low_pressure();

        if (pressure_raw < threshold_raw) {
            char msg[64];
            snprintf(msg, sizeof(msg), "raw=%u should be LOW (threshold=%u)", pressure_raw, threshold_raw);
            TEST_ASSERT_TRUE_MESSAGE(result, msg);
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "raw=%u should NOT be LOW (threshold=%u)", pressure_raw, threshold_raw);
            TEST_ASSERT_FALSE_MESSAGE(result, msg);
        }
    }
}

void test_low_pressure_boundary(void) {
    uint16_t threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_SURFACE_THRESHOLD_BAR);

    // exactly at threshold -> NOT low
    fake_pressure_sample.pressure = threshold_raw;
    TEST_ASSERT_FALSE(priv__is_low_pressure());

    // one below threshold -> low
    fake_pressure_sample.pressure = threshold_raw - 1;
    TEST_ASSERT_TRUE(priv__is_low_pressure());

    // one above threshold -> NOT low
    fake_pressure_sample.pressure = threshold_raw + 1;
    TEST_ASSERT_FALSE(priv__is_low_pressure());

    // zero -> low
    fake_pressure_sample.pressure = 0;
    TEST_ASSERT_TRUE(priv__is_low_pressure());
}

void test_low_pressure_disabled(void) {
    // when pressure sensor is disabled, priv__is_low_pressure always returns 1
    s_active_subsystems = (0xFFFFFFFF & ~(EN_PRESSURE));
    fake_pressure_sample.pressure = UINT16_MAX;
    TEST_ASSERT_TRUE(priv__is_low_pressure());
}

void test_high_pressure_fuzz(void) {
    uint16_t threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_DIVE_THRESHOLD_BAR);

    for (int i = 0; i < FUZZ_ITERATIONS; i++) {
        uint16_t pressure_raw = (uint16_t)(rand() % UINT16_MAX);
        fake_pressure_sample.pressure = pressure_raw;

        int result = priv__is_high_pressure();

        if (pressure_raw >= threshold_raw) {
            char msg[64];
            snprintf(msg, sizeof(msg), "raw=%u should be HIGH (threshold=%u)", pressure_raw, threshold_raw);
            TEST_ASSERT_TRUE_MESSAGE(result, msg);
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg), "raw=%u should NOT be HIGH (threshold=%u)", pressure_raw, threshold_raw);
            TEST_ASSERT_FALSE_MESSAGE(result, msg);
        }
    }
}

void test_high_pressure_boundary(void) {
    uint16_t threshold_raw = acq_pressure_bar_to_pressure_raw(PRESSURE_DIVE_THRESHOLD_BAR);

    // exactly at threshold -> high
    fake_pressure_sample.pressure = threshold_raw;
    TEST_ASSERT_TRUE(priv__is_high_pressure());

    // one below threshold -> NOT high
    fake_pressure_sample.pressure = threshold_raw - 1;
    TEST_ASSERT_FALSE(priv__is_high_pressure());

    // one above threshold -> high
    fake_pressure_sample.pressure = threshold_raw + 1;
    TEST_ASSERT_TRUE(priv__is_high_pressure());

    // max value -> high
    fake_pressure_sample.pressure = UINT16_MAX;
    TEST_ASSERT_TRUE(priv__is_high_pressure());
}

void test_high_pressure_disabled(void) {
    // when pressure sensor is disabled, priv__is_high_pressure always returns 0
    s_active_subsystems = (0xFFFFFFFF & ~(EN_PRESSURE));
    fake_pressure_sample.pressure = UINT16_MAX;
    TEST_ASSERT_FALSE(priv__is_high_pressure());
    tag_config.pressure.enabled = 1;
}


/** Helper: set all fakes to "quiet" (no transitions should fire) */
static void set_nominal(void) {
    /* pressure between surface (1 bar) and dive (5 bar) thresholds */
    fake_pressure_sample.pressure = acq_pressure_bar_to_pressure_raw(2.0);
    fake_mission_battery_is_low_voltage_return_value = 0;
    fake_mission_battery_is_in_error_return_value = 0;
    fake_float_detection_is_floating_return_value = 0;
    fake_time_value = 0;
    tag_config.mission.float_detection_enabled = 0;
    /* setUp already puts s_burn_start_timestamp_s = 0xFFFFFFFF */
    s_burn_started = 0;
}

/** Helper: assert a single state transition */
static void assert_transition(MissionState from, MissionState expected_to, MissionTransitionCause expected_cause) {
    MissionTransitionCause cause;
    MissionState next = priv__mission_get_next_state(from, &cause);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s -> got %s (cause %s), expected %s (cause %s)",
        MissionStateNames[from],
        MissionStateNames[next],
        (cause < sizeof(MissionTransitionCauseNames)/sizeof(*MissionTransitionCauseNames) && MissionTransitionCauseNames[cause])
            ? MissionTransitionCauseNames[cause] : "NONE",
        MissionStateNames[expected_to],
        (expected_cause < sizeof(MissionTransitionCauseNames)/sizeof(*MissionTransitionCauseNames) && MissionTransitionCauseNames[expected_cause])
            ? MissionTransitionCauseNames[expected_cause] : "NONE");
    TEST_ASSERT_EQUAL_MESSAGE(expected_to, next, msg);
    TEST_ASSERT_EQUAL_MESSAGE(expected_cause, cause, msg);
}

/* === MISSION_START === */
void test_START_always_transitions_to_surface(void) {
    assert_transition(MISSION_STATE_MISSION_START, STARTING_STATE, MISSION_TRANSITION_START);
}

/* === RECORD_SURFACE === */
void test_SURFACE_nominal(void) {
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_RECORD_SURFACE, MISSION_TRANSITION_NONE);
}

void test_SURFACE_low_voltage(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_LOW_VOLTAGE);
}

void test_SURFACE_battery_error(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_BATTERY_ERRORS);
}

void test_SURFACE_time_to_burn(void) {
    s_burn_start_timestamp_s = 100;
    fake_time_value = 100ULL * 1000000;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_BURN, MISSION_TRANSITION_TIMER);
}

void test_SURFACE_high_pressure(void) {
    fake_pressure_sample.pressure = acq_pressure_bar_to_pressure_raw(PRESSURE_DIVE_THRESHOLD_BAR);
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_RECORD_DIVE, MISSION_TRANSITION_HIGH_PRESSURE);
}

void test_SURFACE_float_detected(void) {
    tag_config.mission.float_detection_enabled = 1;
    fake_float_detection_is_floating_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_RECORD_FLOATING, MISSION_TRANSITION_FLOAT_DETECTED);
}

void test_SURFACE_float_disabled_ignores_floating(void) {
    s_active_subsystems = (0xFFFFFFFF & ~(EN_FLOAT));
    tag_config.mission.float_detection_enabled = 0;
    fake_float_detection_is_floating_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_RECORD_SURFACE, MISSION_TRANSITION_NONE);
}

/* === RECORD_FLOATING === */
void test_FLOATING_nominal(void) {
    tag_config.mission.float_detection_enabled = 1;
    fake_float_detection_is_floating_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_RECORD_FLOATING, MISSION_TRANSITION_NONE);
}

void test_FLOATING_low_voltage(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_LOW_VOLTAGE);
}

void test_FLOATING_battery_error(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_BATTERY_ERRORS);
}

void test_FLOATING_time_to_burn(void) {
    s_burn_start_timestamp_s = 100;
    fake_time_value = 100ULL * 1000000;
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_BURN, MISSION_TRANSITION_TIMER);
}

void test_FLOATING_high_pressure(void) {
    fake_pressure_sample.pressure = acq_pressure_bar_to_pressure_raw(PRESSURE_DIVE_THRESHOLD_BAR);
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_RECORD_DIVE, MISSION_TRANSITION_HIGH_PRESSURE);
}

void test_FLOATING_not_floating_returns_to_surface(void) {
    tag_config.mission.float_detection_enabled = 1;
    fake_float_detection_is_floating_return_value = 0;
    assert_transition(MISSION_STATE_RECORD_FLOATING, MISSION_STATE_RECORD_SURFACE, MISSION_TRANSITION_FLOAT_ENDED);
}

/* === RECORD_DIVE === */
void test_DIVE_nominal(void) {
    assert_transition(MISSION_STATE_RECORD_DIVE, MISSION_STATE_RECORD_DIVE, MISSION_TRANSITION_NONE);
}

void test_DIVE_low_voltage(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_DIVE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_LOW_VOLTAGE);
}

void test_DIVE_battery_error(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_DIVE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_BATTERY_ERRORS);
}

void test_DIVE_time_to_burn(void) {
    s_burn_start_timestamp_s = 100;
    fake_time_value = 100ULL * 1000000;
    assert_transition(MISSION_STATE_RECORD_DIVE, MISSION_STATE_BURN, MISSION_TRANSITION_TIMER);
}

void test_DIVE_low_pressure(void) {
    fake_pressure_sample.pressure = acq_pressure_bar_to_pressure_raw(PRESSURE_SURFACE_THRESHOLD_BAR) - 1;
    assert_transition(MISSION_STATE_RECORD_DIVE, MISSION_STATE_RECORD_SURFACE, MISSION_TRANSITION_LOW_PRESSURE);
}

/* === BURN === */
void test_BURN_nominal(void) {
    assert_transition(MISSION_STATE_BURN, MISSION_STATE_BURN, MISSION_TRANSITION_NONE);
}

void test_BURN_low_voltage(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    assert_transition(MISSION_STATE_BURN, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_LOW_VOLTAGE);
}

void test_BURN_battery_error(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_BURN, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_BATTERY_ERRORS);
}

void test_BURN_complete(void) {
    s_burn_started = 1;
    s_burn_start_timestamp_s = 0;
    tag_config.burnwire.duration_s = 1; /* burn duration = 1*60 = 60s */
    fake_time_value = 61ULL * 1000000;  /* 61s elapsed > 60s duration */
    assert_transition(MISSION_STATE_BURN, MISSION_STATE_RETRIEVE, MISSION_TRANSITION_TIMER);
}

void test_BURN_floating(void) {
    tag_config.mission.float_detection_enabled = 1;
    fake_float_detection_is_floating_return_value = 1;
    assert_transition(MISSION_STATE_BURN, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_FLOAT_DETECTED);
}

/* === LOW_POWER_BURN === */
void test_LP_BURN_nominal(void) {
    assert_transition(MISSION_STATE_LOW_POWER_BURN, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_NONE);
}

void test_LP_BURN_complete(void) {
    s_burn_started = 1;
    s_burn_start_timestamp_s = 0;
    tag_config.burnwire.duration_s = 1;
    fake_time_value = 61ULL * 1000000;
    assert_transition(MISSION_STATE_LOW_POWER_BURN, MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_TRANSITION_TIMER);
}

/* === RETRIEVE === */
void test_RETRIEVE_nominal(void) {
    assert_transition(MISSION_STATE_RETRIEVE, MISSION_STATE_RETRIEVE, MISSION_TRANSITION_NONE);
}

void test_RETRIEVE_low_voltage(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    assert_transition(MISSION_STATE_RETRIEVE, MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_TRANSITION_LOW_VOLTAGE);
}

void test_RETRIEVE_battery_error(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_RETRIEVE, MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_TRANSITION_BATTERY_ERRORS);
}

void test_RETRIEVE_floating(void) {
    tag_config.mission.float_detection_enabled = 1;
    fake_float_detection_is_floating_return_value = 1;
    assert_transition(MISSION_STATE_RETRIEVE, MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_TRANSITION_FLOAT_DETECTED);
}

/* === LOW_POWER_RETRIEVE (terminal) === */
void test_LP_RETRIEVE_stays(void) {
    assert_transition(MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_STATE_LOW_POWER_RETRIEVE, MISSION_TRANSITION_NONE);
}

/* === ERROR === */
void test_ERROR_stays(void) {
    assert_transition(MISSION_STATE_ERROR, MISSION_STATE_ERROR, MISSION_TRANSITION_NONE);
}

/* === Priority: low_voltage beats battery_error === */
void test_SURFACE_low_voltage_priority_over_battery_error(void) {
    fake_mission_battery_is_low_voltage_return_value = 1;
    fake_mission_battery_is_in_error_return_value = 1;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_LOW_VOLTAGE);
}

/* === Priority: battery errors beat timer === */
void test_SURFACE_battery_error_priority_over_timer(void) {
    fake_mission_battery_is_in_error_return_value = 1;
    s_burn_start_timestamp_s = 100;
    fake_time_value = 100ULL * 1000000;
    assert_transition(MISSION_STATE_RECORD_SURFACE, MISSION_STATE_LOW_POWER_BURN, MISSION_TRANSITION_BATTERY_ERRORS);
}

void setUp(void) {
    s_burn_start_timestamp_s = 0xFFFFFFFF;
    s_active_subsystems = 0xFFFFFFFF;
    set_nominal();
}

void tearDown(void) {
}

int main(void) {
    srand(time(NULL));
    UNITY_BEGIN();

    /* pressure detection fuzz/boundary */
    RUN_TEST(test_low_pressure_fuzz);
    RUN_TEST(test_low_pressure_boundary);
    RUN_TEST(test_low_pressure_disabled);
    RUN_TEST(test_high_pressure_fuzz);
    RUN_TEST(test_high_pressure_boundary);
    RUN_TEST(test_high_pressure_disabled);

    /* state transitions */
    RUN_TEST(test_START_always_transitions_to_surface);
    RUN_TEST(test_SURFACE_nominal);
    RUN_TEST(test_SURFACE_low_voltage);
    RUN_TEST(test_SURFACE_battery_error);
    RUN_TEST(test_SURFACE_time_to_burn);
    RUN_TEST(test_SURFACE_high_pressure);
    RUN_TEST(test_SURFACE_float_detected);
    RUN_TEST(test_SURFACE_float_disabled_ignores_floating);
    RUN_TEST(test_FLOATING_nominal);
    RUN_TEST(test_FLOATING_low_voltage);
    RUN_TEST(test_FLOATING_battery_error);
    RUN_TEST(test_FLOATING_time_to_burn);
    RUN_TEST(test_FLOATING_high_pressure);
    RUN_TEST(test_FLOATING_not_floating_returns_to_surface);
    RUN_TEST(test_DIVE_nominal);
    RUN_TEST(test_DIVE_low_voltage);
    RUN_TEST(test_DIVE_battery_error);
    RUN_TEST(test_DIVE_time_to_burn);
    RUN_TEST(test_DIVE_low_pressure);
    RUN_TEST(test_BURN_nominal);
    RUN_TEST(test_BURN_low_voltage);
    RUN_TEST(test_BURN_battery_error);
    RUN_TEST(test_BURN_complete);
    RUN_TEST(test_BURN_floating);
    RUN_TEST(test_LP_BURN_nominal);
    RUN_TEST(test_LP_BURN_complete);
    RUN_TEST(test_RETRIEVE_nominal);
    RUN_TEST(test_RETRIEVE_low_voltage);
    RUN_TEST(test_RETRIEVE_battery_error);
    RUN_TEST(test_RETRIEVE_floating);
    RUN_TEST(test_LP_RETRIEVE_stays);
    RUN_TEST(test_ERROR_stays);
    RUN_TEST(test_SURFACE_low_voltage_priority_over_battery_error);
    RUN_TEST(test_SURFACE_battery_error_priority_over_timer);

    return UNITY_END();
}
