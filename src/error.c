//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "error.h"

#include <fx_api.h>
#include <time.h>

extern FX_MEDIA sdio_disk;

#define FILE_FORMAT_ZIP 0
#define FILE_FORMAT_BIN 1
#define FILE_FORMAT_CSV 2

#define ERROR_FILE_FORMAT FILE_FORMAT_BIN

#define ERROR_QUEUE_SIZE (1024/sizeof(ErrorQueueElement))
#if ERROR_FILE_FORMAT == FILE_FORMAT_BIN
    #define ERROR_FILE_FILENAME "errors.bin"
    #define ERROR_QUEUE_ELEMENT_HEADER {'C','E','T','I'}
#else
    #error "Unsupported error file format"
#endif

typedef struct {
    uint8_t frame_header[4];
    CetiStatus error;
    time_t timestamp;
} ErrorQueueElement;

static volatile size_t s_error_write_position = 0;
static volatile size_t s_error_read_position = 0;
ErrorQueueElement s_error_queue[ERROR_QUEUE_SIZE] = {};
static FX_FILE s_error_queue_file = {};
time_t s_error_queue_overflow_timestamp = 0;

/// @brief initializes the error queue
/// @param  
/// @return `0` = success;
///         `-1` = failure;
int error_queue_init(void) {
    // open error file
    UINT fx_create_result = fx_file_create(&sdio_disk, ERROR_FILE_FILENAME);
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        return -1;
    }

    UINT fx_open_result = fx_file_open(&sdio_disk, &s_error_queue_file, ERROR_FILE_FILENAME, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS != fx_open_result) {
        return;
    }

    return 0;
}

/// @brief adds a timestamped error to the error queue to be logged
/// @param error `CetiStatus` - error code to log
void error_queue_push(CetiStatus error) {
    size_t nv_w = s_error_write_position;
    size_t next_w = (nv_w + 1) % 8;
    if (next_w == s_error_read_position) {
        // overflow trying to report error
        s_error_queue_overflow_timestamp = 1;
    } else {
        s_error_queue[nv_w] = (ErrorQueueElement){
            .frame_header = ERROR_QUEUE_ELEMENT_HEADER,
            .error = error,
            .timestamp = 1,
        };
        s_error_write_position = next_w;
    }
}

int error_queue_is_empty(void) {
    return  (s_error_read_position == s_error_write_position);
}

/// @brief writes out any remaining errors in the error queue to the SD card
/// @param  
void error_queue_flush(void) {
    if (error_queue_is_empty()) {
        return;
    }
        
    // write out error queue
    size_t nv_r = s_error_read_position;
    size_t nv_w = s_error_write_position;
    if (nv_r > nv_w) {
#if ERROR_FILE_FORMAT == FILE_FORMAT_BIN
        size_t valid_len = sizeof(s_error_queue) - nv_r*sizeof(ErrorQueueElement);
        fx_file_write(&s_error_queue_file, &s_error_queue[nv_r], valid_len);
        s_error_read_position = 0;
        nv_r = 0;
#endif
    }
    if (nv_r < nv_w) {
#if ERROR_FILE_FORMAT == FILE_FORMAT_BIN
        size_t valid_len = (nv_w - nv_r)*sizeof(ErrorQueueElement);
        fx_file_write(&s_error_queue_file, &s_error_queue[nv_r], valid_len);
        s_error_read_position = nv_w;
#endif
    } 

    // check if error queue overflow occured
    if (0 != s_error_queue_overflow_timestamp) {
#if ERROR_FILE_FORMAT == FILE_FORMAT_BIN
        ErrorQueueElement overflow_error = {
            .frame_header = ERROR_QUEUE_ELEMENT_HEADER,
            .timestamp = s_error_queue_overflow_timestamp,
            .error = CETI_ERROR(ERR_SUBSYS_ERROR_QUEUE, ERR_TYPE_DEFAULT, ERR_BUFFER_OVERFLOW),
        };
        fx_file_write(&s_error_queue_file, &overflow_error, sizeof(ErrorQueueElement));
#endif
        s_error_queue_overflow_timestamp = 0;
    }
}

/// @brief call this function periodically in your main loop.
/// @param 
void error_queue_task(void){
    static size_t r_half = 0;
    size_t w_half = 2*s_error_write_position/ERROR_QUEUE_SIZE;
    if (r_half != w_half) {
        error_queue_flush();
        r_half = 2*s_error_read_position/ERROR_QUEUE_SIZE;
    }
}

/// @brief flushes and closes the error queue
/// @param  
void error_queue_close(void) {
    error_queue_flush();
    fx_file_close(&s_error_queue_file);
}
