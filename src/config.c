/*****************************************************************************
 *   @file      config.c
 *   @brief     
 *   @project   Project CETI
 *   @date      1/14/2025
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#include "config.h"

#include "stm32u5xx_hal.h"

// defines the section start and end
extern uint8_t _tag_config_flash_start;
extern uint8_t _tag_config_flash_end;

__attribute__ ((section(".tag_config_flash")))
CetiTagRuntimeConfiguration nv_tag_config;

uint8_t tag_config_valid = 0;
CetiTagRuntimeConfiguration tag_config = {0};

/// @brief  Apply the current tag configure to the system 
/// @param  
void config_apply_to_system(void) {
    #warning "ToDo: implement config_apply_to_system"
}

/// @brief Stores current tag_config to nonvolatile flash
/// @param  
void config_save(void) {
    FLASH_EraseInitTypeDef erase_def = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_2,
        .Page = 254,
        .NbPages = 1,
    };
    
    HAL_StatusTypeDef result = HAL_OK;
    uint32_t page_error;

    result = HAL_FLASH_Unlock();

    // flash must be erased before being written.
    result |= HAL_FLASHEx_Erase(&erase_def, &page_error);

    // write volatile copy of  configuration to  flash
    for (size_t size = 0; size < sizeof(CetiTagRuntimeConfiguration); size += (8 * 16)) {
        result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BURST, ((uint32_t)&nv_tag_config) + size, ((uint32_t)&tag_config) + size);
        if (result != HAL_OK) {

        }
    }

    result |= HAL_FLASH_Lock();
}


/// @brief Reloads nonvolatile configuration to the tag
/// @param  
void config_reload(void) {
    tag_config = nv_tag_config;
    tag_config_valid = 1;
    config_apply_to_system();
}


/// @brief Initailizes flash memory and reads persistant tag configuration from
/// from `COFIG_FLASH`
/// @param  
HAL_StatusTypeDef config_init(void) {
    HAL_StatusTypeDef result = HAL_OK;
    
    result = HAL_FLASH_Unlock();
    result |= HAL_FLASH_Lock();

    // copy tag configuration to nonvolatile tag config
    config_reload();

    return result;
}
