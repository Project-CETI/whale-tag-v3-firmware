/*****************************************************************************
 *   @file      metadata.h
 *   @brief     Code to log tag configuration and metadata to SD card
 *   @project   Project CETI
 *   @date      04/03/2026
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [Add other contributors here]
 *****************************************************************************/
#include "metadata.h"

#include <app_filex.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "timing.h"
#include "version.h"
#include "version_hw.h"

extern FX_MEDIA sdio_disk;
static FX_FILE s_fp = {};

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
            "    adc: { manufacturer: Analog Devices, model: AD7768-4 }\n"
            "    channels:\n"
            "      - hydrophone:\n"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: { value: 18, units: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: 100, units: Hz }\n"
            "            max: { value: 100, units: kHz }\n"
            "          placement:\n"
            "            x: { value: 32.6, units: mm }\n"
            "            y: { value: -51.4, units: mm }\n"
            "            z: { value: 34.8, units: mm }\n" 
            "      - null\n"
            "      - hydrophone:\n"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: {value: 18, units: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: 100, units: Hz }\n"
            "            max: { value: 100, units: kHz }\n"
            "          placement:\n"
            "            x: { value: -32.6, units: mm }\n"
            "            y: { value: -51.4, units: mm }\n"
            "            z: { value: 34.8, units: mm }\n" 
            "      - hydrophone:\n"
            "          manufacturer: Project CETI\n"
            "          model: PH2\n"
            "          shape: spherical\n"
            "          gain: { value: 18, units: dB }\n"
            "          frequency_range:\n" 
            "            min: { value: 100, units: Hz }\n"
            "            max: { value: 100, units: kHz }\n"
            "          placement:\n"
            "            x: { value: -32.6, units: mm }\n"
            "            y: { value: 51.4, units: mm }\n"
            "            z: { value: 34.8, units: mm }\n" 
        );
    }
    if (tag_config.hw_config.bms.available) {
        // Note consider getting age, etc from 
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  battery:\n"
            "    bms:\n"
            "      manufacturer: Analog Devices\n"
            "      model: MAX17320\n"
            "    cell:\n"
            "      manufacturer: AA Portable Power Corp\n"
            "      model: PL-605060-2C\n"
            "      nominal_voltage: { value: 3.7, units: V }\n"
            "      capacity: { value: 2000, units: mAh }\n"
            "    configuration: 2S\n"
            "    thermistors: 1\n"
        );
    }

    if (tag_config.hw_config.ecg.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  ecg:\n"
            "    adc:\n"
            "      manufacturer: Texas Instruments\n"
            "      model: ADS1219\n"
            "    amp:\n"
            "      manufacturer: Analog Devices\n"
            "      model: AD8232\n"
            "    configuration: %s\n"
            "    gain: { value: %0.2f , units: dB } \n",
            (tag_config.hw_config.ecg.configuration == ECG_HW_CONFIG_2_TERMINAL) ? "2 Terminal"
            : (tag_config.hw_config.ecg.configuration == ECG_HW_CONFIG_3_TERMINAL) ? "3 Terminal" 
            : "unknown",
            tag_config.hw_config.ecg.gain_db
        );
    }
    
    if (tag_config.hw_config.gps.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  gps:\n"
            "    manufacturer: uBlocks\n"
            "    model: MAX-M10s\n"
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
            "    placement:\n"
            "      x: { value: 0.0, units: mm }\n"
            "      y: { value: 31.8, units: mm }\n"
            "      z: { value: 21.0, units: mm }\n" 
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
            "  argos:\n"
            "    manufacturer: Arribada\n"
            "    model: SMD\n"
        );
    }
    
    if (tag_config.hw_config.vhf_pinger.available) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  vhf_pinger:\n"
            "    controllable: %s"
            "    ping_rate: { value: 1, units: Hz }"
            "    carrier_frequency: { value: %.3f, units: MHz }\n"
            , (HW_VERSION == HW_VERSION_3_2_0) ? "true" : "false" 
            , tag_config.hw_config.vhf_pinger.carrier_frequency_mhz
        );
    }

    UINT fx_write_result = fx_file_write(&s_fp, buffer, offset);
}

