
#include "log_gps.h"
#include "gps.h"

#include "error.h"
#include "syslog.h"
#include "util/buffer_writer.h"

#include <app_filex.h>
#include <stdio.h>

extern FX_MEDIA sdio_disk;

#if LOG_GPS_FORMAT == LOG_GPS_FORMAT_RAW
static FX_FILE s_gps_log_file = {};

static uint8_t gps_raw_buffer[2 * GPS_BULK_TRANSFER_SIZE];
static volatile uint16_t gps_raw_buffer_read_position = 0;
static volatile uint8_t gps_bulk_write_position = 0;
static uint8_t gps_bulk_read_position = 0;

/// @brief initialize gps logging
/// @param
void log_gps_init(void) {
    // try to create file
    CETI_LOG("Created new gps file \"gps.log\"");
    UINT fx_create_result = fx_file_create(&sdio_disk, "gps.log");
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_create_result));
    }
    UINT fx_result = fx_file_open(&sdio_disk, &s_gps_log_file, "gps.log", FX_OPEN_FOR_WRITE);
}

/// @brief initialize gps logging
/// @param
void log_gps_deinit(void) {
        fx_file_close(&s_gps_log_file);

}

/// @brief performs the task of logging gps coordinates to a file
/// @param
static void log_gps_task(void) {
    const uint8_t *gps_bulk_ptr = gps_pop_bulk_transfer();
    if (NULL == gps_bulk_ptr) {
        return;
    }

    UINT fx_result = fx_file_write(&s_gps_log_file, (char *)gps_bulk_ptr, GPS_BULK_TRANSFER_SIZE);
    if (FX_SUCCESS != fx_result) { // Catch error
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_result));
    }
}


int log_gps_task_call_required(void) {
    // check if buffer is half full
    int buffer_count = (int)gps_bulk_write_position - (int)gps_bulk_read_position;
    buffer_count = (buffer_count >= 0) ? buffer_count : 2*GPS_BULK_TRANSFER_SIZE + buffer_count;
    return (buffer_count >= (GPS_BULK_TRANSFER_SIZE/2));
}

void log_gps_message_complete_callback(const GpsSentence *p_sentence) {
    // nothing to do
}

void log_gps_raw_rx_complete(const uint8_t *data, size_t len) {
    size_t nv_w = gps_bulk_write_position;
    size_t remaining_capacity = sizeof(gps_raw_buffer) - nv_w;
    if (len  > remaining_capacity) {
        memcpy(gps_raw_buffer[nv_w], data, remaining_capacity);
        len -= remaining_capacity;
        data = &data[remaining_capacity];
        nv_w = 0;
    }
    
    memcpy(gps_raw_buffer[nv_w], data, len);
    size_t next_position = nv_w + len;
    static uint8_t gps_raw_buffer[2 * GPS_BULK_TRANSFER_SIZE];
    // ToDo: check for overflow
    gps_bulk_write_position = next_position;
}

#elif LOG_GPS_FORMAT == LOG_GPS_FORMAT_CSV

/********* Buffered File *****************************************************/
#define LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD (3 * 1024)
#define LOG_GPS_ENCODE_BUFFER_SIZE (LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD + 512)
static uint8_t log_gps_encode_buffer[LOG_GPS_ENCODE_BUFFER_SIZE];
static BufferWriter s_bw = {
    .buffer = log_gps_encode_buffer,
    .threshold = LOG_GPS_ENCODE_BUFFER_FLUSH_THRESHOLD,
    .capacity = LOG_GPS_ENCODE_BUFFER_SIZE,
};

/********* CSV ***************************************************************/


#define GPS_BUFFER_COUNT (1 + sizeof(gps_raw_buffer) / NMEA_MAX_SIZE)

static FX_FILE s_gps_log_file = {};

static char *log_gps_csv_header = 
    "Timestamp [us]"
    ", Notes"
    ", GPS"
    "\n";

static GpsSentence gps_buffer[GPS_BUFFER_COUNT] 
static volatile uint8_t gpsBuffer_write_index = 0;
static uint8_t gpsBuffer_read_index = 0;

/// @brief initialize gps csv
/// @param
void log_gps_init(void) {
    // try to create file
    CETI_LOG("Created new gps file \"gps.csv\"");
    UINT fx_create_result = fx_file_create(&sdio_disk, "gps.csv");
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_create_result));
        return;
    }

    // open file
    UINT fx_open_result = buffer_writer_open(&s_bw, &s_gps_log_file, "gps.csv");
    if (FX_SUCCESS != fx_open_result) {
        CETI_ERR("Failed to open gps.csv");
        return;
    }

    //write header
    if (FX_ALREADY_CREATED != fx_create_result) {
        CETI_LOG("Created new gps csv \"gps.csv\"");
        buffer_writer_write(&s_bw, (uint8_t 8)log_gps_csv_header, strlen(log_gps_csv_header));
        buffer_writer_flush(&s_bw);
    }
}

void log_gps_deinit(void) {
    // ToDo: flush gps buffer
    log_gps_task();

    // close file
    buffer_writer_close(&s_bw);
}

static uint16_t __gps_to_csv(const GpsSentence *sentence, uint8_t *buffer, uint16_t buffer_capacity) {
    uint16_t offest = 0;
    offset += snprintf((char *)&buffer[offset], buffer_capacity - offset, "%lld", sentence->timestamp_us);
    
    /* ToDo: note conversion */
    offset += snprintf((char *)&buffer[offset], buffer_capacity - offset, ", ");

    offset += snprintf((char *)&buffer[offset], buffer_capacity - offset, ", \"");
    memcpy(&buffer[offset], sentence->msg, sentence->len);
    offset += sentence->len;
    offset += snprintf((char *)&buffer[offset], buffer_capacity - offset, "\"\n");

    return offset;
}

int log_gps_task_call_required(void) {
    // check if buffer is half full
    int buffer_count = (int)gpsBuffer_write_index - (int)gpsBuffer_read_index;
    buffer_count = (buffer_count >= 0) ? buffer_count : GPS_BUFFER_COUNT + buffer_count;
    return (buffer_count >= (GPS_BUFFER_COUNT/2));
}


void log_gps_task(void) {
    while (gpsBuffer_read_index != gpsBuffer_write_index) {
        //convert sample to csv line
        uint8_t csv_buffer[256];
        uint16_t csv_line_len = __gps_to_csv(&gps_buffer[gpsBuffer_read_index], csv_buffer, sizeof(csv_buffer));

        //save sample to file
        UINT write_result = buffer_writer_write(&s_bw, csv_encode_buffer, encoded_bytes);
        if (FX_SUCCESS != write_result) {
            //ToDo: Handle write error
        }

        // advance read head
        gpsBuffer_read_index = (1 + gpsBuffer_read_index) % GPS_BUFFER_COUNT;
    }
}

void log_gps_message_complete_callback(const GpsSentence *p_sentence) {
    uint8_t nv_w = gpsBuffer_write_index;
    gps_buffer[nv_w] = *p_sentence; // memcpy structure
    nv_w = (nv_w + 1) % GPS_BUFFER_COUNT;
    if (nv_w == gpsBuffer_read_index) {
        // ToDo: report overflow error
    } else {
        gpsBuffer_write_index = nv_w;
        //ToDo: wake tag is buffer is half full 
    }
}

// nothing to do uses log
void log_gps_raw_rx_complete(const uint8_t *data, size_t len) {
}

#else
#error Unknown LOG_GPS_FORMAT selected!!!!
#endif