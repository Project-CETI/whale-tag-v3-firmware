
#include "tusb.h"

/// @brief processes usb cdc 
/// @param  
void usb_cdc_task(void) {
    // Nothing to do if no data is avaiable
    if (!tud_cdc_available()) {
        return; 
    }
    
    char buffer[512];
    uint32_t count = tud_cdc_read(buffer, sizeof(buffer));

    // echo user input
    tud_cdc_write(buffer, count);
    tud_cdc_write_flush();
    
    // ToDo: process cdc data
}
