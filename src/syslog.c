#include "syslog.h"

#include "util/buffer_writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stm32u5xx_hal.h>

#include "main.h"
#include "version.h"

#ifdef DEBUG
#include "core_cm33.h"

void itm_printf(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    for (int i = 0; i < len && i < (int)sizeof(buf); i++) {
        ITM_SendChar(buf[i]);
    }
}
#endif

extern RTC_HandleTypeDef hrtc;
extern FX_MEDIA sdio_disk;

#define SYSLOG_FILENAME "syslog.log"

#define SYSLOG_BUFFER_THRESHOLD (512)
#define SYSLOG_BUFFER_SIZE (SYSLOG_BUFFER_THRESHOLD + 512)
static uint8_t syslog_buffer[SYSLOG_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = syslog_buffer,
    .threshold = SYSLOG_BUFFER_THRESHOLD,
    .capacity = SYSLOG_BUFFER_SIZE,
};

void syslog_init(void) {
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, SYSLOG_FILENAME);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
        Error_Handler();
    }

    UINT fx_open_result = buffer_writer_open(&s_bw, SYSLOG_FILENAME);
    if ((fx_open_result != FX_SUCCESS)) {
        Error_Handler();
    }
    syslog_write("system log initialized");
    syslog_write("Firmware Compiled:" FW_COMPILATION_DATE);
    syslog_write(FW_VERSION_TEXT);
}

void syslog_deinit(void) {
    buffer_writer_close(&s_bw);
}

void syslog_flush(void) {
    buffer_writer_flush(&s_bw);
}

// call this function to write to the system log
UINT priv__syslog_write(const str identifier[static 1], const char fmt[static 1], ...) {
    uint8_t scratch_buffer[1024] = {};
    uint8_t *position = &scratch_buffer[0];

    // add date/time to scratch buffer
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, 0);
    HAL_RTC_GetDate(&hrtc, &date, 0);
    position += snprintf(
        (char *)position, sizeof(scratch_buffer) - (position - scratch_buffer) - 1,
        "20%02d-%02d-%02d %02d:%02d:%02d ",
        date.Year, date.Month, date.Date, time.Hours, time.Minutes, time.Seconds);

    // add calling identifier to data buffer
    memcpy(position, identifier->ptr, identifier->length);
    position += identifier->length;
    memcpy(position, ": ", 2);
    position += 2;

    // append user generated message
    va_list ap;
    va_start(ap, fmt);
    position += vsnprintf((char *)position, sizeof(scratch_buffer) - (position - scratch_buffer) - 1, fmt, ap);
    va_end(ap);

    *position = '\n';
    position++;

    // Write to SWD if in DEBUG mode
#ifdef DEBUG
    for (int i = 0; i < (position - scratch_buffer) && i < (int)sizeof(scratch_buffer); i++) {
        ITM_SendChar(scratch_buffer[i]);
    }
#endif

    UINT fx_result = buffer_writer_write(&s_bw, scratch_buffer, (position - scratch_buffer));
    if (fx_result != FX_SUCCESS) {
        Error_Handler();
    }

    return fx_result;
}
