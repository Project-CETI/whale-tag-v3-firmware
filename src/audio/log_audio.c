/*****************************************************************************
 *   @file      audio/log_audio.c
 *   @brief     Audio sample processing and logging code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
// # Definitions:
// Sample: 1 N-channel audio sample
// Block: largest DMA transferable sample aligned size
// Page: size half of entire audio buffer

// local files
#include "log_audio.h"
#include "acq_audio.h"

#include "error.h"
#include "metadata.h"
#include "syslog.h"
#include "timing.h"

// stm libraries
#include <app_filex.h>
#include <main.h>

// std libraries
#include <stdint.h>
#include <stdio.h>
#include <time.h>

// macros
#define AUDIO_LOG_TYPE AUDIO_LOG_RAW

#define TARGET_AUDIO_FILE_DURATION_S (5 * 60)

#define AUDIO_PAGE_SIZE_BYTES (AUDIO_LOG_BLOCK_SIZE * AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2)
#define AUDIO_BLOCKS_PER_PAGE (AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2)

#define AUDIO_BUFFER_BLOCK_TO_HALF(block) ((block) / (AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2))

// typedefs
typedef enum {
    AUDIO_LOG_RAW,
    AUDIO_LOG_FLAC,
} AudioLogTypes;

// external variables
extern FX_MEDIA sdio_disk;

// private variables
static uint8_t s_log_audio_initialized = 0;
static uint64_t s_audio_start_time_us;
static char s_audio_filename[64] = {};
static char s_audio_filename_extension[32] = {};

static FX_FILE s_audio_file = {};

static uint8_t s_audio_buffer_read_half = 0;
static volatile uint8_t s_audio_buffer_write_half = 0;
static uint64_t s_expected_filesize_pages = 0;
static uint64_t s_current_filesize_bytes = 0;

// public variables
union {
    uint8_t block[AUDIO_LOG_BUFFER_SIZE_BLOCKS][AUDIO_LOG_BLOCK_SIZE]; // accessed for DMA
    uint8_t half[2][AUDIO_PAGE_SIZE_BYTES];                            // access for SD card write
} g_audio_buffer;

uint8_t *log_audio_buffer = g_audio_buffer.block[0];

#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
/// @brief Creates a new raw audio file on the SD card
/// @note The filename is derived from the current RTC epoch timestamp
///       and the pre-configured extension string. The file is
///       pre-allocated to s_expected_filesize_pages to reduce
///       fragmentation during streaming writes.
static void log_audio_create_raw_file(void) {
    /* Create file based on RTC time */
    s_audio_start_time_us = rtc_get_epoch_us();
    snprintf(s_audio_filename, sizeof(s_audio_filename) - 1,
             "%lld.%s",
             s_audio_start_time_us, s_audio_filename_extension);

    /* Create/open audio file */
    UINT fx_create_result = fx_file_create(&sdio_disk, s_audio_filename);
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_AUDIO, ERR_TYPE_FILEX, fx_create_result), log_audio_create_raw_file);
        return;
    }

    CETI_LOG("Created new audio file \"%s\"", s_audio_filename);
    /* Try to allocate expected file size */
    UINT fx_open_result = fx_file_open(&sdio_disk, &s_audio_file, s_audio_filename, FX_OPEN_FOR_WRITE);
    if ((FX_SUCCESS != fx_open_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_AUDIO, ERR_TYPE_FILEX, fx_open_result), log_audio_create_raw_file);
        return;
    }

    s_current_filesize_bytes = 0;
    CETI_LOG("New audio file \"%s\" allocated", s_audio_filename);
    UINT fx_allocation_result = fx_file_allocate(&s_audio_file, s_expected_filesize_pages * AUDIO_PAGE_SIZE_BYTES);
    if ((FX_SUCCESS != fx_allocation_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_AUDIO, ERR_TYPE_FILEX, fx_allocation_result), log_audio_create_raw_file);
        return;
    }

    metadata_log_file_creation(s_audio_filename, DATA_TYPE_AUDIO, DATA_FORMAT_BIN, 0);
}

/// @brief Closes the current audio log file
/// @param
void log_audio_deinit(void) {
    fx_file_close(&s_audio_file);
}

