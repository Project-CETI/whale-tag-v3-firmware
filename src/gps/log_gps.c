
#include <app_filex.h>

#include "gps/acq_gps.h"
#include "error.h"
#include "metadata.h"

extern FX_MEDIA sdio_disk;
static FX_FILE gps_log_file = {};

/// @brief initialize gps logging
/// @param
void log_gps_init(void) {
    // try to create file
    UINT fx_create_result = fx_file_create(&sdio_disk, "data_gps.log");
    if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_create_result), log_gps_init);
    }
    UINT fx_result = fx_file_open(&sdio_disk, &gps_log_file, "data_gps.log", FX_OPEN_FOR_WRITE);
    metadata_log_file_creation("data_gps.log", DATA_TYPE_GPS, DATA_FORMAT_TXT, 0);
}

/// @brief initialize gps logging
/// @param
void log_gps_deinit(void) {
    fx_file_close(&gps_log_file);
}

/// @brief performs the task of logging gps coordinates to a file
/// @param
void log_gps_task(void) {
    const uint8_t *gps_bulk_ptr = gps_pop_bulk_transfer();
    if (NULL == gps_bulk_ptr) {
        return;
    }

    UINT fx_result = fx_file_write(&gps_log_file, (char *)gps_bulk_ptr, GPS_BULK_TRANSFER_SIZE);
    if (FX_SUCCESS != fx_result) { // Catch error
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_result), log_gps_task);
    }
}