#include <app_filex.h>

// static FX_FILE gps_log_file = {};

// static

// // /// @brief initialize gps logging
// // /// @param
// static void log_gps_init(void) {
//     // try to create file
//     CETI_LOG("Created new gps file \"gps.log\"");
//     UINT fx_create_result = fx_file_create(&sdio_disk, "gps.log");
//     if ((FX_SUCCESS != fx_create_result) && (FX_ALREADY_CREATED != fx_create_result)) {
//         error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_GPS, ERR_TYPE_FILEX, fx_create_result));
//     }
//     UINT fx_result = fx_file_open(&sdio_disk, &gps_log_file, "gps.log", FX_OPEN_FOR_WRITE);
// }

// // /// @brief initialize gps logging
// // /// @param
// static void log_gps_deinit(void) {
//     fx_file_close(&gps_log_file);
// }

// // /// @brief performs the task of logging gps coordinates to a file
// // /// @param
// static void log_gps_task(void) {
//     const uint8_t *gps_bulk_ptr = gps_pop_bulk_transfer();
//     if (NULL == gps_bulk_ptr) {
//         return;
//     }

//     UINT fx_result = fx_file_write(&gps_log_file, (char *)gps_bulk_ptr, GPS_BULK_TRANSFER_SIZE);
//     if (FX_SUCCESS != fx_result) { // Catch error
//         error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_MISSION, ERR_TYPE_FILEX, fx_result));
//     }
// }