/// @brief Writes raw audio data to the current audio file
/// @param pData pointer to audio data buffer
/// @param size number of bytes to write
/// @return 0 on completion
/// @note Automatically rotates to a new file when the current file
///       reaches the expected file size.
int log_audio_raw_write(uint8_t *pData, uint32_t size) {
    // copy data directly to SD card
    UINT fx_result = fx_file_write(&s_audio_file, pData, size);
    if (FX_SUCCESS != fx_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_AUDIO, ERR_TYPE_FILEX, fx_result), log_audio_raw_write);
    }
    s_current_filesize_bytes += size;
    // check if new file needs to be created
    if (s_current_filesize_bytes >= s_expected_filesize_pages * AUDIO_PAGE_SIZE_BYTES) {
        fx_file_close(&s_audio_file);
        log_audio_create_raw_file();
    }
    return 0;
}
#endif

/// @brief Initializes the audio logging subsystem
/// @param settings pointer to audio configuration (channel enables,
///        bitdepth, sample rate)
/// @note Computes the expected file size and creates the first audio
///       file. Does nothing if audio is disabled in settings.
void log_audio_init(const AudioConfig *settings) {
    if (!settings->enabled) {
        return;
    }

    uint8_t channel_count = 0;
    for (int i = 0; i < 4; i++) {
        channel_count += settings->channel_enabled[i];
    }

    uint32_t bytes_per_sample = (settings->bitdepth / 8) * channel_count;
    uint32_t samples_per_block = AUDIO_LOG_BLOCK_SIZE / bytes_per_sample;
    uint32_t samples_per_page = samples_per_block * AUDIO_BLOCKS_PER_PAGE;
    uint64_t target_filesize_samples = TARGET_AUDIO_FILE_DURATION_S * settings->samplerate_sps;

    // round target_filesize to nearest number of pages for expected file size
    s_expected_filesize_pages = (target_filesize_samples + (samples_per_page / 2)) / samples_per_page;
    snprintf(s_audio_filename_extension, sizeof(s_audio_filename_extension) - 1,
             "be.%dbit.%dch.%ldsps.bin",
             settings->bitdepth, channel_count, settings->samplerate_sps);

    // enable audio acquisition
#if AUDIO_LOG_TYPE == AUDIO_LOG_RAW
    log_audio_create_raw_file();
#endif
    s_log_audio_initialized = 1;
}

/// @brief Disables audio logging
/// @param
/// @warning Does not flush any partially filled audio buffer
void log_audio_disable(void) {
    if (!s_log_audio_initialized) {
        return;
    }

#warning ToDo: log_audio_disable  flush partial audio buffer

    s_log_audio_initialized = 0;
}

/// @brief DMA block-complete callback for audio acquisition
/// @param block_index index of the block that just finished filling
/// @note Called from interrupt context. Signals the main loop to
///       write a page to SD when a buffer half boundary is crossed.
void log_audio_block_complete_callback(uint16_t block_index) {
    // check if audio buffer is half full
    if (s_audio_buffer_write_half != AUDIO_BUFFER_BLOCK_TO_HALF(block_index)) {
        // signal sd card write
        s_audio_buffer_write_half = AUDIO_BUFFER_BLOCK_TO_HALF(block_index);
        HAL_PWR_DisableSleepOnExit(); // ensures control transitions to CPU for SD card write
    }
}

/// @brief Drains completed audio buffer halves to the SD card
/// @param
/// @note Call periodically from the main loop. Writes one page per
///       completed buffer half.
/// @note Expected SD card write intervals:
///     Min: 32 * 56.9 mS = 1.82 S
///     Max: 32 * 28.444 mS = 910 mS
void log_audio_task(void) {
    uint8_t nv_read_half = s_audio_buffer_read_half;
    while (nv_read_half != s_audio_buffer_write_half) {
        uint8_t *p_data;
        size_t data_size;
        p_data = g_audio_buffer.half[nv_read_half];
        data_size = AUDIO_LOG_BLOCK_SIZE * AUDIO_LOG_BUFFER_SIZE_BLOCKS / 2;
        log_audio_raw_write(p_data, data_size);
        nv_read_half ^= 1;
        s_audio_buffer_read_half = nv_read_half;
    }
}