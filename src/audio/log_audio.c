/*****************************************************************************
 *   @file      audio/log_audio.c
 *   @brief     Audio sample processing and logging code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
// local files
#include "log_audio.h"
#include "acq_audio.h"

#include "log/log_syslog.h"
#include "timing.h"

// stm libraries
#include <app_filex.h>
#include <main.h>

// std libraries
#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef enum {
    AUDIO_LOG_RAW,
    AUDIO_LOG_FLAC,
} AudioLogTypes;

#define AUDIO_LOG_TYPE AUDIO_LOG_RAW
static uint8_t s_log_audio_enabled = 0;
static int s_audio_log_page = 0;

static time_t s_audio_start_time_us;
static char audiofilename[32] = {};

extern FX_MEDIA sdio_disk;
FX_FILE audio_file = {};

#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
static void log_audio_create_raw_file(void) {
    /* Create file based on RTC time */
    s_audio_start_time_us = rtc_get_epoch_us();
    snprintf(audiofilename, sizeof(audiofilename) - 1, "%lld.bin", s_audio_start_time_us);

    /* Create/open audio file */
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, audiofilename);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
    	Error_Handler();
    }

    CETI_LOG("Created new audio file \"%s\"", audiofilename);
}

int log_audio_raw_write(uint8_t *pData, uint32_t size) {
    static uint16_t write_count = 0;

    // copy data directly to SD card
    UINT fx_result = fx_file_open(&sdio_disk, &audio_file, audiofilename, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS == fx_result) { fx_result = fx_file_seek(&audio_file, -1); };
    // if (FX_SUCCESS == fx_result) { fx_result = fx_file_write_notify_set(&audio_file, audio_SDWriteComplete); };
    if (FX_SUCCESS == fx_result) { fx_result = fx_file_write(&audio_file, pData, size); };
    if (FX_SUCCESS != fx_result) {
        #warning ToDo: handle log_audio_raw_write error
    }
    fx_file_close(&audio_file);
    write_count++;

    // check if new file needs to be created
    time_t now_us = rtc_get_epoch_us();
    if (now_us - s_audio_start_time_us >= 5*60*1000000) {
        log_audio_create_raw_file();
    }
    return 0;
}
#endif

void log_audio_init(void) {
    // enable audio acquisition
#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
    log_audio_create_raw_file();
    acq_audio_set_log_callback(log_audio_raw_write);
#endif
    s_log_audio_enabled = 1;
}

void log_audio_disable(void) {
	if (!s_log_audio_enabled) {
		return;
	}

#warning ToDo: log_audio_disable  flush partial audio buffer

    s_log_audio_enabled = 0;
}
