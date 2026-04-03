/*****************************************************************************
 *   @file      metadata.h
 *   @brief     Code to log tag configuration and metadata to SD card
 *   @project   Project CETI
 *   @date      04/03/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [Add other contributors here]
 *****************************************************************************/

#include <app_filex.h>
#include <stdint.h>

#include "config.h"


extern FX_MEDIA sdio_disk;
state FX_FILE s_fp = {};

static void __write_static_hardware_config(void) {
    uint16_t offset = 0;
    char buffer[2048];

    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "hardware: \n"
        "  manufacturer: Project CETI\n"
        "  model: %s\n"
        , (HW_VERSION == HW_VERSION_3_2_0) ? "WhaleTag v3.2" 
          : (HW_VERSION == HW_VERSION_3_1_1) ? "WhaleTage v3.1 (Modified)" 
          : "Unknown"
    );

    if (tag_config.hw_config.audio.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  audio:\n"
            "    channels:\n"
            "      - hydrophone:\n"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: { value: 18, unit: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: , unit: Hz }\n"
            "            max: { value: , unit: Hz }\n"
            "          placement:\n"
            "            x: { value: , unit: mm}\n"
            "            y: { value: , unit: mm}\n"
            "            z: { value: , unit: mm}\n" 
            "      - null\n"
            "      - hydrophone:\n"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: {value: 18, unit: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: , unit: Hz }\n"
            "            max: { value: , unit: Hz }\n"
            "          placement:\n"
            "            x: { value: , unit: mm}\n"
            "            y: { value: , unit: mm}\n"
            "            z: { value: , unit: mm}\n" 
            "      - hydrophone:"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: { value: 18, unit: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: , unit: Hz }\n"
            "            max: { value: , unit: Hz }\n"
            "          placement:\n"
            "            x: { value: , unit: mm }\n"
            "            y: { value: , unit: mm }\n"
            "            z: { value: , unit: mm }\n" 
            "    adc: { manufacturer: Analog Devices, model: AD7768-4 }\n"
        );
    }

    if (tag_config.hw_config.ecg.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  ecg:\n"
            "    adc:\n"
            "      manufacturer:\n"
            "      model:\n"
            "    amp: 1\n"
            "      manufacturer:\n"
            "      model\n"
            "    configuration: %s\n"
            "    gain: { value: %0.2f , unit: dB } \n",
            (tag_config.hw_config.ecg.configuration == ECG_HW_CONFIG_2_TERMINAL) ? "2 Terminal"
            : (tag_config.hw_config.ecg.configuration == ECG_HW_CONFIG_3_TERMINAL) ? "3 Terminal" 
            : "unknown",
            tag_config.hw_config.ecg.gain_db
        );
    }
    if (tag_config.hw_config.imu.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  imu:\n"
            "    manufacturer: CEVA Inc\n"
            "    model: BNO086\n"
            "    orientation:\n"
            "      x: starboard\n"
            "      y: bow\n"
            "      z: up\n"
        );
    }
    if (tag_config.hw_config.gps.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  gps:\n"
            "    manufacturer: uBlocks\n"
            "    model: MAX-M10s\n"
        );
    }
    if (tag_config.hw_config.pressure.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  pressure:\n"
            "    manufacturer: Keller\n"
            "    model: 4LD\n"
        );
    }
    if (tag_config.hw_config.argos.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  ARGOs:\n"
            "    manufacturer: Arribada\n"
            "    model: SMD\n"
        );
    }
    if (tag_config.hw_config.vhf_pinger.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  vhf_pinger:\n"
            "    controllable: %s"
            "    ping_rate: { value: 1, unit: Hz }"
            "    carrier_frequency: { value: %.3f, unit: MHz }\n"
            , (HW_VERSION == HW_VERSION_3_2_0) ? true : false 
            , tag_config.hw_config.vhf_pinger.carrier_frequency_mhz
        );
    }

    return fx_file_write(&s_fp, buffer, offset);
}

void metadata_create(void) {


    // create file
    UINT fx_create_result = fx_file_create(&sdio_disk, "tag_info.yaml");

    // write basic info    
    uint16_t offset = 0;
    char buffer[2048];

    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "---\n"
        "hostname: %s\n"
        "timestamp: %lld\n"
        tag_config.hostname,
        rtc_get_epoch_s()
    );

    // log files as they get created
}