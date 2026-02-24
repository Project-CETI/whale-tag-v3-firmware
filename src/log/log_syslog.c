#include "log/log_syslog.h"

#include <stm32u5xx_hal.h>
#include <stdio.h>
#include <stdarg.h>

#include "main.h"
#include "version.h"

#define SYSLOG_FILENAME "syslog.log"

extern RTC_HandleTypeDef hrtc;
extern FX_MEDIA sdio_disk;
FX_FILE syslog_file = {};

void syslog_init(void) {
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, SYSLOG_FILENAME);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
    	Error_Handler();
    }

    syslog_write("system log initialized");
    syslog_write("Firmware Compiled:" FW_COMPILATION_DATE);
    syslog_write(FW_VERSION_TEXT);
}

// // call this function to write to the system log
static uint8_t scratch_buffer[1024] = {};
UINT __syslog_write(const str *identifier, const char *fmt, ...) {
    uint8_t *position = &scratch_buffer[0];

    // add date/time to scratch buffer
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    HAL_RTC_GetDate(&hrtc, &date, 0);
    HAL_RTC_GetTime(&hrtc, &time, 0);
    position += snprintf(
        (char *)position, sizeof(scratch_buffer) - (position - scratch_buffer) - 1, 
        "20%02d-%02d-%02d %02d:%02d:%02d ",
        date.Year, date.Month, date.Date, time.Hours, time.Minutes, time.Seconds
    );

    // add calling identifier to data buffer
    memcpy(position, identifier->ptr, identifier->length);
    position += identifier->length;
    memcpy(position, ": ", 2);
    position += 2;

    // append user generated message
    va_list ap;
    va_start(ap,fmt);
    position += vsnprintf((char *)position, sizeof(scratch_buffer) - (position - scratch_buffer) - 1, fmt, ap);
    va_end(ap);

    *position = '\n';
    position++;

    UINT fx_result = fx_file_open(&sdio_disk, &syslog_file, SYSLOG_FILENAME, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS == fx_result) { fx_result = fx_file_seek(&syslog_file, -1); }
    if (FX_SUCCESS == fx_result) {fx_result = fx_file_write(&syslog_file, scratch_buffer, (position - scratch_buffer));}
    if (fx_result != FX_SUCCESS) {
        fx_file_close(&syslog_file);
    	Error_Handler();
    }
    fx_file_close(&syslog_file);

    return fx_result;
}