static void __write_static_software_config(void) {
    uint16_t offset = 0;
    char buffer[2048];

    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "software_settings: \n"
        "  version: \"" FW_VERSION_TEXT "\"\n"
        "  compilation_data: \"" FW_COMPILATION_DATE "\"\n"
    );

    if (tag_config.hw_config.audio.available) {
        if (!tag_config.audio.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  audio:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  audio:\n"
                "    enabled: true\n"
                "    bitdepth: { value: %d, units: bits }\n"
                "    filter_type: %s\n"
                "    priority: %s\n"
                "    sample_rate: { value: %ld, units: Hz }\n"
                "    channels:\n"
                "      - enabled: %s\n"
                "      - enabled: %s\n"
                "      - enabled: %s\n"
                "      - enabled: %s\n"
                , tag_config.audio.bitdepth
                , (tag_config.audio.filter_type == AUDIO_FILTER_WIDEBAND) ? "wideband"
                  : (tag_config.audio.filter_type == AUDIO_FILTER_SINC) ? "sinc"
                  : "null"
                , (tag_config.audio.priority == AUDIO_PRIORITIZE_POWER) ? "power"
                  : (tag_config.audio.priority == AUDIO_PRIORITIZE_NOISE) ? "sinc"
                  : "null"
                , tag_config.audio.samplerate_sps
                , (tag_config.audio.channel_enabled[0]) ? "true" : "false"
                , (tag_config.audio.channel_enabled[1]) ? "true" : "false"
                , (tag_config.audio.channel_enabled[2]) ? "true" : "false"
                , (tag_config.audio.channel_enabled[3]) ? "true" : "false"
            );
        }
    }

    if (tag_config.hw_config.argos.available) {
        if (!tag_config.argos.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  argos:\n"
                "    enabled: false\n"
            );
        } else {
            uint8_t id_buffer[9] = {};
            memcpy(id_buffer, tag_config.argos.id, 8); 

            uint8_t address_buffer[9] = {};
            memcpy(address_buffer, tag_config.argos.address, 8); 

            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  argos:\n"
                "    enabled: true\n"
                "    id: %s\n"
                "    address: %s\n"
                , id_buffer
                , address_buffer
            );
        }
    }

    if (tag_config.hw_config.bms.available) {
        if (!tag_config.battery.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  bms:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  bms:\n"
                "    enabled: true\n"
                "    sample_rate: { value: %d, units: Hz }\n"
                , (1000 / tag_config.battery.samplerate_ms)
            );
        }
    }

    if (tag_config.hw_config.ecg.available) {
        if (!tag_config.ecg.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  ecg:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  ecg:\n"
                "    enabled: true\n"
                "    sample_rate: { value: %d, units: Hz }\n"
                , (1000 / tag_config.ecg.samplerate_ms)
            );
        }
    }

    if (tag_config.hw_config.gps.available) {
        if (!tag_config.gps.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  gps:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  gps:\n"
                "    enabled: true\n"
            );
        }
    }

    if (tag_config.hw_config.pressure.available) {
        if (!tag_config.pressure.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  pressure:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  pressure:\n"
                "    enabled: true\n"
                "    sample_rate: { value: %d, units: Hz }\n"
                , (1000 / tag_config.pressure.samplerate_ms)
            );
        }
    }

    if (tag_config.hw_config.flasher.available) {
        if (!tag_config.flasher.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  led_flasher:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  led_flasher:\n"
                "    enabled: true\n"
            );
        }
    }

    UINT fx_write_result = fx_file_write(&s_fp, buffer, offset);
}

