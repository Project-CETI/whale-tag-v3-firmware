/*****************************************************************************
 *   @file      battery/log_battery.c
 *   @brief     code for saving acquired battery data to disk
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "main.h"

#include "acq_battery.h"
#include "syslog.h"

#include <app_filex.h>
#include <stdio.h>

typedef enum {
    LOG_BATTERY_FORMAT_BIN,
    LOG_BATTERY_FORMAT_CSV,
} LogBatteryFormat;

#define LOG_BATTERY_FORMAT LOG_BATTERY_FORMAT_CSV
#define LOG_BATTERY_FILENAME "data_battery.csv"

#define LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD (3*1024)
#define LOG_BATTERY_ENCODE_BUFFER_SIZE (LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)

#define _RSHIFT(x, s, w) (((x) >> s) & ((1 << w) - 1))
#define _LSHIFT(x, s, w) (((x) & ((1 << w) - 1)) << s)

 uint8_t log_battery_encode_buffer[LOG_BATTERY_ENCODE_BUFFER_SIZE];

static char *log_battery_csv_header =
    "Timestamp [us]"
    ", Notes"
    ", Battery V1 [V]"
    ", Battery V2 [V]"
    ", Battery I [mA]"
    ", Battery T1 [C]"
    ", Battery T2 [C]"
    ", State of Charge [\%]"
    ", Status"
    ", Protection Alerts"
    "\n"
;
extern FX_MEDIA sdio_disk;
FX_FILE log_battery_file = {};

/**
 * @brief converts the raw value of the BMS status register to a human
 * readable string.
 *
 * @param raw raw status register value.
 * @return const char*
 */
static const char *__status_to_str(uint16_t raw) {
    static char status_string[72] = ""; // max string length is 65
    static uint16_t previous_status = 0;
    char *flags[11] = {};
    int flag_count = 0;

    // mask do not care bits
    raw &= 0xF7C6;

    // don't do work we don't have to do
    if (raw == previous_status) {
        return status_string;
    }

    // clear old string
    status_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
        previous_status = 0;
        return status_string;
    }

    // detect flags
    if (_RSHIFT(raw, 15, 1))
        flags[flag_count++] = "PA";
    if (_RSHIFT(raw, 1, 1))
        flags[flag_count++] = "POR";
    // if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "dSOCi"; //ignored indicates interger change in SoC
    if (_RSHIFT(raw, 2, 1))
        flags[flag_count++] = "Imn";
    if (_RSHIFT(raw, 6, 1))
        flags[flag_count++] = "Imx";
    if (_RSHIFT(raw, 8, 1))
        flags[flag_count++] = "Vmn";
    if (_RSHIFT(raw, 12, 1))
        flags[flag_count++] = "Vmx";
    if (_RSHIFT(raw, 9, 1))
        flags[flag_count++] = "Tmn";
    if (_RSHIFT(raw, 13, 1))
        flags[flag_count++] = "Tmx";
    if (_RSHIFT(raw, 10, 1))
        flags[flag_count++] = "Smn";
    if (_RSHIFT(raw, 14, 1))
        flags[flag_count++] = "Smx";

    // generate string
    for (int j = 0; j < flag_count; j++) {
        if (j != 0) {
            strncat(status_string, " | ", sizeof(status_string) - 1);
        }
        strncat(status_string, flags[j], sizeof(status_string) - 1);
    }
    previous_status = raw;

    return status_string;
}

/**
 * @brief converts the raw value of the BMS protAlert register to a human
 * readable string.
 *
 * @param raw raw protAlert register value.
 * @return const char*
 */
