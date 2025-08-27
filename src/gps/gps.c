#include "gps.h"

#include "main.h"

#include "log/log_syslog.h"

#include "usart.h"
#include <app_filex.h>


#define NMEA_MAX_SIZE (82)

typedef enum {
    ACQ_GPS_PROTOCOL_UART,
    ACQ_GPS_PROTOCOL_I2C
} GpsProtocol;

#define ACQ_GPS_PROTOCOL ACQ_GPS_PROTOCOL_UART
#define LOG_GPS_FILENAME "data_gps.txt"

extern UART_HandleTypeDef GPS_huart;
extern FX_MEDIA sdio_disk;


static FX_FILE s_log_gps_file = {};

uint8_t max10s_rx_buffer[2*16*NMEA_MAX_SIZE];
uint8_t s_write_buffer_index = 0;
uint8_t s_read_buffer_index = 0;


static uint8_t s_valid_datetime = 0;
static uint8_t s_valid_position = 0;
static struct minmea_date s_latest_date = {};
static struct minmea_time s_latest_time = {};
static struct minmea_float s_latest_latitude = {};
static struct minmea_float s_latest_longitude = {};


void gps_RxCpltCallback(UART_HandleTypeDef *huart) {
    uint8_t nv_w_indx = s_write_buffer_index ^ 1;
    s_write_buffer_index = nv_w_indx;
    if(nv_w_indx == s_read_buffer_index) {
        // ToDo: handle overflow
    }
}

static void __gps_hw_init(void) {
    /* configure gpios */
    HAL_GPIO_WritePin(GPIOE, GPS_PWR_EN_GPIO_Output_Pin | GPS_NRST_GPIO_Output_Pin, GPIO_PIN_RESET);

    /* setup UART*/
    MX_USART1_UART_Init();

    // start GPS
    HAL_UART_RegisterCallback(&GPS_huart, HAL_UART_RX_COMPLETE_CB_ID, gps_RxCpltCallback);
	HAL_UART_RegisterCallback(&GPS_huart, HAL_UART_RX_HALFCOMPLETE_CB_ID, gps_RxCpltCallback);
    HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);

    /* ToDo: reconfigure baudrate */
   
    /* ToDo: reconfigure message filters */

}

static void __gps_log_init(void) {
    UINT fx_result = FX_ACCESS_ERROR;
    fx_result = fx_file_create(&sdio_disk, LOG_GPS_FILENAME);
    if ((fx_result != FX_SUCCESS) && (fx_result != FX_ALREADY_CREATED)) {
        // ToDo: Error handling
    }

    fx_result = fx_file_open(&sdio_disk, &s_log_gps_file, LOG_GPS_FILENAME, FX_OPEN_FOR_WRITE);
    if (fx_result != FX_SUCCESS) {
        // Error_Handler();
    }

    CETI_LOG("Opened GPS file \"%s\"", LOG_GPS_FILENAME);
}

void gps_init(void) {
    __gps_hw_init();
    __gps_log_init();
}

void gps_start(void) {
    // initialize DMA
    HAL_UART_Receive_DMA(&GPS_huart, &max10s_rx_buffer[0], sizeof(max10s_rx_buffer));

    // take chip out of reset
    HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
}

void gps_log_task(void) {
    // write raw buffer out to SD card
    uint8_t nv_r_index = s_read_buffer_index;
    while(nv_r_index != s_write_buffer_index) {
        UINT fx_result = fx_file_write(&s_log_gps_file, &max10s_rx_buffer[nv_r_index * 16*NMEA_MAX_SIZE], 16*NMEA_MAX_SIZE);
        if (fx_result != FX_SUCCESS) {
            //  ToDo: Handle Errors
        }
        nv_r_index ^= 1;
        s_read_buffer_index = nv_r_index;
    }
}