static void __write_static_mission_config(void) {
    uint16_t offset = 0;
    char buffer[2048];

    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "mission: \n"
    );

    if (tag_config.hw_config.burnwire.available) {
        if (!tag_config.burnwire.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  burnwire:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  burnwire:\n"
                "    enabled: true\n"
                "    duration: { value: %ld, units: seconds }\n"
                , tag_config.burnwire.duration_s
            );
        }
    }
    if (tag_config.hw_config.bms.available) {
        if (!tag_config.mission.low_power_release.enabled) {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  low_power_release:\n"
                "    enabled: false\n"
            );
        } else {
            offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
                "  low_power_release:\n"
                "    enabled: true\n"
                "    voltage: {value: %d, units: mv }\n"
                , tag_config.mission.low_power_release.threshold_mV
            );
        }
    }

    if (!tag_config.mission.timer_release.enabled) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  timer_release:\n"
            "    enabled: false\n"
        );
    } else {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  timer_release:\n"
            "    enabled: true\n"
            "    hours: %d\n"
            "    minutes: %d\n"
            , tag_config.mission.timer_release.hours
            , tag_config.mission.timer_release.minutes
        );
    }

    if (!tag_config.mission.time_of_day_release_utc.enabled) {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  time_of_day_release:\n"
            "    enabled: false\n"
        );
    } else {
        offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
            "  time_of_day_release:\n"
            "    enabled: true\n"
            "    time: %02d:%02d UTC\n"
            , tag_config.mission.time_of_day_release_utc.hour
            , tag_config.mission.time_of_day_release_utc.minute
        );
    }

    UINT fx_write_result = fx_file_write(&s_fp, buffer, offset);
}

static void __write_file_name_section(char *mission_directory) {
    uint16_t offset = 0;
    char buffer[2048];
    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "logging:\n"
        "  directory_path: %s\n"
        "  files:\n"
        , mission_directory
    );
    UINT fx_write_result = fx_file_write(&s_fp, buffer, offset);
    return;
}

static const char * data_type_str[] = {
    [DATA_TYPE_METADATA] = "metadata",
    [DATA_TYPE_ARGOS] = "argos",
    [DATA_TYPE_AUDIO] = "audio",
    [DATA_TYPE_BMS] = "bms",
    [DATA_TYPE_BENCHMARK] = "benchmark",
    [DATA_TYPE_ECG] = "ecg",
    [DATA_TYPE_ERRORS] = "errors",
    [DATA_TYPE_GPS] = "gps",
    [DATA_TYPE_IMU_ROTATION] = "rotation",
    [DATA_TYPE_IMU_ACCEL] = "accelerometer",
    [DATA_TYPE_IMU_GYRO] = "gryoscope",
    [DATA_TYPE_IMU_MAG] = "magnetometer",
    [DATA_TYPE_MISSION] = "mission",
    [DATA_TYPE_PRESSURE] = "pressure",
};

static const char * format_str[] = {
    [DATA_FORMAT_YAML] = "yaml",
    [DATA_FORMAT_CSV] = "csv",
    [DATA_FORMAT_BIN] = "bin",
    [DATA_FORMAT_TXT] = "txt",
};

void metadata_log_file_creation(char * filename, DataType data_type, DataFormat format, uint16_t version) {
    uint16_t offset = 0;
    char buffer[2048];
    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "    - file: { filename: %s, type: %s, format: %s, version: %d }\n"
        , filename 
        , data_type_str[data_type]
        , format_str[format]
        , version
    );
    UINT fx_write_result = fx_file_write(&s_fp, buffer, offset);
    return;
}

void metadata_create(char * mission_directory) {
    // create file
    UINT fx_create_result = fx_file_create(&sdio_disk, "tag_info.yaml");

    UINT fx_open_result = fx_file_open(&sdio_disk, &s_fp, "tag_info.yaml", FX_OPEN_FOR_WRITE);

    // write basic info    
    uint16_t offset = 0;
    char buffer[2048];

    offset += snprintf((char *)&buffer[offset], sizeof(buffer) - offset, 
        "---\n"
        "id: %s\n"
        "timestamp: %lld\n"
        , tag_config.hostname
        , rtc_get_epoch_s()
    );

    __write_static_hardware_config();

    __write_static_software_config();
    
    __write_static_mission_config();

    // log files as they get created
    __write_file_name_section(mission_directory);
    metadata_log_file_creation("tag_info.yaml", DATA_TYPE_METADATA, DATA_FORMAT_YAML, 0);
}

