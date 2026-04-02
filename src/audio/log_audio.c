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

#include "syslog.h"
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

static time_t s_audio_start_time_us;
static char audiofilename[64] = {};
static char audiofilename_extension[32] = {};

extern FX_MEDIA sdio_disk;
FX_FILE audio_file = {};

#define AUDIO_FILE_DURATION_S (5 * 60)

// #define AUDIO_BUFFER_SIZE_BYTES (AUDIO_LOG_BUFFER_SIZE_BLOCKS * AUDIO_LOG_BLOCK_SIZE)
// #define BYTES_PER_SAMPLE (AUDIO_CHANNEL_COUNT * ((AUDIO_SAMPLE_BITDEPTH + (8-1))/8)) //(channel_count*(bitdepth/8))
// #define SAMPLES_PER_BUFFER ((AUDIO_BUFFER_SIZE_BYTES/2)/BYTES_PER_SAMPLE)
// #define TARGET_FILE_SIZE_SAMPLES  (AUDIO_FILE_DURATION_S * AUDIO_SAMPLERATE_SPS)
// #define FILE_SIZE_BUFFERS ((uint32_t)((TARGET_FILE_SIZE_SAMPLES + (SAMPLES_PER_BUFFER - 1)) / SAMPLES_PER_BUFFER))
// #define EXPECTED_FILE_SIZE_BYTES (FILE_SIZE_BUFFERS * (AUDIO_BUFFER_SIZE_BYTES/2))

#define AUDIO_BUFFER_BLOCK_TO_HALF(block) ((block) / (AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2))
union {
    uint8_t block[AUDIO_LOG_BUFFER_SIZE_BLOCKS][AUDIO_LOG_BLOCK_SIZE];        // accessed for DMA
    uint8_t half[2][AUDIO_LOG_BLOCK_SIZE * AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2]; // access for SD card write
} g_audio_buffer;

static uint8_t s_audio_buffer_read_half = 0;
static volatile uint8_t s_audio_buffer_write_half = 0;
static uint8_t sd_card_writing = 0;

uint8_t *log_audio_buffer = g_audio_buffer.block[0];


#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
static void log_audio_create_raw_file(void) {
    /* Create file based on RTC time */
    s_audio_start_time_us = rtc_get_epoch_us();
    snprintf(audiofilename, sizeof(audiofilename) - 1, 
        "%lld.%s", 
        s_audio_start_time_us, audiofilename_extension 
    );

    /* Create/open audio file */
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, audiofilename);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
        Error_Handler();
    }
    CETI_LOG("Created new audio file \"%s\"", audiofilename);

    /* Try to allocate expected file size */
    fx_result = fx_file_open(&sdio_disk, &audio_file, audiofilename, FX_OPEN_FOR_WRITE);
}

void log_audio_deinit(void) {
    fx_file_close(&audio_file);
}

int log_audio_raw_write(uint8_t *pData, uint32_t size) {
    static uint16_t write_count = 0;

    // copy data directly to SD card
    UINT fx_result = fx_file_write(&audio_file, pData, size);
    if (FX_SUCCESS != fx_result) {
#warning ToDo: handle log_audio_raw_write error
    }
    write_count++;

    // check if new file needs to be created
    time_t now_us = rtc_get_epoch_us();
    if (now_us - s_audio_start_time_us >= AUDIO_FILE_DURATION_S * 1000000) {
        fx_file_close(&audio_file);
        log_audio_create_raw_file();
    }
    return 0;
}
#endif

void log_audio_init(const AudioConfig *settings) {
    if (!settings->enabled){
        return;
    }

    uint8_t channel_count = 0;
    for (int i = 0; i < 4; i++) {
        channel_count += settings->channel_enabled[i];
    }
    snprintf(audiofilename_extension, sizeof(audiofilename_extension) - 1, 
        "be.%dbit.%dch.%ldsps.bin", 
        settings->bitdepth, channel_count, settings->samplerate_sps 
    );

    // enable audio acquisition
#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
    log_audio_create_raw_file();
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

void log_audio_block_complete_callback(uint16_t block_index) {
    // check if audio buffer is half full
    if (s_audio_buffer_read_half != AUDIO_BUFFER_BLOCK_TO_HALF(block_index)) {
        if (!sd_card_writing) {
            // signal sd card write
            sd_card_writing = 1;
            s_audio_buffer_write_half = AUDIO_BUFFER_BLOCK_TO_HALF(block_index);
            HAL_PWR_DisableSleepOnExit(); // ensures control transitions to CPU for SD card write
        } else {
            // ToDo: SD card write too slow
        }
    }
}

/// @note Expected SD card write intervals:
///     Min: 32 * 56.9 mS = 1.82 S
///     Max: 32 * 28.444 mS = 910 mS
void log_audio_task(void) {
    uint8_t nv_read_half = s_audio_buffer_read_half;
    if (nv_read_half != s_audio_buffer_write_half) {
        uint8_t *p_data;
        size_t data_size;
        p_data = g_audio_buffer.half[nv_read_half];
        data_size = AUDIO_LOG_BLOCK_SIZE * AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2;
        log_audio_raw_write(p_data, data_size);
        sd_card_writing = 0;
        s_audio_buffer_read_half = s_audio_buffer_write_half;
    }
}