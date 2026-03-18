
#include "util/buffer_writer.h"
/*
    @note imu size buffered as a timestamp, and a measurement packet.
*/

#define IMU_FORMAT_SEPERATE_CSV 0
#define IMU_FORMAT_UNIFIED_CSV 1

#define IMU_FORMAT IMU_FORMAT_SEPERATE_CSV

#include <sh2_SensorValue.h>
#include <app_filex.h> // filex

extern FX_MEDIA sdio_disk;

#define IMU_BUFFER_LEN_S 8
#define IMU_PACKET_BUFFER_LEN 8*50
static volatile uint16_t s_pkt_buffer_w_cursor = 0;
static uint16_t s_pkt_buffer_r_cursor = 0;

BufferWriter s_bw = {
};


sh2_SensorValue_t s_accel_sample_buffer[50*IMU_BUFFER_LEN_S];
sh2_SensorValue_t s_gyro_sample_buffer[50*IMU_BUFFER_LEN_S];
sh2_SensorValue_t s_mag_sample_buffer[50*IMU_BUFFER_LEN_S];
sh2_SensorValue_t s_rotation_sample_buffer[20*IMU_BUFFER_LEN_S];

/********* CSV ***************************************************************/
// static void __create_csv_files(const char * filename) {
//     /* Create/open pressure file */
//     UINT fx_create_result = fx_file_create(&sdio_disk, filename);
//     if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
//         #warning "ToDo: Handle Error creating file"
//         // Error_Handler();
//     }

//     /* open file */
//     UINT fx_open_result = buffer_writer_open(&s_bw, filename);
//     if (FX_SUCCESS != fx_open_result) {
//         #warning "ToDo: Handle Error opening file"
//         return;
//     }

//     /* file was newly created. Initialize header */
//     if (FX_ALREADY_CREATED != fx_create_result) {
//         CETI_LOG("Created new pressure file \"%s\"", filename);
//         buffer_writer_write(&s_bw, (uint8_t *)log_pressure_csv_header, strlen(log_pressure_csv_header));
//         buffer_writer_flush(&s_bw);
//     }
// }

/********* Sample Buffer *****************************************************/
void log_imu_init(void) {
    // __create_imu_files("imu.csv");
}

/// @brief 
/// @param  
void log_imu_task(void) {
}