void gps_parse_task(void) {
    int time_updated = 0;
    int position_updated = 0;
    uint8_t nv_w_indx = s_write_buffer_index;
    int start_offset = 16*NMEA_MAX_SIZE * (2 - nv_w_indx);
    int stop_offset = 16*NMEA_MAX_SIZE * (1 - nv_w_indx);
    int current_offset = start_offset;

    // iterate backward over rx buffer parsing nmea 
    // until: 1) position AND timestamp are found
    // OR 2) we hit the "end of the buffer"
    while ((!time_updated || !position_updated) && (1 /*at end of buffer*/)) {
        
        // find end of message
        do{
            current_offset--;
            if(current_offset < stop_offset) {
                return;
            }
        }while ((max10s_rx_buffer[current_offset] != '\n'));

        // find start of message
        do{
            current_offset--;
            if(current_offset < stop_offset) {
                return;
            }
        }while((max10s_rx_buffer[current_offset] != '$'));

        // parse message
        const uint8_t *raw_sentence = &max10s_rx_buffer[current_offset];
        uint8_t sentence_type = minmea_sentence_id((char *)raw_sentence, false);
        // check message type for position
        if (!position_updated) {
            switch(sentence_type) {
                case MINMEA_SENTENCE_RMC: {
                    struct minmea_sentence_rmc rmc_frame;
                    if (!minmea_parse_rmc(&rmc_frame, (char *)raw_sentence)) {
                        break;
                    }

                    if (!rmc_frame.valid) {
                        break;
                    }

                    //update lat and lon
                    s_latest_latitude = rmc_frame.latitude;
                    s_latest_longitude = rmc_frame.longitude;
                    position_updated = 1;
                    if (!time_updated) {
                        // update date and time
                        s_latest_date = rmc_frame.date;
                        s_latest_time = rmc_frame.time;
                        time_updated = 1;
                    }
                    break; // nothing more to do
                }
                case MINMEA_SENTENCE_GGA: {
                    struct minmea_sentence_gga gga_frame = {};
                    if (!minmea_parse_gga(&gga_frame, (char *)raw_sentence)) {
                        break;
                    }
                    s_latest_latitude = gga_frame.latitude;
                    s_latest_longitude = gga_frame.longitude;
                    position_updated = 1;
                    break;
                }
                case MINMEA_SENTENCE_GLL: {
                    struct minmea_sentence_gll gll_frame = {};
                    if (!minmea_parse_gll(&gll_frame, (char *)raw_sentence)) {
                        break;
                    }

                    if (gll_frame.status == MINMEA_GLL_STATUS_DATA_NOT_VALID) {
                        break;
                    }

                    s_latest_latitude = gll_frame.latitude;
                    s_latest_longitude = gll_frame.longitude;
                    position_updated = 1;
                    break;
                }
                default: {
                    break;
                }
            }
        } 
        
        if (!time_updated) {
            switch(sentence_type) {
                case MINMEA_SENTENCE_RMC: {
                     struct minmea_sentence_rmc rmc_frame;
                    if (!minmea_parse_rmc(&rmc_frame, (char *)raw_sentence)) {
                        break;
                    }

                    if (!rmc_frame.valid) {
                        break;
                    }

                    // update date and time
                    s_latest_date = rmc_frame.date;
                    s_latest_time = rmc_frame.time;
                    time_updated = 1;
                    break;
                }
                case MINMEA_SENTENCE_ZDA : {
                    struct minmea_sentence_zda zda_frame = {};
                    if (!minmea_parse_zda(&zda_frame, (char *)raw_sentence)) {
                        break;
                    }
                                        // update date and time
                    s_latest_date = zda_frame.date;
                    s_latest_time = zda_frame.time;
                    time_updated = 1;
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }
}

void gps_task(void) {
    gps_log_task();
    gps_parse_task();
}

int gps_get_latest_timestamp(struct minmea_date *pDate, struct minmea_time *pTime) {
    if (!s_valid_datetime) {
        return -1;
    }
    if(pDate != NULL) {
        *pDate = s_latest_date;
    }
    if(pTime != NULL) {
        *pTime = s_latest_time;
    }
    return 0;
}

int gps_get_latest_position(struct minmea_float *pLatitude, struct minmea_float *pLongitude) {
    if(!s_valid_position) {
        return -1;
    }
    if(pLatitude != NULL) {
        *pLatitude = s_latest_latitude;
    }
    if(pLongitude != NULL) {
        *pLatitude = s_latest_longitude;
    }
    return 0;
}

// ToDo: gps_sleep