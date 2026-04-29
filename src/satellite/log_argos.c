/*****************************************************************************
 *   @file      log_argos.c
 *   @brief     code for saving argos tranmission to csv
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *   @date      4/20/2026
 *****************************************************************************/
#include "log_argos.h"

#include "error.h"
#include "metadata.h"
#include "satellite.h"

#ifndef UNIT_TEST
#include <app_filex.h>
#endif

#include <stdint.h>
#include <stdio.h>


/* External Variables */
extern FX_MEDIA sdio_disk;

/* MACRO Definitions */
#define ARGOS_TX_LOG_FILENAME "tag_rf.csv"
#define ARGOS_TX_LOG_CSV_VERSION 0

/* Private Variables */
char *s_argos_tx_log_csv_header = 
    "# version: 0\n"
    "Timestamp [us]"
    ", Notes"
    ", Type"
    ", Message"
    "\n";

static FX_FILE s_file = {};


/* Private Functions */
static void priv__create_csv_file(char * filename) {
    /* Create/open file */
    UINT fx_create_result = fx_file_create(&sdio_disk, filename);
    if ((fx_create_result != FX_SUCCESS) && (fx_create_result != FX_ALREADY_CREATED)) {
        error_queue_push(
            CETI_ERROR(ERR_SUBSYS_LOG_ARGOS, ERR_TYPE_FILEX, fx_create_result), 
            priv__create_csv_file
        );
        return;
    }

    /* open file */
    UINT fx_open_result = fx_file_open(&sdio_disk, &s_file, filename, FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS != fx_open_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ARGOS, ERR_TYPE_FILEX, fx_create_result), priv__create_csv_file);
        return;
    }

    metadata_log_file_creation(filename, DATA_TYPE_ARGOS, DATA_FORMAT_CSV, ARGOS_TX_LOG_CSV_VERSION);

    /* file was newly created. Initialize header */
    if (FX_ALREADY_CREATED != fx_create_result) {
        [[maybe_unused]] UINT fx_write_result = fx_file_write(&s_file, s_argos_tx_log_csv_header, strlen(s_argos_tx_log_csv_header));
        [[maybe_unused]] UINT fx_flush_result = fx_media_flush(&sdio_disk);
    }
}

static uint16_t priv__event_to_csv_line(ArgosTxEvent event, uint8_t *buffer, uint16_t capacity) {
    uint8_t offset = 0;
    //Timestamp [uS]
    offset += snprintf((char *)&buffer[offset], capacity - offset, "%lld", event.timestamp_us);
    
    //", Notes"
    offset += snprintf((char *)&buffer[offset], capacity - offset, ", ");

    //", Type"
    const char * argos_type_string[] = {
        [ARGOS_MOD_LDA2] = "LDA2",
        [ARGOS_MOD_VLDA4] = "VLDA2",
        [ARGOS_MOD_LDK] = "LDK",
        [ARGOS_MOD_LDA2L] = "LDA2L",
    };
    offset += snprintf((char *)&buffer[offset], capacity - offset, ", %s", argos_type_string[event.tx_type]);

    //", Message"
    offset += snprintf((char *)&buffer[offset], capacity - offset, ", %s", event.message);

    offset += snprintf((char *)&buffer[offset], capacity - offset, "\n");
    return offset;
} 

/* Functions */
void log_argos_init() {
    // create CSV
    priv__create_csv_file(ARGOS_TX_LOG_FILENAME);
}

void log_argos_deinit() {
    fx_file_close(&s_file);
}

void log_argos_event(ArgosTxEvent event) {
    uint8_t csv_line_buffer[256];
    uint16_t line_length = priv__event_to_csv_line(event, csv_line_buffer, sizeof(csv_line_buffer) - 1);
    UINT fx_write_result = fx_file_write(&s_file, csv_line_buffer, line_length);
    if (FX_SUCCESS != fx_write_result) {
        error_queue_push(CETI_ERROR(ERR_SUBSYS_LOG_ARGOS, ERR_TYPE_FILEX, fx_write_result), log_argos_event);
    }
}