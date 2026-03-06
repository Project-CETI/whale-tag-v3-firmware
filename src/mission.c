//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "mission.h"
#include "config.h"

#include "audio/acq_audio.h"
#include "burnwire.h"
#include "error.h"
#include "led/led.h"
#include "satellite/satellite.h"
#include "satellite/argos_tx_mgr.h"

#include "audio/log_audio.h"
#include "battery/log_battery.h"
#include "gps/gps.h"
#include "imu/acq_imu.h"
#include "syslog.h"

#include "main.h"
#include <usart.h>

#define STB_PRESSURE_IMPLEMENTATION // include the header implementations here
#include "pressure/pressure.h"

#define MISSION_DIVE_THRESHOLD_BAR (5.0)
#define MISSION_SURFACE_THRESHOLD_BAR (1.0)
#define MISSION_BURN_THRESHOLD_CELL_V (3.3)
#define MISSION_CRITICAL_THRESHOLD_CELL_V (3.2)
#define MISSION_BURNWIRE_BURN_PERIOD_MIN (20)

#define ARRAY_LEN(x) (sizeof((x))/sizeof(*(x)))

static MissionState s_state = MISSION_STATE_ERROR;

static struct {
	uint8_t valid;
	uint8_t year;
	uint8_t month;
	uint8_t day;  // (0..31]
	uint8_t hours; // (0..24]
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


typedef void (*MissionTask)(void); 

const char * const MissionStateNames[] = {
    [MISSION_STATE_MISSION_START] =     "MISSION_START",
    [MISSION_STATE_RECORD_SURFACE] =    "RECORD_SURFACE",
    [MISSION_STATE_RECORD_FLOATING] =   "RECORD_FLOATING",
    [MISSION_STATE_RECORD_DIVE] =       "RECORD_DIVE",
    [MISSION_STATE_BURN] =              "BURN",
    [MISSION_STATE_LOW_POWER_BURN] =    "LOW_POWER_BURN",
    [MISSION_STATE_RETRIEVE] =          "RETRIEVE",
    [MISSION_STATE_LOW_POWER_RETRIEVE] = "LOW_POWER_RETRIEVE",
    [MISSION_STATE_ERROR] =             "ERROR",
};


/* BATTERY CHECKS ************************************************************/
#define LOW_VOLTAGE_THRESHOLD 3.3
#define BATTERY_NOMINAL_CELL_VOLTAGE 3.8
#define LOW_VOLTAGE_WINDOW_SIZE 4
#define BATTERY_ERROR_COUNT_THRESHOLD 3

static double s_battery_v_samples[2][LOW_VOLTAGE_WINDOW_SIZE] = {0};
static size_t s_batter_v_position = 0; 

static double s_battery_v_sum[2] = {0};
static uint16_t s_battery_consecutive_error_count = 0;

static void __battery_task(void) {
#ifdef BMS_ENABLED
    CetiBatterySample battery_sample = {};

    // get latest sample
    acq_battery_peak_latest_sample(&battery_sample);

    // update error count
    if (battery_sample.error) {
        s_battery_consecutive_error_count += 1;
    } else {
        s_battery_consecutive_error_count = 0;
    }

    // update average voltage
    for (int i = 0; i < 2; i++) {
        s_battery_v_sum[i] += battery_sample.cell_voltage_v[i] - s_battery_v_samples[i][s_batter_v_position];
        s_battery_v_samples[i][s_batter_v_position] = battery_sample.cell_voltage_v[i];
    }
    s_batter_v_position = (s_batter_v_position + 1) % LOW_VOLTAGE_WINDOW_SIZE;
#endif // BMS_ENABLED
    return;
}

static void __initialize_average_voltage_tracker(void) {
    for(int cell_index = 0; cell_index < 2; cell_index++) {
        s_battery_v_sum[cell_index] = LOW_VOLTAGE_WINDOW_SIZE * BATTERY_NOMINAL_CELL_VOLTAGE;
        for (int sample_index = 0; sample_index < LOW_VOLTAGE_WINDOW_SIZE; sample_index++) {
            s_battery_v_samples[cell_index][sample_index] = BATTERY_NOMINAL_CELL_VOLTAGE;
        }
    }
}

static int __is_low_voltage(void) {
#ifdef BMS_ENABLED
    for (int i = 0; i < 2; i++) {
        if (s_battery_v_sum[i] < (LOW_VOLTAGE_THRESHOLD*LOW_VOLTAGE_WINDOW_SIZE)) {
            return 1;
        }
    }
#endif // BMS_ENABLED
    return 0;
}

static int __is_battery_in_error(void) {
#ifdef BMS_ENABLED
    return (s_battery_consecutive_error_count + 1 >= BATTERY_ERROR_COUNT_THRESHOLD);
#else
    return 0;
#endif
}

/* PRESSURE CONTROLS *********************************************************/

static int __is_low_pressure(void) {
#ifdef PRESSURE_ENABLED
    CetiPressureSample pressure_sample;
    pressure_get(&pressure_sample);
    return (pressure_sample.data.pressure < KELLER4LD_PRESSURE_BAR_TO_RAW(MISSION_SURFACE_THRESHOLD_BAR));
#else
    return 1;
#endif //PRESSURE_ENABLED
}

static int __is_high_pressure(void) {
#ifdef PRESSURE_ENABLED
    CetiPressureSample pressure_sample;
    pressure_get(&pressure_sample);
    return (pressure_sample.data.pressure >= KELLER4LD_PRESSURE_BAR_TO_RAW(MISSION_DIVE_THRESHOLD_BAR));
#else
    return 0;
#endif //PRESSURE_ENABLED
}

/* BURN CONTROLS *************************************************************/

static uint8_t s_burn_started = 0;
static time_t s_burn_start_timestamp_s = 0;

/// @brief check if burn should be activated based on timeout
/// @param  
/// @return bool
static int __is_time_to_burn(void) {
    time_t now_timestamp_s;
    now_timestamp_s = rtc_get_epoch_s(); 
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
    time_t elapsed_time = rtc_get_epoch_s() - s_burn_start_timestamp_s;
    return (elapsed_time > MISSION_BURNWIRE_BURN_PERIOD_MIN*60);
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
    char tx_message_ascii[(2*24) + 1];

    // add trailing zeros to message if message less than max message length
    uint8_t sized_message_buffer[24] = {0};
    memcpy(sized_message_buffer, message, message_len);

    // convert hex to ascii representation of hex
	for(int i = 0; i <max_len; i++){
        snprintf(&tx_message_ascii[2*i], 3,"%02X", sized_message_buffer[i]);
    }

    // perform transmission
    satellite_transmit(tx_message_ascii, 2 * max_len); // transmit via argos
    CETI_LOG("Tx Argos: %s", tx_message_ascii);
    #warning "ToDo: Log ARGOS transmission" // log transmission
}

/* MISSION LOG ****************************************************************/
#define MISSION_LOG_CSV_FILENAME "data_state_machine.csv"
#define MISSION_LOG_CSV_HEADER "Timestamp [us],RTC Count,Notes,State To Process,Next State\n"
#include <app_filex.h>

extern FX_MEDIA sdio_disk;
static FX_FILE s_mission_log_file = {};

/// @brief ititialize mission logging
/// @param  
/// @return 
static int __log_mission_init(void) {

    // try to create file
    UINT fx_create_result = fx_file_create(&sdio_disk, MISSION_LOG_CSV_FILENAME);
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_create_result));
        return -1;
    }

    UINT fx_open_result = fx_file_open(&sdio_disk, &s_mission_log_file, MISSION_LOG_CSV_FILENAME, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS != fx_open_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_open_result));
        return -1;
    }

    // check that the file has no contents
    if ((0 == s_mission_log_file.fx_file_current_file_size)) {
        UINT fx_write_result = fx_file_write(&s_mission_log_file, MISSION_LOG_CSV_HEADER, strlen(MISSION_LOG_CSV_HEADER));
        if ( FX_SUCCESS != fx_write_result) {
            error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_write_result));
            return -1;
        }
    }

    return 0;
}

