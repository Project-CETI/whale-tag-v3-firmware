/*****************************************************************************
 *   @file      log_pressure.c
 *   @brief     pressure processing and storing code.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "log_pressure.h"
#include "acq_pressure.h"

#include "syslog.h"
#include "util/buffer_writer.h"

#include <stdint.h>
#include <stdio.h>

#include <app_filex.h>

extern FX_MEDIA sdio_disk;

/********* Buffered File *****************************************************/
#define LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD (512)
#define LOG_PRESSURE_ENCODE_BUFFER_SIZE (LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t log_pressure_encode_buffer[LOG_PRESSURE_ENCODE_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = log_pressure_encode_buffer,
    .threshold = LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD,
    .capacity = LOG_PRESSURE_ENCODE_BUFFER_SIZE,
};

/********* CSV ***************************************************************/

#define PRESSURE_FILENAME "data_pressure.csv"

char *log_pressure_csv_header =
    "Timestamp [us]"
    ", Notes"
    ", Pressure [bar]"
    ", Temperature [C]"
    "\n";

/// @brief creates a .csv file and a writes pressure csv header to file if file
/// was newly created
/// @param
static void __create_csv_file(char *filename) {
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
        CETI_LOG("Created new pressure file \"%s\"", filename);
        buffer_writer_write(&s_bw, (uint8_t *)log_pressure_csv_header, strlen(log_pressure_csv_header));
        buffer_writer_flush(&s_bw);
    }
}

/// @brief Converts a single pressure sample into a CSV line stored at `p_buffer`
/// @param p_sample pointer to immutable input sample to be converted
/// @param p_buffer pointer to output buffer
/// @param buffer_len length in bytes of buffer pointed to by `p_buffer`
/// @return number of bytes written to buffer
static size_t __pressure_sample_to_csv_line(const CetiPressureSample *p_sample, uint8_t *p_buffer, size_t buffer_len) {
    uint8_t offset = 0;
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "%lld", p_sample->timestamp_us);

    /* ToDo: note conversion */
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", ");
    // offset += snprintf(&p_buffer[offset], buffer_len - offset, ", %s", p_sample->error);

    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %.3f", acq_pressure_raw_to_pressure_bar(p_sample->data.pressure));
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, ", %.3f", acq_pressure_raw_to_temperature_c(p_sample->data.temperature));
    offset += snprintf((char *)&p_buffer[offset], buffer_len - offset, "\n");
    return offset;
}

/********* Sample Buffer *****************************************************/
#define PRESSURE_BUFFER_SIZE (8)
static CetiPressureSample pressure_sample_buffer[PRESSURE_BUFFER_SIZE]; // minute long buffer
static volatile uint16_t s_pressure_sample_buffer_write_position = 0;
static uint16_t s_pressure_sample_buffer_read_position = 0;

/// @brief adds a pressure sample to the sample buffer
/// @param p_sample pointer to input sample
/// @note
__attribute__((no_instrument_function))
void log_pressure_buffer_sample(const CetiPressureSample *p_sample) {
    uint16_t nv_write_position = s_pressure_sample_buffer_write_position;
    memcpy(&pressure_sample_buffer[nv_write_position], p_sample, sizeof(CetiPressureSample));
    nv_write_position = (nv_write_position + 1) % PRESSURE_BUFFER_SIZE;
    if (nv_write_position == s_pressure_sample_buffer_read_position) {
        // ToDo: Handle overflow
    }
    s_pressure_sample_buffer_write_position = nv_write_position;
    if (log_pressure_sample_buffer_is_half_full()) {
        HAL_PWR_DisableSleepOnExit();
    }
}

/// @brief initialize pressure logging subsystem
/// @param
void log_pressure_init(void) {
    // create pressure file
    __create_csv_file(PRESSURE_FILENAME);
}

/// @brief preiodically call this function to ensure pressure data logging
/// occurs. Contains non-time critical processes
/// @param
void log_pressure_task(void) {
    // process any new samples in the sample buffer
    while (s_pressure_sample_buffer_read_position != s_pressure_sample_buffer_write_position) {
        const CetiPressureSample *p_sample = &pressure_sample_buffer[s_pressure_sample_buffer_read_position];
        uint8_t buffer[256];
        size_t len;
        len = __pressure_sample_to_csv_line(p_sample, buffer, sizeof(buffer));
        buffer_writer_write(&s_bw, buffer, len);
        s_pressure_sample_buffer_read_position = (s_pressure_sample_buffer_read_position + 1) % PRESSURE_BUFFER_SIZE;
    }
}

/// @brief Deinitializes pressure logging infastructure
/// @param
void log_pressure_deinit(void) {
    log_pressure_task();        // ensure all samples are flushed
    buffer_writer_close(&s_bw); // ensure encoded samples get written to file
    s_pressure_sample_buffer_write_position = 0;
    s_pressure_sample_buffer_read_position = 0;
}

/// @brief returns if the sample buffer is atleast half full.
/// @param  
/// @return bool
int log_pressure_sample_buffer_is_half_full(void) {
    int8_t buffered_samples = ((int8_t)s_pressure_sample_buffer_write_position - (int8_t)s_pressure_sample_buffer_read_position);
    buffered_samples = (buffered_samples >= 0) ? buffered_samples :  PRESSURE_BUFFER_SIZE + buffered_samples;
    return (buffered_samples >= PRESSURE_BUFFER_SIZE/2);
}