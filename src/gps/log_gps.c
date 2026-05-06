/*****************************************************************************
 *   @file      log_gps.c
 *   @brief     GPS Logging
 *   @project   Project CETI
 *   @date      04/13/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#include "log_gps.h"

#include "gps/acq_gps.h"
#include "error.h"
#include "metadata.h"
#include "util/buffer_writer.h"

#include <app_filex.h>

#include <stdio.h>

extern FX_MEDIA sdio_disk;
static FX_FILE gps_log_file = {};

#define SENTENCE_BUFFER_LEN (32)
#define LOG_GPS_FILENAME "data_gps.csv"

static GpsSentence s_gps_sentence_buffer[SENTENCE_BUFFER_LEN];
static volatile uint8_t s_buffer_write_cursor = 0;
static uint8_t s_buffer_read_cursor = 0;

/********* Buffered File *****************************************************/
#define LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD (512)
#define LOG_GPS_ENCODE_BUFFER_SIZE (LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t s_csv_encode_buffer[LOG_GPS_ENCODE_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = s_csv_encode_buffer,
    .threshold = LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD,
    .capacity = LOG_GPS_ENCODE_BUFFER_SIZE,
};

/********* CSV ***************************************************************/
#define GPS_CSV_VERSION 0

static char *s_csv_header =
    "Timestamp [us]"
    "; Notes"
    "; NMEA Sentence"
    "\n";

/// @brief creates a .csv file and a writes pressure csv header to file if file
/// was newly created
/// @param
static void priv__create_csv_file(char *filename) {
    /* Create/open pressure file */
    UINT fx_create_result = fx_file_create(&sdio_disk, filename);
    if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
        error_queue_push(
            CETI_ERROR(ERR_SUBSYS_LOG_PRESSURE, ERR_TYPE_FILEX, fx_create_result),
            priv__create_csv_file);
        return;
    }

    /* open file */
    UINT fx_open_result = buffer_writer_open(&s_bw, filename);
    if (FX_SUCCESS != fx_open_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_PRESSURE, ERR_TYPE_FILEX, fx_create_result), priv__create_csv_file);
        return;
    }

    metadata_log_file_creation(filename, DATA_TYPE_PRESSURE, DATA_FORMAT_CSV, GPS_CSV_VERSION);

    /* file was newly created. Initialize header */
    if (FX_ALREADY_CREATED != fx_create_result) {
        buffer_writer_write(&s_bw, (uint8_t *)s_csv_header, strlen(s_csv_header));
        buffer_writer_flush(&s_bw);
    }
}

/// @brief Converts a single GPS sentence into a CSV line stored at `p_buffer`
/// @param p_sentence pointer to immutable input sample to be converted
/// @param p_buffer pointer to output buffer
/// @param buffer_len length in bytes of buffer pointed to by `p_buffer`
/// @return number of bytes written to buffer
static uint16_t priv__sentence_to_csv_line(const GpsSentence *p_sentence, uint8_t *p_buffer, uint16_t buffer_len) {
    uint8_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sentence->timestamp_us);

    /* ToDo: note conversion */
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "; ");
    // offset += snprintf(&p_buffer[offset], buffer_len - offset, ", %s", p_sample->error);

    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "; %s", p_sentence->msg);
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

/********* API ***************************************************************/
/// @brief initialize gps logging
/// @param
void log_gps_init(void) {
    // try to create file
    priv__create_csv_file(LOG_GPS_FILENAME);
}

/// @brief initialize gps logging
/// @param
void log_gps_deinit(void) {
    fx_file_close(&gps_log_file);
}

/// @brief checks if gps buffer is half full/needs servicing
/// @param
/// @return bool
int log_gps_buffer_is_half_full(void) {
    int8_t buffered_samples = ((int8_t)s_buffer_write_cursor - (int8_t)s_buffer_read_cursor);
    buffered_samples = (buffered_samples >= 0) ? buffered_samples : SENTENCE_BUFFER_LEN + buffered_samples;
    return (buffered_samples >= SENTENCE_BUFFER_LEN / 2);
}

/// @brief performs the task of logging gps coordinates to a file
/// @param
void log_gps_task(void) {
    while (s_buffer_read_cursor != s_buffer_write_cursor) {
        // convert
        uint8_t line_buffer[256];
        uint8_t line_length = priv__sentence_to_csv_line(&s_gps_sentence_buffer[s_buffer_read_cursor], line_buffer, sizeof(line_buffer));

        // write to file
        buffer_writer_write(&s_bw, line_buffer, line_length);

        s_buffer_read_cursor = (s_buffer_read_cursor + 1) % SENTENCE_BUFFER_LEN;
    }
}

void log_gps_push_sentence(const GpsSentence *p_sentence) {
    uint16_t nv_w = s_buffer_write_cursor;
    s_gps_sentence_buffer[nv_w] = *p_sentence;
    nv_w = (nv_w + 1) % SENTENCE_BUFFER_LEN;

    if (nv_w == s_buffer_read_cursor) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_DEFAULT, ERR_BUFFER_OVERFLOW), log_gps_push_sentence);
    } else {
        s_buffer_write_cursor = nv_w;
    }
}