/// @brief Saves state transition to mission log `data_log.csv`
/// @param current_state state at the start of the transistion
/// @param next_state state at the end of the transistion
static void __log_mission_state_transition(MissionState current_state, MissionState next_state) {
    time_t transistion_time_us = rtc_get_epoch_us();
    
    // generate log string
    char transition_string[256] = {};
    uint16_t transistion_string_length = snprintf(
        transition_string, sizeof(transition_string) - 1, 
        "%lld, %lld, , %s, %s\n", 
        transistion_time_us, (transistion_time_us/1000000), MissionStateNames[current_state], MissionStateNames[next_state]
    );

    // write to file
    UINT fx_result = fx_file_write(&s_mission_log_file, transition_string, transistion_string_length);
    if (FX_SUCCESS != fx_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_result));
    }

    fx_file_close(&s_mission_log_file);
}

/* GPS TASKS *****************************************************************/
static FX_FILE gps_log_file = {};

/// @brief initialize gps logging
/// @param  
static void __log_gps_init(void) {
    // try to create file
    CETI_LOG("Created new gps file \"gps.log\"");
    UINT fx_create_result = fx_file_create(&sdio_disk, "gps.log");
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_create_result));
    }
    UINT fx_result = fx_file_open(&sdio_disk, &gps_log_file, "gps.log", FX_OPEN_FOR_WRITE);
}

