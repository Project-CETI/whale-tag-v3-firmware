#include <stm32u5xx_hal.h>

#include "tusb.h"

#include "../config.h"

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


// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    static uint16_t byte_count[3] = {0};
    static uint16_t current_page[3] = {0};
    static uint8_t page_data[3][8*1024];

  (void) alt;
  (void) block_num;

    switch (alt) {
        case 0: // Flash
            break;
        case 1: // Config
            if (sizeof(tag_config) <= byte_count[alt]) {
                memcpy(&tag_config, page_data[alt], sizeof(tag_config));
                config_save();
            }   
            break;
        case 2: // Aop
            break;
    }

  //printf("\r\nReceived Alt %u BlockNum %u of length %u\r\n", alt, wBlockNum, length);

  for(uint16_t i=0; i<length; i++)
  {
    // printf("%c", data[i]);
  }

  // flashing op for download complete without error
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt)
{
  (void) alt;
//   printf("Download completed, enter manifestation\r\n");

  // flashing op for manifest is complete without error
  // Application can perform checksum, should it fail, use appropriate status such as errVERIFY.
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
  (void) block_num;
  (void) length;

    switch (alt) {
        case 0: // Flash
            break;

        case 1: { // Config 
            uint16_t bytes_remaining = sizeof(tag_config) - (block_num * length);
            uint16_t xfer_len = (length < bytes_remaining) ? length : bytes_remaining;
            memcpy(data, &tag_config, xfer_len);
            return xfer_len;
        }

        case 2: // Aop
            break;
    }
//   uint16_t const xfer_len = (uint16_t) strlen(upload_image[alt]);
//   memcpy(data, upload_image[alt], xfer_len);

//   return xfer_len;
    return 0;
}

// Invoked when the Host has terminated a download or upload transfer
void tud_dfu_abort_cb(uint8_t alt)
{
  (void) alt;
//   printf("Host aborted transfer\r\n");
}

// Invoked when a DFU_DETACH request is received
void tud_dfu_detach_cb(void)
{
//   printf("Host detach, we should probably reboot\r\n");
    NVIC_SystemReset();
}