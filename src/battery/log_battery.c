/*****************************************************************************
 *   @file      battery/log_battery.c
 *   @brief     code for saving acquired battery data to disk
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "main.h"

#include "acq_battery.h"
#include "log_battery.h"

#include "syslog.h"
#include "util/buffer_writer.h"

#include <app_filex.h>
#include <stdio.h>


/********* Buffered File *****************************************************/
#define LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD (3 * 1024)
#define LOG_BATTERY_ENCODE_BUFFER_SIZE (LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t log_battery_encode_buffer[LOG_BATTERY_ENCODE_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = log_battery_encode_buffer,
    .threshold = LOG_BATTERY_ENCODE_BUFFER_FLUSH_THRESHOLD,
    .capacity = LOG_BATTERY_ENCODE_BUFFER_SIZE,
};

/********* CSV ***************************************************************/
#define LOG_BATTERY_FILENAME "data_battery.csv"

#define _RSHIFT(x, s, w) (((x) >> s) & ((1 << w) - 1))
#define _LSHIFT(x, s, w) (((x) & ((1 << w) - 1)) << s)

extern FX_MEDIA sdio_disk;

static char *log_battery_csv_header =
    "Timestamp [us]"
    ", Notes"
    ", Battery V1 [V]"
    ", Battery V2 [V]"
    ", Battery I [mA]"
    ", Battery Average Power [mW]"
    ", Battery T1 [C]"
    ", Battery T2 [C]"
    ", State of Charge [\%]"
    ", Status"
    ", Protection Alerts"
    "\n";

/// @brief converts the raw value of the BMS status register to a human
/// readable string.
/// @param raw raw status register value.
/// @return 
/// @note not thread safe
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
    if (_RSHIFT(raw, 15, 1)) { flags[flag_count++] = "PA";  }
    if (_RSHIFT(raw, 1, 1))  { flags[flag_count++] = "POR"; }
    // if(_RSHIFT(raw, 7, 1)) flags[flag_count++] = "dSOCi"; //ignored indicates interger change in SoC
    if (_RSHIFT(raw, 2, 1))  { flags[flag_count++] = "Imn"; }
    if (_RSHIFT(raw, 6, 1))  { flags[flag_count++] = "Imx"; }
    if (_RSHIFT(raw, 8, 1))  { flags[flag_count++] = "Vmn"; }
    if (_RSHIFT(raw, 12, 1)) { flags[flag_count++] = "Vmx"; }
    if (_RSHIFT(raw, 9, 1))  { flags[flag_count++] = "Tmn"; }
    if (_RSHIFT(raw, 13, 1)) { flags[flag_count++] = "Tmx"; }
    if (_RSHIFT(raw, 10, 1)) { flags[flag_count++] = "Smn"; }
    if (_RSHIFT(raw, 14, 1)) { flags[flag_count++] = "Smx"; }

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

/// @brief converts the raw value of the BMS protAlert register to a human
/// readable string.
/// @param raw raw protAlert register value.
/// @return 
/// @note not thread safe
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

    if (_RSHIFT(raw, 15, 1)) { flags[flag_count++] = "ChgWDT";    }
    if (_RSHIFT(raw, 14, 1)) { flags[flag_count++] = "TooHotC";   }
    if (_RSHIFT(raw, 13, 1)) { flags[flag_count++] = "Full";      }
    if (_RSHIFT(raw, 12, 1)) { flags[flag_count++] = "TooColdC";  }
    if (_RSHIFT(raw, 11, 1)) { flags[flag_count++] = "OVP";       }
    if (_RSHIFT(raw, 10, 1)) { flags[flag_count++] = "OCCP";      }
    if (_RSHIFT(raw, 9, 1))  { flags[flag_count++] = "Qovflw";    }
    if (_RSHIFT(raw, 8, 1))  { flags[flag_count++] = "PrepF";     }
    if (_RSHIFT(raw, 7, 1))  { flags[flag_count++] = "Imbalance"; }
    if (_RSHIFT(raw, 6, 1))  { flags[flag_count++] = "PermFail";  }
    if (_RSHIFT(raw, 5, 1))  { flags[flag_count++] = "DieHot";    }
    if (_RSHIFT(raw, 4, 1))  { flags[flag_count++] = "TooHotD";   }
    if (_RSHIFT(raw, 3, 1))  { flags[flag_count++] = "UVP";       }
    if (_RSHIFT(raw, 2, 1))  { flags[flag_count++] = "ODCP";      }
    if (_RSHIFT(raw, 1, 1))  { flags[flag_count++] = "ResDFault"; }
    if (_RSHIFT(raw, 0, 1))  { flags[flag_count++] = "LDet";      }

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