/// @brief initialize gps logging
/// @param  
static void __log_gps_deinit(void) {
    fx_file_close(&gps_log_file);

}

/// @brief performs the task of logging gps coordinates to a file 
/// @param  
static void __log_gps_task(void) {
#ifndef GPS_ENABLED
    return;
#endif

	const uint8_t *gps_bulk_ptr = gps_pop_bulk_transfer();
    if (NULL == gps_bulk_ptr) {
    	return;
    }

    UINT fx_result = fx_file_write(&gps_log_file, (char *)gps_bulk_ptr, GPS_BULK_TRANSFER_SIZE);
    if (FX_SUCCESS != fx_result) { // Catch error
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_result));
    }
}

/// @brief  Parses RMC NMEA messages extracting relavent fields 
/// @param sentence pointer to RMC NMEA string
/// @return 0 == invalid RMC NMEA message; 1 == valid nmea message with valid coord
/// @note this was modified from minmea (https://github.com/kosma/minmea) to be 
/// degeneralized and reduce the amount of work
static int __parse_rmc(const uint8_t *sentence) {
	const uint8_t *field = sentence;
	uint8_t year;
	uint8_t month;
	uint8_t day;  // (1..31]
	uint8_t hours; // (0..24]
	uint8_t minutes; // (0..60]
	uint8_t seconds; // (0..60] // used for RTC sync not logging
	uint8_t latitude_sign;
	uint8_t longitude_sign;
	// uint8_t unused[3];
	float latitude;
	float longitude;

#define isfield(c) (isprint((unsigned char)(c)) && (c) != ',' && (c) != '*')

#define next_field()								\
	do { 											\
		while(isfield(*field)) { field++; }	        \
		if (',' != *field) { 						\
			return 0; 								\
		} else { 									\
			field++; 								\
		} 											\
	}while(0)

	// t - type
	if ('$' != field[0] ) {
		return 0;
	}
	for (int i = 0; i < 5; i++) {
		if(!isfield(field[1+i])) {
			return 0;
		}
	}
	if( 0 != memcmp("RMC", &field[3], 3)) {
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


	//frame is valid RMC. parse!!!

	// parse meaningful values;
	 // Minimum required: integer time.
	{ // hour and minute
		for (int f = 0; f < 6; f++) {
			if (!isdigit((unsigned char)time_str[f])) return 0;
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
			if (!isdigit((unsigned char)field[f])) return 0;
		}
		char dArr[]={ field[0], field[1], 0};
		char mArr[]={ field[2], field[3], 0};
		uint8_t degrees = strtol(dArr, NULL, 10);
		uint8_t minutes = strtol(mArr, NULL, 10);
		float minutes_f = (float)minutes;

		uint32_t subminutes = 0;
		uint32_t scale = 1;
		if ('.' == field[4]) {
			for(int i = 5; isdigit((unsigned char)field[i]); i++) {
				subminutes = (subminutes * 10) + (field[i] - '0');
				scale *= 10;
			}
		}
		minutes_f += ((float)subminutes)/(float)scale;
		latitude = (float)degrees + (minutes_f / 60.0f);
	}

	next_field();
	{ // latitude sign
		if('S' == *field) {
			latitude_sign = 1;
		} else if('N' == *field) {
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
		char dArr[]={ field[0], field[1], field[2], 0};
		char mArr[]={ field[3], field[4], 0};
		uint8_t degrees = strtol(dArr, NULL, 10);
		uint8_t minutes = strtol(mArr, NULL, 10);
		float minutes_f = (float)minutes;

		uint32_t subminutes = 0;
		uint32_t scale = 1;
		if ('.' == field[5]) {
			for(int i = 6; isdigit((unsigned)field[i]); i++) {
				subminutes = (subminutes * 10) + (field[i] - '0');
				scale *= 10;
			}
		}
		minutes_f += ((float)subminutes)/(float)scale;
		longitude = (float)degrees + (minutes_f / 60.0f);
	}

	next_field();
	{ // longitude_sign
		if('W' == *field) {
			longitude_sign = 1;
		} else if('E' == *field) {
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
			if (!isdigit((unsigned char)field[f])) return 0;
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
	latest_gps_coord.day = day;  // (0..31]
	latest_gps_coord.hours = hours; // (0..24]
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
#ifndef GPS_ENABLED
    return;
#endif // GPS_ENABLED
    const uint8_t *gps_sentence = gps_pop_sentence();
	while(NULL != gps_sentence) {
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
#ifndef SATELLITE_ENABLED
    return
#endif // SATELLITE_ENABLED

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
    gps_packet = (((uint64_t)(latest_gps_coord.day & ((1 << 5) - 1))) << (64 - 5))
        | (((uint64_t)(latest_gps_coord.hours & ((1 << 5) - 1))) << (64 - 10))
        | (((uint64_t)( latest_gps_coord.minutes & ((1 << 6) - 1))) << (64 - 16))
        | (((uint64_t)(latest_gps_coord.latitude_sign & ((1 << 1) - 1))) << (64 - 17))
        | (((uint64_t)(lat_abs & ((1 << 20) - 1))) << (64 - 37))
        | (((uint64_t)(latest_gps_coord.longitude_sign & ((1 << 1) - 1))) << (64 - 38))
        | (((uint64_t)(lon_abs & ((1 << 21) - 1))) << (64 - 59));

    // move gps packet into tx_message_buffer
    uint8_t tx_message[24] = {};
    for (int i = 0; i < 8; i++){
        tx_message[i] = (uint8_t)((gps_packet >> 8*(8-(i + 1))) & 0xff);
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
    if ( rconf != ARGOS_MOD_VLDA4) {
        tx_message[max_len - 4] = (uint8_t)((tx_count >> 24) & ((1 << 8) - 1));
        tx_message[max_len - 3] = (uint8_t)((tx_count >> 16) & ((1 << 8) - 1));
        tx_message[max_len - 2] = (uint8_t)((tx_count >> 8) & ((1 << 8) - 1));
        tx_message[max_len - 1] = (uint8_t)((tx_count >> 0) & ((1 << 8) - 1));
        
    	__satellite_transmit_from_raw_with_log(rconf, tx_message,  max_len);
    } else {
        // VLDA4 only consists of 3 bytes, so skip the data info to only transmit GPS coord
    	__satellite_transmit_from_raw_with_log(rconf, &tx_message[1],  3);
    }
    argos_tx_mgr_inval_ready_to_tx(); // reset transmission timer
    tx_count++; // update transmission count  
}

/// @brief this task reverts the recovery board to its "active state" if it is
/// likely the state_machine is stuck in an "unsafe" state with no host
/// communication
static void __wdt_task(void) {
    #warning "ToDo: implement __wdt_task"
}

static void audio_task(void) {
    acq_audio_task();
}

static void audio_disable(void) {
    acq_audio_disable();
    log_audio_disable();
}

/* MISSION STATE MACHINE *****************************************************/
/// @brief determines the next state the tag should transition into
/// @param current_state 
/// @return 
static MissionState __mission_get_next_state(MissionState current_state) {
    switch(current_state) {
        case MISSION_STATE_MISSION_START: {
            return MISSION_STATE_RECORD_SURFACE;
        }

        case MISSION_STATE_RECORD_SURFACE: {
            if (__is_low_voltage()) {
                CETI_LOG("Entering burn due to low voltage!!!");
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_battery_in_error()){
                CETI_LOG("Entering burn due to BMS errors!!!");
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_time_to_burn()) {
                CETI_LOG("Entering burn due to timeout!!!");
                return MISSION_STATE_BURN;
            }

            if (__is_high_pressure()) {
                return MISSION_STATE_RECORD_DIVE;
            }

            if (__is_floating()) {
                return MISSION_STATE_RECORD_FLOATING;
            }

            return MISSION_STATE_RECORD_SURFACE; // remain in current state
        } // case MISSION_STATE_RECORD_SURFACE

        case MISSION_STATE_RECORD_FLOATING: {
            if (__is_low_voltage() || __is_battery_in_error() || __is_time_to_burn()) {
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_high_pressure()) {
                return MISSION_STATE_RECORD_DIVE;
            }

            if (!__is_floating()) {
                return MISSION_STATE_RECORD_SURFACE;
            }

            return MISSION_STATE_RECORD_FLOATING; // remain in current state
        } // case MISSION_STATE_RECORD_SURFACE

        case MISSION_STATE_RECORD_DIVE: {
            if (__is_low_voltage() || __is_battery_in_error()) {
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_time_to_burn()) {
                return MISSION_STATE_BURN;
            }

            if (__is_low_pressure()) {
                return MISSION_STATE_RECORD_SURFACE;
            }

            return MISSION_STATE_RECORD_DIVE; // remain in current state
        } // case MISSION_STATE_RECORD_DIVE

        case MISSION_STATE_BURN: {
            if (__is_low_voltage() || __is_battery_in_error()) {
                return MISSION_STATE_LOW_POWER_BURN;
            }

            if (__is_burn_complete()) {
                return MISSION_STATE_RETRIEVE;
            }
            return MISSION_STATE_BURN; // remain in current state
        } // case MISSION_STATE_BURN

        case MISSION_STATE_LOW_POWER_BURN: {
            if (__is_burn_complete()) {
                return MISSION_STATE_LOW_POWER_RETRIEVE;
            }

            return MISSION_STATE_LOW_POWER_BURN;
        } // case MISSION_STATE_LOW_POWER_BURN
  
        case MISSION_STATE_RETRIEVE: {
            if (__is_low_voltage() || __is_battery_in_error()) {
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

/// @brief Reconfigures tag for next mission state and updates the current state
/// @param next_state 
void mission_set_state(MissionState next_state) {
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

    // Sensing
    if ((MISSION_STATE_LOW_POWER_BURN == next_state) 
        || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)
    ) {
#ifdef AUDIO_ENABLED
        audio_disable();
#endif // AUDIO_ENABLED
#ifdef IMU_ENABLED
#warning ToDo: Disable imu acquistion
#warning ToDo: Disable imu logging
#endif // IMU_ENABLED
#ifdef ECG_ENABLED
#warning ToDo: Disable ecg acquistion
#warning ToDo: Disable ecg logging
#endif // ECG_ENABLED
    } else {
#ifdef AUDIO_ENABLED
        acq_audio_start();

        // log_audio_enable();
        // acq_audio_enable(); 
#endif // AUDIO_ENABLED
#ifdef IMU_ENABLED
#warning ToDo: Enable imu acquistion
#warning ToDo: Enable imu logging
#endif // IMU_ENABLED
#ifdef ECG_ENABLED
#warning ToDo: Enable ecg acquistion
#warning ToDo: Enable ecg logging
#endif // ECG_ENABLED
    }

    // Pressure
#ifdef PRESSURE_ENABLED
    if (MISSION_STATE_LOW_POWER_RETRIEVE == next_state) {
        pressure_deinit();
    } else {
        pressure_start();
    }
#endif

    // Burnwire
#ifdef BURNWIRE_ENABLED
    if ((MISSION_STATE_BURN == next_state) 
        || (MISSION_STATE_LOW_POWER_BURN == next_state)
    ){
        burnwire_on();
    } else {
        burnwire_off();
    }
#endif

    // LED
    if ((MISSION_STATE_RECORD_DIVE == next_state) 
        || (MISSION_STATE_RETRIEVE == next_state)
        || (MISSION_STATE_LOW_POWER_RETRIEVE == next_state)
    ) {
        led_heartbeat_dim();
    } else if ((MISSION_STATE_BURN == next_state) 
        || (MISSION_STATE_LOW_POWER_BURN == next_state)
    ){
        led_burn();
    } else {
        led_heartbeat();
    }

    // GPS
#ifdef GPS_ENABLED
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
#endif

    // Satellite
#ifdef SATELLITE_ENABLED
    if ((MISSION_STATE_RECORD_SURFACE == next_state) 
        || (MISSION_STATE_RECORD_DIVE == next_state)
    ) {
        argos_tx_mgr_disable();
    } else {
        argos_tx_mgr_enable();
    }
#endif

    // Log state transition
    __log_mission_state_transition(s_state, next_state);

    //update state
    s_state = next_state;
}

/* MISSION STATE MACHINE PUBLIC **********************************************/
/// @brief Initialize mission state machine
/// @param  
void mission_init(void) {
#ifdef AUDIO_ENABLED
    /* enable Audio -5V */
    CETI_LOG("Initializing Audio");
    log_audio_init();
    acq_audio_init();
#endif // AUDIO_ENABLED

    /* perform runtime system hardware test to detect available systems */
#ifdef BMS_ENABLED
    CETI_LOG("Initializing BMS");
    acq_battery_init();
    log_battery_enable();
#endif // BMS_ENABLED


#ifdef PRESSURE_ENABLED
    CETI_LOG("Initializing Pressure");
#if HW_VERSION == HW_VERSION_3_1_0 || HW_VERSION == HW_VERSION_3_1_0_MSH
    // Note: on this version of the tag, GPS must be powered to prevent the
    // shared i2c bus from being pulled low.
    HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
#endif
    pressure_init(1);
#endif // PRESSURE_ENABLED

#ifdef IMU_ENABLED
    CETI_LOG("Initializing IMU");
    // acq_imu_init();
#endif // IMU_ENABLED

#ifdef ECG_ENABLED
    CETI_LOG("Initializing ECG");
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPINIT_CB_ID, HAL_I2C_MspInit);
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPDEINIT_CB_ID, HAL_I2C_MspDeInit);
    MX_I2C2_Init(); // ECG ADC
    // acq_ecg_enable();
#endif // ECG_ENABLED

#ifdef GPS_ENABLED
    CETI_LOG("Initializing GPS");
    gps_init();
#endif // GPS_ENABLED

#ifdef SATELLITE_ENABLED
    CETI_LOG("Initializing ARGOS");
    MX_USART2_UART_Init();
    satellite_init();
#endif // SATELLITE_ENABLED

#ifdef FLASHER_ENABLED
    CETI_LOG("Enabling Antenna Flasher");
#endif

    error_queue_init(); // initialize error queue so we can log when issues occur
    __initialize_average_voltage_tracker();
    #warning ToDo: initialize mission hardware
    time_t now_timestamp_s = rtc_get_epoch_s();
    // start burn timer
    s_burn_start_timestamp_s = now_timestamp_s + 4*60*60;
    
    // initialize gps log
    __log_gps_init();
    
    // force state transition
    __log_mission_init();    // initialize mission log prior to performing initial state transistion.
    s_state = MISSION_STATE_ERROR; 
    mission_set_state(STARTING_STATE);
}

/// @brief updates the current system state based on latest system interaction 
/// @param  
void mission_task(void) {
    __battery_task();

    switch (s_state) {
        case MISSION_STATE_MISSION_START: {

        }
        break;

        case MISSION_STATE_RECORD_SURFACE: {
            const MissionTask mission_surface_tasks[] = {
            #ifdef BMS_ENABLED
                log_battery_task,
            #endif
            #ifdef PRESSURE_ENABLED
                pressure_task,
            #endif
            #ifdef IMU_ENABLED
                // log_imu_task,
                acq_imu_task,
            #endif
            #ifdef ECG_ENABLED
                // log_ecg_task,
            #endif
                // log_syslog_task,
            };

            /* tasks to execute on wake */
            for (int i = 0; i < ARRAY_LEN(mission_surface_tasks); i++){
#ifdef AUDIO_ENABLED
                audio_task(); // always check if audio needs to be logged
#endif
                mission_surface_tasks[i](); // perform an additional task
            }
#ifdef AUDIO_ENABLED
            audio_task(); // always check if audio needs to be logged
#endif
        }
        break;

        case MISSION_STATE_RECORD_DIVE: {
            const MissionTask record_floating_list[] = {
                log_battery_task,
                pressure_task,
                // log_imu_task,
                // log_ecg_task,
                // log_syslog_task,
            };
            

            /* tasks to execute on wake */
            for (int i = 0; i < ARRAY_LEN(record_floating_list); i++){
                audio_task(); // always check if audio needs to be logged
                record_floating_list[i](); // perform an additional task
            }
        }
        break;

        case MISSION_STATE_RECORD_FLOATING: {
            const MissionTask record_floating_task_list[] = {
                log_battery_task,
                pressure_task,
                __log_gps_task,
                __parse_gps_task,
                __transmit_gps_task,
            };
            for (int i = 0; i < ARRAY_LEN(record_floating_task_list); i++) {
                audio_task(); // priority task
                record_floating_task_list[i]();
            }
        }
        break;

        case MISSION_STATE_BURN: {
            const MissionTask burn_task_list[] = {
                log_battery_task,
                pressure_task,
                __log_gps_task,
                __parse_gps_task,
                __transmit_gps_task,
            };
            for (int i = 0; i < ARRAY_LEN(burn_task_list); i++) {
                audio_task(); // priority task
                burn_task_list[i]();
            }
        }
        break;

        case MISSION_STATE_LOW_POWER_BURN: {

        }
        break;

        case MISSION_STATE_RETRIEVE: {
            const MissionTask retrieve_task_list[] = {
                log_battery_task,
                __log_gps_task,
                __parse_gps_task,
                __transmit_gps_task,
            };
            for (int i = 0; i < ARRAY_LEN(retrieve_task_list); i++) {
                audio_task(); // priority task
                retrieve_task_list[i]();
            }
        }
        break;

        case MISSION_STATE_LOW_POWER_RETRIEVE: {
            log_battery_task();
        	__log_gps_task();
            __parse_gps_task();
            __transmit_gps_task();
        }
        break;

        case MISSION_STATE_ERROR:
            break;
    }

    // Check if tag is stuck not transmitting GPS via ARGOS
    __wdt_task();
    // __syslog_task();
    
    // Update the mission state
    MissionState next_state = __mission_get_next_state(s_state);
    mission_set_state(next_state);

    // Store any errors to file
    error_queue_flush();
}

/// @brief call after mission_task to puts the system to sleep until
///        the system receives another input signal
/// @param 
void mission_sleep(void) {
    switch (s_state) {
        case MISSION_STATE_MISSION_START:
            break;

        case MISSION_STATE_RECORD_SURFACE:
        	if (gps_bulk_queue_is_empty()) {

        	}
            break;

        case MISSION_STATE_RECORD_FLOATING: {
        	__disable_irq();
            if (gps_bulk_queue_is_empty()
                && gps_queue_is_empty()
                && !argos_tx_mgr_ready_to_tx()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                // ToDo: reenable clocks

                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_RECORD_DIVE:
            break;

        case MISSION_STATE_BURN:
            break;

        case MISSION_STATE_LOW_POWER_BURN:
            break;

        case MISSION_STATE_RETRIEVE: {
        	__disable_irq();
            if (gps_bulk_queue_is_empty()
                && gps_queue_is_empty()
                && !argos_tx_mgr_ready_to_tx()
            ) {
                // nothing to currently do!
                // go to sleep until next interrupt
                HAL_SuspendTick();
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                // ToDo: reenable clocks

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
                HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                // ToDo: reenable clocks

                HAL_ResumeTick();
            }
            __enable_irq();
            break;
        }

        case MISSION_STATE_ERROR:
            break;
    }
}
