#include "parse_gps.h"

#include "acq_gps.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int s_parsed_rmc_outdated = 1;
static GpsCoord s_latest_coord;
static GpsSentence s_latest_valid_rmc_sentence = {};

[[gnu::pure]]
static inline int priv__is_field(uint8_t c) {
    return (isprint((unsigned char)(c)) && (c) != ',' && (c) != '*');
}

[[gnu::pure]]
static inline const uint8_t * priv__next_field(const uint8_t *field) {
    if (NULL == field) { 
        return NULL;
    } 
    while (priv__is_field(*field)) {
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
[[gnu::pure]]
static GpsCoord priv__parse_rmc(const uint8_t *sentence) {
    GpsCoord coordinates = {.valid = 0};
    const uint8_t *field = sentence;

    if (NULL == sentence) {
        return coordinates;
    }
    
    // t - type
    if ('$' != sentence[0]) {
        return coordinates;
    }
    for (int i = 0; i < 5; i++) {
        if (!priv__is_field(sentence[1 + i])) {
            return coordinates;
        }
    }
    if (0 != memcmp("RMC", &sentence[3], 3)) {
        return coordinates;
    }
    field = sentence + 5;

    // check validity
    const uint8_t *time_field = priv__next_field(field);
    // skip time field for now
    
    const uint8_t *valid_field = priv__next_field(time_field);
    
    if (NULL == valid_field) { return coordinates; }
    { 
        if ('A' != *valid_field) { return coordinates; }
    }
    // frame is valid RMC. parse!!!
        
    // parse meaningful values;
    // Minimum required: integer time.
    if (NULL == time_field) { return coordinates; }
    { // hour and minute
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)time_field[f]))
                return coordinates;
        }

        char hArr[] = {time_field[0], time_field[1], '\0'};
        char mArr[] = {time_field[2], time_field[3], '\0'};
        char sArr[] = {time_field[4], time_field[5], '\0'};

        coordinates.hours = strtol(hArr, NULL, 10);
        coordinates.minutes = strtol(mArr, NULL, 10);
        coordinates.seconds = strtol(sArr, NULL, 10);
    }

    const uint8_t *latitude_field = priv__next_field(valid_field);
    if (NULL == latitude_field) { return coordinates; }
    { // latitude
        for (int f = 0; f < 4; f++) {
            if (!isdigit((unsigned char)latitude_field[f]))
                return coordinates;
        }
        char dArr[] = {latitude_field[0], latitude_field[1], 0};
        char mArr[] = {latitude_field[2], latitude_field[3], 0};
        uint8_t degrees = strtol(dArr, NULL, 10);
        uint8_t minutes = strtol(mArr, NULL, 10);
        float minutes_f = (float)minutes;

        uint32_t subminutes = 0;
        uint32_t scale = 1;
        if ('.' == latitude_field[4]) {
            for (int i = 5; isdigit((unsigned char)latitude_field[i]); i++) {
                subminutes = (subminutes * 10) + (latitude_field[i] - '0');
                scale *= 10;
            }
        }
        minutes_f += ((float)subminutes) / (float)scale;
        coordinates.latitude = (float)degrees + (minutes_f / 60.0f);
    }

    const uint8_t *latitude_sign_field = priv__next_field(latitude_field);
    if (NULL == latitude_sign_field) { return coordinates; }
    { // latitude sign
        if ('S' == *latitude_sign_field) {
            coordinates.latitude_sign = 1;
        } else if ('N' == *latitude_sign_field) {
            coordinates.latitude_sign = 0;
        } else {
            return coordinates;
        }
    }

    field = priv__next_field(latitude_sign_field);
    if (NULL == field) {
        return coordinates;
    }
    { // longitude
        for (int f = 0; f < 5; f++) {
            if (!isdigit((unsigned char)field[f])) {
                return coordinates;
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
        coordinates.longitude = (float)degrees + (minutes_f / 60.0f);
    }

    field = priv__next_field(field);
    if (NULL == field) {
        return coordinates;
    }
    { // longitude_sign
        if ('W' == *field) {
            coordinates.longitude_sign = 1;
        } else if ('E' == *field) {
            coordinates.longitude_sign = 0;
        } else {
            return coordinates;
        }
    }

    field = priv__next_field(field);
    if (NULL == field) {
        return coordinates;
    }
    { // speed
    }

    field = priv__next_field(field);
    if (NULL == field) {
        return coordinates;
    }
    { // course
    }

    field = priv__next_field(field);
    if (NULL == field) {
        return coordinates;
    }
    { // Date
        for (int f = 0; f < 6; f++) {
            if (!isdigit((unsigned char)field[f]))
                return coordinates;
        }
        char dArr[] = {field[0], field[1], 0};
        char mArr[] = {field[2], field[3], 0};
        char yArr[] = {field[4], field[5], 0};
        coordinates.day = strtol(dArr, NULL, 10);
        coordinates.month = strtol(mArr, NULL, 10);
        coordinates.year = strtol(yArr, NULL, 10);
    }

    // everything parsed OK
    coordinates.valid = 1;
    return coordinates;
}

[[gnu::pure]]
static int priv__validate_rmc_sentence(const uint8_t *sentence) {
    const uint8_t *field = sentence;

    // t - type
    if ('$' != field[0]) {
        return 0;
    }
    for (int i = 0; i < 5; i++) {
        if (!priv__is_field(field[1 + i])) {
            return 0;
        }
    }
    if (0 != memcmp("RMC", &field[3], 3)) {
        return 0;
    }
    field += 5;

    // check validity
    field = priv__next_field(field);
    if (NULL == field) {
        return 0;
    }
    // skip time field for now

    field = priv__next_field(field);
    if (NULL == field) {
        return 0;
    }
    {

    if ('A' != *field) {
        return 0;
    }
    }
    field = priv__next_field(field);
    if (NULL == field) {
        return 0;
    }
    return 1;
} 

void parse_gps_push_sentence(const GpsSentence *p_sentence) {
    if (priv__validate_rmc_sentence(p_sentence->msg)){
        s_latest_valid_rmc_sentence = *p_sentence;
        s_parsed_rmc_outdated = 1;
    }
}

GpsCoord parse_gps_get_latest_coordinates(void) {  
    // update coord if newer coord available
    if (s_parsed_rmc_outdated) {
        GpsCoord new_coord = priv__parse_rmc(s_latest_valid_rmc_sentence.msg);
        if (new_coord.valid) {
            s_latest_coord = new_coord;
            s_parsed_rmc_outdated = 0;
        }
    }

    return s_latest_coord;    
}