/// @brief opens/creates a new csv file. initializes csv header
/// @param filename 
static void __open_csv_file(char *filename) {
    /* Create/open pressure file */
    UINT fx_create_result = fx_file_create(&sdio_disk, filename);
    if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
        #warning "ToDo: Handle Error creating file"
        // Error_Handler();
    }

    /* open file */
    UINT fx_open_result = buffer_writer_open(&s_bw, filename);
    if (FX_SUCCESS != fx_open_result) {
        #warning "ToDo: Handle Error opening file"
        return;
    }

    /* file was newly created. Initialize header */
    if (FX_ALREADY_CREATED != fx_create_result) {
        CETI_LOG("Created new battery file \"%s\"", filename);
        buffer_writer_write(&s_bw, (uint8_t *)log_battery_csv_header, strlen(log_battery_csv_header));
        buffer_writer_flush(&s_bw);
    }
}

/// @brief encodes a CetiBatterySample to a CSV line
/// @param pSample pointer to sample to be encoded
/// @param pBuffer output buffer
/// @param buffer_len buffer capacity
/// @return number of bytes in encoded output
static int __sample_to_csv(const CetiBatterySample *pSample, uint8_t *pBuffer, size_t buffer_len) {
    uint8_t offset = 0;
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, "%lld", pSample->time_us);

    /* ToDo: note conversion */
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", ");
    // offset += snprintf(&pBuffer[offset], buffer_len - offset, ", %s", pSample->error);

    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_voltage_v[0]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_voltage_v[1]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->current_mA);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->average_power_mw);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_temperature_c[0]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->cell_temperature_c[0]);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %.3f", pSample->state_of_charge_percent);
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %s", __status_to_str(pSample->status));
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, ", %s", __protAlrt_to_str(pSample->protection_alert));
    offset += snprintf((char *)&pBuffer[offset], buffer_len - offset, "\n");
    return offset;
}

/********* Sample Buffer *****************************************************/
#define LOG_BATTERY_BUFFER_SIZE (8)
static CetiBatterySample s_sample_buffer[LOG_BATTERY_BUFFER_SIZE];
static volatile size_t s_sample_buffer_write_cursor = 0;
static size_t s_sample_buffer_read_cursor = 0;

/// @brief add a sample to the sample logging buffer
/// @param p_sample pointer to sample to be buffered
void log_battery_buffer_sample(const CetiBatterySample *p_sample) {
    static uint8_t overflow = 0;
    CetiBatterySample *p_buffer = &s_sample_buffer[s_sample_buffer_write_cursor];
    *p_buffer = *p_sample;
    if (overflow) {
        p_buffer->error = 1;
        overflow = 0;
    }

    size_t next_w_pos = (s_sample_buffer_write_cursor + 1) % LOG_BATTERY_BUFFER_SIZE;
    /* check for overflow */
    if (next_w_pos == s_sample_buffer_read_cursor) {
        overflow = 1;
    } else {
        s_sample_buffer_write_cursor = next_w_pos;
    }
    if(log_battery_sample_buffer_is_half_full()) {
        HAL_PWR_DisableSleepOnExit();
    }

}

/// @brief initializes battery logging subsystem
/// @param  
void log_battery_init(void) {
    // create output file for battery data
    __open_csv_file(LOG_BATTERY_FILENAME);
}

/// @brief Perfom non-time critical logging tasks. Call periodically.
/// @param  
void log_battery_task(void) {
    // check if new samples to log
    while (s_sample_buffer_read_cursor != s_sample_buffer_write_cursor) {
        uint8_t csv_encode_buffer[512];
        const CetiBatterySample *p_sample = &s_sample_buffer[s_sample_buffer_read_cursor];
        size_t encoded_bytes = __sample_to_csv(p_sample, csv_encode_buffer, sizeof(csv_encode_buffer));
        UINT write_result = buffer_writer_write(&s_bw, csv_encode_buffer, encoded_bytes);
        if (FX_SUCCESS != write_result) {
            #warning "ToDo: Handle write error"
        }
        s_sample_buffer_read_cursor = (1 + s_sample_buffer_read_cursor) % LOG_BATTERY_BUFFER_SIZE;
    }
}

/// @brief returns if the sample buffer is atleast half full.
/// @param  
/// @return bool
int log_battery_sample_buffer_is_half_full(void) {
    int8_t buffered_samples = ((int8_t)s_sample_buffer_write_cursor - (int8_t)s_sample_buffer_read_cursor);
    buffered_samples = (buffered_samples >= 0) ? buffered_samples :  LOG_BATTERY_BUFFER_SIZE + buffered_samples;
    return (buffered_samples >= LOG_BATTERY_BUFFER_SIZE/2);
}