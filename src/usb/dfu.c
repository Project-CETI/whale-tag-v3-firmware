#include <stm32u5xx_hal.h>

#include "tusb.h"

#include "../satellite/previpass.h"

#include "../config.h"

#if CFG_TUD_DFU

extern uint8_t _flash_start;
extern uint8_t _flash_end;
extern uint8_t _tag_config_flash_start;
extern uint8_t _tag_config_flash_end;
extern uint8_t _tag_aop_start;
extern uint8_t _tag_aop_end;

extern DCACHE_HandleTypeDef hdcache1;


#define ALT_COUNT (3)

#define ADDR_TO_FLASH_BANK(addr) (((((uint32_t)(addr)) - ((uint32_t)&_flash_start)) / FLASH_BANK_SIZE))
#define ADDR_TO_FLASH_PAGE(addr) (((((uint32_t)(addr)) - ((uint32_t)&_flash_start))/ FLASH_PAGE_SIZE) % (FLASH_PAGE_NB))
#define ALT_BASE_ADDR(alt) ((0 == (alt)) ? ((uint32_t)&_flash_start) \
                           : (1 == (alt)) ? ((uint32_t)&_tag_config_flash_start) \
                           : (2 == (alt)) ? ((uint32_t)&_tag_aop_start) \
                           : ((uint32_t)&_tag_aop_end))

static uint16_t s_byte_count[ALT_COUNT] = {0};
static uint16_t s_current_flash_page[ALT_COUNT] = {0};
static uint8_t  s_page_data[ALT_COUNT][FLASH_PAGE_SIZE];


//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
  if ( state == DFU_DNBUSY )
  {
    return 1;
  }
  else if (state == DFU_MANIFEST)
  {
    // since we don't buffer entire image and do any flashing in manifest stage
    return 0;
  }

  return 0;
}

static HAL_StatusTypeDef write_page(uint8_t alt) {
    // write out block to flash
    uint32_t flash_base_addr = ALT_BASE_ADDR(alt);

    uint32_t flash_addr = flash_base_addr + (s_current_flash_page[alt] * FLASH_PAGE_SIZE);
            FLASH_EraseInitTypeDef erase_def = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_1 + ADDR_TO_FLASH_BANK(flash_addr),
        .Page = ADDR_TO_FLASH_PAGE(flash_addr),
        .NbPages = 1,
    };
    HAL_StatusTypeDef result = HAL_OK;
    uint32_t page_error;

    result = HAL_FLASH_Unlock();

    // flash must be erased before being written.
    result |= HAL_FLASHEx_Erase(&erase_def, &page_error);

    // write volatile copy of  configuration to  flash
    for (size_t wsize = 0; wsize < FLASH_PAGE_SIZE; wsize += (8 * 16)) {
        result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BURST, flash_addr + wsize, (uint32_t)&s_page_data[alt][wsize]);
        if (HAL_OK != result) {
            return result;
        }
    }

    result |= HAL_FLASH_Lock();
    // HAL_DCACHE_InvalidateByAddr(&hdcache1, flash_addr, FLASH_PAGE_SIZE);
    HAL_ICACHE_Invalidate();
    return result;
}

// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    (void) block_num; // not used

    // move data into appropriate page buffer
    uint16_t remaining_bytes_in_block = FLASH_PAGE_SIZE - s_byte_count[alt];
    while (remaining_bytes_in_block <= length) {
        memcpy(&s_page_data[alt][s_byte_count[alt]], data, remaining_bytes_in_block);

        // write out block to flash
        write_page(alt);
        
        //advance data ptrs
        data = data + remaining_bytes_in_block;
        length = length - remaining_bytes_in_block;
        s_current_flash_page[alt] += 1; // flash page
        s_byte_count[alt] = 0;
        remaining_bytes_in_block = FLASH_PAGE_SIZE;
    }

    // copy over any partial pages
    memcpy(&s_page_data[alt][s_byte_count[alt]], data, length);
    s_byte_count[alt] += length;

    // flashing op for download complete without error
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt)
{
    if (0 != s_byte_count[alt]) { // partial page to write
        write_page(alt);
    }
    s_byte_count[alt] = 0;
    s_current_flash_page[alt] = 0; // flash page

    // flashing op for manifest is complete without error
    // Application can perform checksum, should it fail, use appropriate status such as errVERIFY.
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
    uint32_t offset = (uint32_t)block_num * CFG_TUD_DFU_XFER_BUFSIZE;
    uint32_t flash_base_addr = ALT_BASE_ADDR(alt);
    uint32_t requested_address = flash_base_addr + offset;

    switch (alt) {
        case 0: {// Flash  
            if (requested_address >  (uint32_t)&_flash_end){
                return 0;
            }
            uint16_t bytes_remaining = (uint16_t)(((uint32_t)&_flash_end) - requested_address);
            uint16_t xfer_len = (length < bytes_remaining) ? length : bytes_remaining;
            memcpy(data, (void *)requested_address, xfer_len);
            return xfer_len;
        }

        case 1: { // Config 
            if (offset >  sizeof(tag_config)){
                return 0;
            }
            uint16_t bytes_remaining = (uint16_t)(sizeof(tag_config) - offset);
            uint16_t xfer_len = (length < bytes_remaining) ? length : bytes_remaining;
            memcpy(data, (void *)requested_address, xfer_len);
            return xfer_len;
        }

        case 2: {// Aop
            uint32_t aop_size_offset = ((uint32_t)&_tag_aop_start) + sizeof(uint64_t);
            uint16_t aop_table_len = *(uint16_t *)aop_size_offset;
            uint32_t aop_table_size = 16 + aop_table_len * sizeof(struct AopSatelliteEntry_t);
            if (offset >  aop_table_size){
                return 0;
            }
            uint16_t bytes_remaining = (uint16_t)(aop_table_size - offset);
            uint16_t xfer_len = (length < bytes_remaining) ? length : bytes_remaining;
            memcpy(data, (void *)requested_address, xfer_len);
            return xfer_len;
        }
        
        default:
            return 0;
    }
}

// Invoked when the Host has terminated a download or upload transfer
void tud_dfu_abort_cb(uint8_t alt)
{
    (void) alt;

    // clear existing buffered data
    s_byte_count[alt] = 0;
    s_current_flash_page[alt] = 0;
}

// Invoked when a DFU_DETACH request is received
void tud_dfu_detach_cb(void)
{
    // printf("Host detach, we should probably reboot\r\n");
    // NVIC_SystemReset();
}

#endif