static const char *__protAlrt_to_str(uint16_t raw) {
    static char protAlrt_string[160] = "";
    static uint16_t previous_protAlrt = 0;
    char *flags[16] = {};
    int flag_count = 0;

    // don't do work we don't have to do
    if (raw == previous_protAlrt) {
        return protAlrt_string;
    }

    // clear old string
    protAlrt_string[0] = '\0';

    // check if no flags
    if (raw == 0) {
        previous_protAlrt = 0;
        return protAlrt_string;
    }

    if (_RSHIFT(raw, 15, 1))
        flags[flag_count++] = "ChgWDT";
    if (_RSHIFT(raw, 14, 1))
        flags[flag_count++] = "TooHotC";
    if (_RSHIFT(raw, 13, 1))
        flags[flag_count++] = "Full";
    if (_RSHIFT(raw, 12, 1))
        flags[flag_count++] = "TooColdC";
    if (_RSHIFT(raw, 11, 1))
        flags[flag_count++] = "OVP";
    if (_RSHIFT(raw, 10, 1))
        flags[flag_count++] = "OCCP";
    if (_RSHIFT(raw, 9, 1))
        flags[flag_count++] = "Qovflw";
    if (_RSHIFT(raw, 8, 1))
        flags[flag_count++] = "PrepF";
    if (_RSHIFT(raw, 7, 1))
        flags[flag_count++] = "Imbalance";
    if (_RSHIFT(raw, 6, 1))
        flags[flag_count++] = "PermFail";
    if (_RSHIFT(raw, 5, 1))
        flags[flag_count++] = "DieHot";
    if (_RSHIFT(raw, 4, 1))
        flags[flag_count++] = "TooHotD";
    if (_RSHIFT(raw, 3, 1))
        flags[flag_count++] = "UVP";
    if (_RSHIFT(raw, 2, 1))
        flags[flag_count++] = "ODCP";
    if (_RSHIFT(raw, 1, 1))
        flags[flag_count++] = "ResDFault";
    if (_RSHIFT(raw, 0, 1))
        flags[flag_count++] = "LDet";

    // generate string
    for (int j = 0; j < flag_count; j++) {
        if (j != 0) {
            strncat(protAlrt_string, " | ", sizeof(protAlrt_string) - 1);
        }
        strncat(protAlrt_string, flags[j], sizeof(protAlrt_string) - 1);
    }
    previous_protAlrt = raw;

    return protAlrt_string;
}

static void log_battery_open_csv_file(void) {
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, LOG_BATTERY_FILENAME);
    int is_new_file = 0;
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
        // Error_Handler();
    } else if ((fx_result != FX_ALREADY_CREATED)) {
        is_new_file = 1;
        CETI_LOG("Created new battery file \"%s\"", LOG_BATTERY_FILENAME);
    }

    fx_result = fx_file_open(&sdio_disk, &log_battery_file, LOG_BATTERY_FILENAME, FX_OPEN_FOR_WRITE);
    if (fx_result != FX_SUCCESS) {
        // Error_Handler();
    }
    CETI_LOG("Opened battery file \"%s\"", LOG_BATTERY_FILENAME);

    if (!is_new_file) {
        return;
    }   

    // write header
    fx_result = fx_file_write(&log_battery_file, log_battery_csv_header, strlen(log_battery_csv_header));
    if(fx_result != FX_SUCCESS){
        // Error_Handler();
    }
    
}

int log_battery_sample_to_csv(const CetiBatterySample * pSample, uint8_t *pBuffer, size_t buffer_len) {
    uint8_t offset = 0;
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, "%lld", pSample->time_us);

    /* ToDo: note conversion */
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", ");
    // offset += snprintf(&pBuffer[offset], buffer_len - offset, ", %s", pSample->error);

    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_voltage_v[0]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_voltage_v[1]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->current_mA);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_temperature_c[0]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_temperature_c[1]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->state_of_charge_percent);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %s", __status_to_str(pSample->status));
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %s", __protAlrt_to_str(pSample->protection_alert));
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, "\n");
    return offset;
}

void log_battery_enable(void) {
    // ToDo: create file for battery data
    log_battery_open_csv_file();
}

void log_battery_task(void) {
    size_t encoded_bytes = 0;

    // check if new samples to log
    const CetiBatterySample* pSample = acq_battery_get_next_sample();
    while (pSample != NULL) {
        //encode
#if LOG_BATTERY_FORMAT == LOG_BATTERY_FORMAT_CSV
        encoded_bytes += log_battery_sample_to_csv(pSample, &log_battery_encode_buffer[encoded_bytes], LOG_BATTERY_ENCODE_BUFFER_SIZE - encoded_bytes);
#endif
        if (encoded_bytes < 0) {
            // ToDo: error handling
        }

        if (encoded_bytes > LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD) {
            // Flush buffer to SD card
            UINT fx_result = fx_file_write(&log_battery_file, log_battery_encode_buffer, encoded_bytes);
            if (fx_result != FX_SUCCESS) {
               //  ToDo: Handle Errors
            }
            encoded_bytes = 0;
        }
        pSample = acq_battery_get_next_sample();
    }

    // write remaining samples to SD card
    if (encoded_bytes != 0) {
        // Flush buffer to SD card
        UINT fx_result = fx_file_write(&log_battery_file, log_battery_encode_buffer, encoded_bytes);
        if (fx_result != FX_SUCCESS) {
            //  ToDo: Handle Errors
        }
    }
}
