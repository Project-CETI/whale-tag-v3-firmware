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
extern uint8_t _tag_aop_start;
extern uint8_t _tag_aop_end;

__attribute__((section(".tag_config_flash")))
CetiTagRuntimeConfiguration nv_tag_config = {
    .config_version = 0xdeadbeef,
    .hw_version = HW_VERSION,
    .audio = {
        .enabled = AUDIO_ENABLED,
        .channel_enabled = {
            [0] = AUDIO_CH_0_EN,
            [1] = AUDIO_CH_1_EN,
            [2] = AUDIO_CH_2_EN,
            [3] = AUDIO_CH_3_EN,
        },
        .bitdepth = AUDIO_SAMPLE_BITDEPTH,
        .filter_type = AUDIO_FILTER_WIDEBAND,
        .priority = AUDIO_PRIORITIZE_POWER,
        .samplerate_sps = AUDIO_SAMPLERATE_SPS,
    },
    .argos = {
        .enabled = SATELLITE_ENABLED,
    },
    .battery = {
        .enabled = BMS_ENABLED,
    },
    .burnwire = {
        .enabled = BURNWIRE_ENABLED,
        .duration_s = 20*60,
    },
    .ecg = {
        .enabled = ECG_ENABLED,
    },
    .flasher = {
        .enabled = FLASHER_ENABLED,
    },
    .gps = {
        .enabled = GPS_ENABLED,
    },

    .imu = {
        .enabled = IMU_ENABLED,
    },
    .pressure = {
        .enabled = PRESSURE_ENABLED,
        .samplerate_Hz = 1,
    },
};

uint8_t tag_config_valid = 0;
CetiTagRuntimeConfiguration tag_config = {0};

// stores aop_file to flash
void aop_update(uint8_t *data, uint16_t data_size) {
    FLASH_EraseInitTypeDef erase_def = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_2,
        .Page = 255,
        .NbPages = 1,
    };

    HAL_StatusTypeDef result = HAL_OK;
    uint32_t page_error;

    result = HAL_FLASH_Unlock();

    // flash must be erased before being written.
    result |= HAL_FLASHEx_Erase(&erase_def, &page_error);

    // write volatile copy of  configuration to  flash
    for (size_t size = 0; size < data_size; size += (8 * 16)) {
        result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BURST, ((uint32_t)(&_tag_aop_start)) + size, ((uint32_t)data) + size);
        if (result != HAL_OK) {
        }
    }

    result |= HAL_FLASH_Lock();
}

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
