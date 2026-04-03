#ifdef asfdlkasdfljkasdjklfd
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>


static inline int __is_field(uint8_t c) {
    return (isprint((unsigned char)(c)) && (c) != ',' && (c) != '*');
}

static inline const uint8_t * __next_field(const uint8_t *field) {
    while (__is_field(*field)) {
        field++;
    }
    if (',' != *field) {
        return NULL;
    } else {
        field++;
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
        if (!__is_field(field[1 + i])) {
            return 0;
        }
    }
    if (0 != memcmp("RMC", &field[3], 3)) {
        return 0;
    }
    field += 5;

    // check validity
    const char *time_str = field = __next_field(field);
    if (NULL == field) {
        return 0;
    }
    // skip time field for now

    field = __next_field(field);
    if (NULL == field) {
        return 0;
    }
    {

    if ('A' != *field) {
        return 0;
    }
    }
    field = __next_field(field);
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

    field = __next_field(field);
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

    field = __next_field(field);
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

    field = __next_field(field);
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

    field = __next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // speed
    }

    field = __next_field(field);
    if (NULL == field) {
        return 0;
    }
    { // course
    }

    field = __next_field(field);
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
    latest_gps_coord.valid = 1;
    latest_gps_coord.year = year;
    latest_gps_coord.month = month;
    latest_gps_coord.day = day;         // (0..31]
    latest_gps_coord.hours = hours;     // (0..24]
    latest_gps_coord.minutes = minutes; // (0..60]
    latest_gps_coord.seconds = seconds; // (0..60] // used for RTC sync not logging
    latest_gps_coord.latitude_sign = latitude_sign;
    latest_gps_coord.longitude_sign = longitude_sign;
    latest_gps_coord.latitude = latitude;
    latest_gps_coord.longitude = longitude;
    return 1;
}

int __validate_rmc_sentence(const uint8_t *sentence) {

} 

/// @brief  Task that updates latest valid GPS coordinates, forwards all gps
/// to a file, and resyncronizes the RTC if it hasn't been yet;
/// @param
void __parse_gps_task(void) {
    const uint8_t *gps_sentence = gps_pop_sentence();
    while (NULL != gps_sentence) {
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

#endif