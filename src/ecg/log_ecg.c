/*****************************************************************************
 *   @file      ecg/log_ecg.c
 *   @brief     ECG data acquisition and logging code
 *   @project   Project CETI
 *   @date      04/14/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "log_ecg.h"
#include "acq_ecg.h"

#include "error.h"
#include "metadata.h"
#include "util/buffer_writer.h"

#include <app_filex.h>
#include <stdio.h>


#define ECG_CSV_FILENAME "data_ecg.csv"
#define ECG_SAMPLE_BUFFER_LENGTH (2*1024)
static EcgSample s_ecg_sample_buffer[ECG_SAMPLE_BUFFER_LENGTH];
static volatile uint16_t s_ecg_buffer_write_cursor = 0;
static uint16_t s_ecg_buffer_read_cursor = 0;


#define LOG_ECG_ENCODE_BUFFER_FLUSH_THRESHOLD (7 * 512)
#define LOG_ECG_ENCODE_BUFFER_SIZE (LOG_ECG_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t log_ecg_encode_buffer[LOG_ECG_ENCODE_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = log_ecg_encode_buffer,
    .threshold = LOG_ECG_ENCODE_BUFFER_FLUSH_THRESHOLD,
    .capacity = LOG_ECG_ENCODE_BUFFER_SIZE,
};

extern FX_MEDIA sdio_disk;

static char *log_ecg_csv_header = 
    "Timestamp [us]"
    ", Notes"
    ", Ecg Value"
    ", Ecg LoD+"
    ", Ecg LoD-"
    "\n";

/// @brief opens and creates an ecg file
/// @param filename filename string pointer
[[gnu::nonnull]]
static void priv__open_csv_file(char *filename) {
    /* Create/open ecg csv file */
    UINT fx_create_result = fx_file_create(&sdio_disk, filename);
    if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ECG, ERR_TYPE_FILEX, fx_create_result), priv__open_csv_file);
    }

    /* open file */
    UINT fx_open_result = buffer_writer_open(&s_bw, filename);
    if (FX_SUCCESS != fx_open_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ECG, ERR_TYPE_FILEX, fx_open_result), priv__open_csv_file);
    }
    
    /* log file creation */
    metadata_log_file_creation(filename, DATA_TYPE_ECG, DATA_FORMAT_CSV, 0);

    /* file was newly created. Initialize header */
    if (FX_ALREADY_CREATED != fx_create_result) {
        buffer_writer_write(&s_bw, (uint8_t *)log_ecg_csv_header, strlen(log_ecg_csv_header));
        buffer_writer_flush(&s_bw);
    }
}

/// @brief encodes an ECG sample as a csv line string
/// @param p_sample pointer to sample to be converted
/// @param p_buffer pointer to destination buffer
/// @param buffer_len destination buffer capacity
/// @return encoded string length
[[gnu::nonnull]]
static int priv__sample_to_csv(const EcgSample *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    uint8_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp_us);

    /* ToDo: note conversion */
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf(&p_buffer[offset], buffer_len - offset, ", %s", p_sample->error);

    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %ld", p_sample->value);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->lod_p);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %d", p_sample->lod_n);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

/// @brief initialize ecg logging
/// @param  
void log_ecg_init(void) {
    // zero buffer
    s_ecg_buffer_write_cursor = 0;
    s_ecg_buffer_read_cursor = 0;

    // create file
    priv__open_csv_file(ECG_CSV_FILENAME);
}

/// @brief indication if the ecg sample buffer is halffull and needs servicing
/// @param  
/// @return bool
[[gnu::pure]]
int log_ecg_sample_buffer_is_half_full(void) {
    int16_t buffered_samples = ((int16_t)s_ecg_buffer_write_cursor - (int16_t)s_ecg_buffer_read_cursor);
    buffered_samples = (buffered_samples >= 0) ? buffered_samples : ECG_SAMPLE_BUFFER_LENGTH + buffered_samples;
    return (buffered_samples >= ( ECG_SAMPLE_BUFFER_LENGTH / 2 ));
}

/// @brief adds an ecg sample to ecg sample buffer
/// @param p_sample nonnull pointer to sample
[[gnu::nonnull]]
void log_ecg_push_sample(const EcgSample* p_sample) 
{
    // add sample to buffer
    uint16_t nv_w = s_ecg_buffer_write_cursor;
    s_ecg_sample_buffer[nv_w] = *p_sample;
    nv_w = (nv_w + 1) % ECG_SAMPLE_BUFFER_LENGTH;

    // check for overflow
    if (nv_w == s_ecg_buffer_read_cursor) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ECG, ERR_TYPE_DEFAULT, ERR_BUFFER_OVERFLOW), log_ecg_push_sample);
    } else {
        s_ecg_buffer_write_cursor = nv_w;
    }

    // check if ecg buffer needs to be serviced
    if (log_ecg_sample_buffer_is_half_full()) {
        HAL_PWR_DisableSleepOnExit();
    }
}


/// @brief Perform non-time critical logging tasks.
/// @param  
void log_ecg_task(void) {
    while(s_ecg_buffer_read_cursor != s_ecg_buffer_write_cursor) {
        // encode sample
        uint8_t csv_encode_buffer[512];
        const EcgSample *p_sample = &s_ecg_sample_buffer[s_ecg_buffer_read_cursor];
        size_t encoded_bytes = priv__sample_to_csv(p_sample, csv_encode_buffer, sizeof(csv_encode_buffer));

        // write sample
        UINT write_result = buffer_writer_write(&s_bw, csv_encode_buffer, encoded_bytes);
        if (FX_SUCCESS != write_result) {
            error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ECG, ERR_TYPE_FILEX, write_result), log_ecg_task);
        }

        // advance read head
        s_ecg_buffer_read_cursor =  (s_ecg_buffer_read_cursor + 1)  % ECG_SAMPLE_BUFFER_LENGTH;
    }
}

/// @brief deinitilizes ecg logging
/// @param  
void log_ecg_deinit(void) {
    // flush any partial buffers
    log_ecg_task();

    // close file 
    buffer_writer_close(&s_bw);
}