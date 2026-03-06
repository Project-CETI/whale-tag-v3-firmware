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

#include <stdint.h>
#include <stdio.h>

#include <app_filex.h>


#define PRESSURE_FORMAT_BIN (0)    
#define PRESSURE_FORMAT_CSV (1)

#define PRESSURE_FORMAT PRESSURE_FORMAT_CSV

#define PRESSURE_FILENAME "data_pressure.csv"

extern FX_MEDIA sdio_disk;
FX_FILE pressure_file = {};

/********* Encoded Buffer *****************************************************/
#define LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD (512)
#define LOG_PRESSURE_ENCODE_BUFFER_SIZE (LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)

static size_t s_encoded_bytes = 0;
uint8_t log_pressure_encode_buffer[LOG_PRESSURE_ENCODE_BUFFER_SIZE];

static void __encoded_buffer_flush(void) {
    if (0 == s_encoded_bytes) {
        return;
    }
    
    UINT fx_result = fx_file_write(&pressure_file, log_pressure_encode_buffer, s_encoded_bytes);
    if (fx_result != FX_SUCCESS) {
        Error_Handler();
    }
    s_encoded_bytes = 0;
}

static void __encoded_buffer_write(uint8_t* p_bytes, size_t len) {
    memcpy(&log_pressure_encode_buffer[s_encoded_bytes], p_bytes, len);
    s_encoded_bytes += len;

    // flush encode buffer if it is almost full
    if (s_encoded_bytes > LOG_PRESSURE_ENCODE_BUFFER_FLUSH_THRESHOLD) {
        __encoded_buffer_flush();
    }
}

static void __encoded_buffer_close() {
    __encoded_buffer_flush();
    fx_file_close(&pressure_file);
}

/********* CSV *****************************************************/
const char *log_pressure_csv_header =
    "Timestamp [us]"
    ", Notes"
    ", Pressure [bar]"
    ", Temperature [C]"
    "\n"
;

/// @brief creates a .csv file and a writes pressure csv header to file if file
/// was newly created
/// @param  
static void __create_csv_file(void) {
    /* Create/open presure file */
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, PRESSURE_FILENAME);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
         Error_Handler();
    }
    UINT fx_open_result = fx_file_open(&sdio_disk, &pressure_file, PRESSURE_FILENAME, FX_OPEN_FOR_WRITE);
    if ((fx_open_result != FX_ALREADY_CREATED)) {
        CETI_LOG("Created new pressure file \"%s\"", PRESSURE_FILENAME);
        __encoded_buffer_write((uint8_t *)log_pressure_csv_header, strlen(log_pressure_csv_header));
        fx_media_flush(&sdio_disk);
    }
}

/// @brief Converts a single pressure sample into a CSV line stored at `p_buffer`
/// @param p_sample pointer to immutable input sample to be converted
/// @param p_buffer pointer to output buffer
/// @param buffer_len length in bytes of buffer pointed to by `p_buffer`
/// @return number of bytes written to buffer
static size_t __pressure_sample_to_csv_line(const CetiPressureSample* p_sample, uint8_t * p_buffer, size_t buffer_len) {  
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
static CetiPressureSample pressure_sample_buffer[PRESSURE_BUFFER_SIZE]; //minute long buffer
static volatile uint16_t s_pressure_sample_buffer_write_position = 0;
static uint16_t s_pressure_sample_buffer_read_position = 0;

/// @brief adds a pressure sample to the sample buffer
/// @param p_sample pointer to input sample
/// @note
void log_pressure_buffer_sample(const CetiPressureSample *p_sample) {
    uint16_t nv_write_position = s_pressure_sample_buffer_write_position;
    memcpy(&pressure_sample_buffer[nv_write_position], p_sample, sizeof(CetiPressureSample));
    nv_write_position = (nv_write_position + 1) % PRESSURE_BUFFER_SIZE;
    if (nv_write_position == s_pressure_sample_buffer_read_position) {
        // ToDo: Handle overflow
    }
    s_pressure_sample_buffer_write_position = nv_write_position;
}

/// @brief initialize pressure logging subsystem
/// @param  
void log_pressure_init(void) {
    // create pressure file
#if PRESSURE_FORMAT == PRESSURE_FORMAT_CSV
    __create_csv_file();
#else
    #error Unknown pressure format specified
#endif
}

/// @brief preiodically call this function to ensure pressure data logging
/// occurs. Contains non-time critical processes
/// @param  
void log_pressure_task(void) {
    //process any new samples in the sample buffer
    while(s_pressure_sample_buffer_read_position != s_pressure_sample_buffer_write_position) {
        const CetiPressureSample* p_sample = &pressure_sample_buffer[s_pressure_sample_buffer_read_position];
        uint8_t buffer[256];
        size_t len;
        len = __pressure_sample_to_csv_line(p_sample, buffer, sizeof(buffer));
        __encoded_buffer_write(buffer, len);
        s_pressure_sample_buffer_read_position = (s_pressure_sample_buffer_read_position + 1) % PRESSURE_BUFFER_SIZE;
    }
}

/// @brief Deinitializes pressure logging infastructure 
/// @param  
void log_pressure_deinit(void) {
    log_pressure_task(); // ensure all samples are flushed
    __encoded_buffer_close(); // ensure encoded samples get written to file
    s_pressure_sample_buffer_write_position = 0;
    s_pressure_sample_buffer_read_position = 0;
}
