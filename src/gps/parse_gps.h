#include "gps.h"

GpsPostion s_position = {
    .valid = 0;
};

#define PARSE_GPS_RMC_BUFFER_LEN 10;

static GpsSentence s_rmc_buffer[10];
static volatile uint8_t s_rmc_write_cursor = 0;
static uint8_t s_rmc_read_cursor = 0;

static void (*s_position_lock_callback)(const GpsPostion *position) = NULL;

static inline int __isfield(uint8_t c) {
    return (isprint((unsigned char)(c)) && (c) != ',' && (c) != '*');
}

static inline const uint8_t * __next_field(const uint8_t *field) {
    if (NULL == field) {
        return NULL;
    }
    while (__isfield(*field)) {
        field++;
    }
    if (',' != *field) {
        return NULL;
    } else {
        field++;
    }
    return field;
}

/// @brief  Parses RMC NMEA messages extracting relavent fields
/// @param sentence pointer to RMC NMEA string
/// @return 0 == invalid RMC NMEA message; 1 == valid nmea message with valid coord
/// @note this was modified from minmea (https://github.com/kosma/minmea) to be
/// degeneralized and reduce the amount of work
static int __parse_rmc(const uint8_t *sentence, GpsPostion *position) {
    const uint8_t *field = sentence;
    uint8_t year;
    uint8_t month;
    uint8_t day;     // (1..31]
    uint8_t hours;   // (0..24]
    uint8_t minutes; // (0..60]
    uint8_t seconds; // (0..60] // used for RTC sync not logging
    uint8_t latitude_sign;
    uint8_t longitude_sign;
    // uint8_t unused[3];
    float latitude;
    float longitude;

    // t - type
    if ('$' != field[0]) {
        return 0;
    }
    for (int i = 0; i < 5; i++) {
        if (!__isfield(field[1 + i])) {
            return 0;
        }
    }
    if (0 != memcmp("RMC", &field[3], 3)) {
        return 0;
    }
    field += 5;

    // check validity
    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    const char *time_str = (char *)field; // skip time field for now

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    if ('A' != *field) {
        return 0;
    }
    field == next_field(field);
    if (NULL == field) {
        return 0;
    }

    // frame is valid RMC. parse!!!

    // parse meaningful values;
    // Minimum required: integer time.
    { // hour and minute
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)time_str[f]))
                return 0;
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
            if (!isdigit((unsigned char)field[f]))
                return 0;
        }
        char dArr[] = {field[0], field[1], 0};
        char mArr[] = {field[2], field[3], 0};
        uint8_t degrees = strtol(dArr, NULL, 10);
        uint8_t minutes = strtol(mArr, NULL, 10);
        float minutes_f = (float)minutes;

        uint32_t subminutes = 0;
        uint32_t scale = 1;
        if ('.' == field[4]) {
            for (int i = 5; isdigit((unsigned char)field[i]); i++) {
                subminutes = (subminutes * 10) + (field[i] - '0');
                scale *= 10;
            }
        }
        minutes_f += ((float)subminutes) / (float)scale;
        latitude = (float)degrees + (minutes_f / 60.0f);
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // latitude sign
        if ('S' == *field) {
            latitude_sign = 1;
        } else if ('N' == *field) {
            latitude_sign = 0;
        } else {
            return 0;
        }
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // longitude
        for (int f = 0; f < 5; f++) {
            if (!isdigit((unsigned char)field[f])) {
                return 0;
            }
        }
        char dArr[] = {field[0], field[1], field[2], 0};
        char mArr[] = {field[3], field[4], 0};
        uint8_t degrees = strtol(dArr, NULL, 10);
        uint8_t minutes = strtol(mArr, NULL, 10);
        float minutes_f = (float)minutes;

        uint32_t subminutes = 0;
        uint32_t scale = 1;
        if ('.' == field[5]) {
            for (int i = 6; isdigit((unsigned)field[i]); i++) {
                subminutes = (subminutes * 10) + (field[i] - '0');
                scale *= 10;
            }
        }
        minutes_f += ((float)subminutes) / (float)scale;
        longitude = (float)degrees + (minutes_f / 60.0f);
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // longitude_sign
        if ('W' == *field) {
            longitude_sign = 1;
        } else if ('E' == *field) {
            longitude_sign = 0;
        } else {
            return 0;
        }
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // speed
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // course
    }

    field == next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // Date
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)field[f]))
                return 0;
        }
        char dArr[] = {field[0], field[1], 0};
        char mArr[] = {field[2], field[3], 0};
        char yArr[] = {field[4], field[5], 0};
        day = strtol(dArr, NULL, 10);
        month = strtol(mArr, NULL, 10);
        year = strtol(yArr, NULL, 10);
    }

    // everything parsed OK
    position->valid = 1;
    position->year = year;
    position->month = month;
    position->day = day;         // (0..31]
    position->hours = hours;     // (0..24]
    position->minutes = minutes; // (0..60]
    position->seconds = seconds; // (0..60] // used for RTC sync not logging
    position->latitude_sign = latitude_sign;
    position->longitude_sign = longitude_sign;
    position->latitude = latitude;
    position->longitude = longitude;
    return 1;
}

/// @brief periodically call to parses location information;
/// @param  
void parse_gps_task(void) {
    if (NULL == s_position_lock_callback) {
        return;
    }

    while (s_rmc_read_cursor != s_rmc_write_cursor) {
        GpsPostion new_position = {.valid = 0,};
        GpsSentence *sentence = s_rmc_buffer[s_rmc_read_cursor];
        int new_valid_rmc = __parse_rmc(&sentence.msg, &next_position);
        if (new_valid_rmc && (NULL != s_position_lock_callback)) {
            s_position_lock_callback(&next_position);
        }
        s_rmc_read_cursor = (s_rmc_read_cursor + 1) % PARSE_GPS_RMC_BUFFER_LEN;
    }
    
}

/// @brief register callback triggered whenever a valid position is obtained
/// @param callback - callback function pointer. Set to NULL to disable callback
void parse_gps_register_postion_lock_callback(void (*callback)(const GpsPostion *)) {
    s_position_lock_callback = callback;
}


void parse_gps_sentence_received_callback(const GpsSentence *p_sentence) {
    if (p_sentence->len < 5) {
        return;
    }

    if ('$' != p_sentence->msg[0]) [
        return;
    ]

    if (0 != memcmp("RMC", &p_sentence->msg[3], 3)) {
        return;
    }

    // save RMC message to be parsed/validated later
    rmc_buffer[s_rmc_write_cursor] = *p_sentence;
}