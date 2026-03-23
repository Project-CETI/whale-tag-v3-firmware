//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
// USB Mass Storage Class — exposes SD card as a single LUN via DMA.
// FileX must be closed before USB MSC is started (see main.c).
// DMA completion flags (sd_rx_cplt / sd_tx_cplt) and HAL_SD callbacks are
//-----------------------------------------------------------------------------

#include "app_filex.h"
#include "stm32u5xx_hal.h"
#include "tusb.h"
#include "tusb_config.h"
#include <sdmmc.h>
#include <string.h>

#if CFG_TUD_MSC

#define SD_TIMEOUT_MS 1000
#define SD_BLOCK_SIZE 512

// DMA completion flags (set by HAL_SD_RxCpltCallback / HAL_SD_TxCpltCallback
// in fx_stm32_sd_driver_glue.c)
extern __IO UINT sd_rx_cplt;
extern __IO UINT sd_tx_cplt;

//--------------------------------------------------------------------+
// USB Serial Number from STM32 Unique ID
//--------------------------------------------------------------------+
static size_t board_get_unique_id(uint8_t id[], size_t max_len) {
    (void)max_len;
    volatile uint32_t *stm32_uuid = (volatile uint32_t *)UID_BASE;
    uint32_t *id32 = (uint32_t *)(uintptr_t)id;
    uint8_t const len = 12;

    id32[0] = stm32_uuid[0];
    id32[1] = stm32_uuid[1];
    id32[2] = stm32_uuid[2];

    return len;
}

static inline size_t board_usb_get_serial(uint16_t desc_str1[],
                                          size_t max_chars) {
    uint8_t uid[16] TU_ATTR_ALIGNED(4);
    size_t uid_len;

    uid_len = board_get_unique_id(uid, sizeof(uid));

    if (uid_len > max_chars / 2)
        uid_len = max_chars / 2;

    for (size_t i = 0; i < uid_len; i++) {
        for (size_t j = 0; j < 2; j++) {
            const char nibble_to_hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
            uint8_t const nibble = (uid[i] >> (j * 4)) & 0xf;
            desc_str1[i * 2 + (1 - j)] = nibble_to_hex[nibble]; // UTF-16-LE
        }
    }

    return 2 * uid_len;
}

//--------------------------------------------------------------------+
// MSC Callbacks
//--------------------------------------------------------------------+

uint8_t tud_msc_get_maxlun_cb(void) {
    return 1; // single LUN — SD card
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;

    const char vid[] = "CETI";
    const char pid[] = "SD Card";
    const char rev[] = "1.0";

    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;

    HAL_SD_CardInfoTypeDef info;
    HAL_SD_GetCardInfo(&hsd1, &info);

    *block_count = info.LogBlockNbr;
    *block_size = (uint16_t)info.LogBlockSize;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset; // always 0 for block-aligned access

    uint32_t num_blocks = bufsize / SD_BLOCK_SIZE;

    sd_rx_cplt = 0;
    if (HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t *)buffer, lba, num_blocks) != HAL_OK) {
        return -1;
    }

    // Wait for DMA completion
    uint32_t start = HAL_GetTick();
    while (!sd_rx_cplt) {
        if (HAL_GetTick() - start > SD_TIMEOUT_MS)
            return -1;
    }

    // Wait for card to return to transfer state
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
        if (HAL_GetTick() - start > SD_TIMEOUT_MS)
            return -1;
    }

    return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset; // always 0 for block-aligned access

    uint32_t num_blocks = bufsize / SD_BLOCK_SIZE;

    sd_tx_cplt = 0;
    if (HAL_SD_WriteBlocks_DMA(&hsd1, buffer, lba, num_blocks) != HAL_OK) {
        return -1;
    }

    // Wait for DMA completion
    uint32_t start = HAL_GetTick();
    while (!sd_tx_cplt) {
        if (HAL_GetTick() - start > SD_TIMEOUT_MS)
            return -1;
    }

    // Wait for card to finish programming
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {
        if (HAL_GetTick() - start > SD_TIMEOUT_MS)
            return -1;
    }

    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize) {
    (void)buffer;
    (void)bufsize;

    switch (scsi_cmd[0]) {
        default:
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
            return -1;
    }
}

#endif
