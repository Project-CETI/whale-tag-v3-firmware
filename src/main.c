//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

/* Global & HAL includes */
#include <app_filex.h>
#include <gpdma.h>
#include <gpio.h>
#include <i2c.h>
#include <icache.h>
#include <main.h>
#include <rtc.h>
#include <sdmmc.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>
#include <usb_otg.h>

/* Library Includes */
#include "tusb.h"

/* Local Includes */
#include "audio/acq_audio.h"
#include "battery/acq_battery.h"
#include "battery/bms_ctl.h"
#include "burnwire.h"
#include "config.h"
#include "ecg/acq_ecg.h"
#include "gps/gps.h"
#include "imu/acq_imu.h"
#include "led/led.h"
#include "mission.h"
#include "pressure/acq_pressure.h"
#include "satellite/satellite.h"
#include "timing.h"
#include "usb/usb.h"
#include "version_hw.h"

#include "pressure/keller4ld.h"

#include "syslog.h"

void SystemClock_Config(void);

#define BOOTLOADER_REQUEST_MAGIC 0xB00710ADUL
#define STM32U5_SYSTEM_MEMORY    0x0BF90000UL

#define FAULT_TOLERANT_SIZE (3 * 1024)
uint8_t fault_tolerant_buffer[FAULT_TOLERANT_SIZE];
FX_MEDIA sdio_disk = {}; // struct must be initialized or invalid pointers exist
ALIGN_32BYTES(uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

extern UART_HandleTypeDef SAT_huart;
extern UART_HandleTypeDef GPS_huart;
extern void satellite_rx_callback(UART_HandleTypeDef *huart, uint16_t pos);
extern void gps_rx_callback(UART_HandleTypeDef *huart, uint16_t pos);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t pos) {
    if (SAT_huart.Instance == huart->Instance) {
        satellite_rx_callback(huart, pos);
    } else if (GPS_huart.Instance == huart->Instance) {
        gps_rx_callback(huart, pos);
    } else {
        __NOP();
    }
}

/**
 * @brief Power Configuration
 * @retval None
 */
static void SystemPower_Config(void) {
    HAL_PWREx_EnableVddIO2();

    /*
     * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
     */
    HAL_PWREx_DisableUCPDDeadBattery();

    /*
     * Switch to SMPS regulator instead of LDO
     */
    if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK) {
        Error_Handler();
    }
    /* USER CODE BEGIN PWR */
    /* USER CODE END PWR */
}

int bms_ctl_verify(void);
int bms_ctl_program_nonvolatile_memory(void);

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    switch (GPIO_Pin) {
        case KELLER_DRDY_EXTI9_Pin:
            if (tag_config.pressure.enabled) {
                keller4ld_eoc_callback();
            }
            break;
        default:
            __NOP();
            break;
    }
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    switch (GPIO_Pin) {
        case ECG_ADC_NDRDY_GPIO_Input_Pin: // 2
            if (tag_config.ecg.enabled) {
                acq_ecg_EXTI_Callback();
            }
            break;
        case IMU_NINT_GPIO_EXTI10_Pin: // 10
            if (tag_config.imu.enabled) {
                acq_imu_EXTI_Callback();
            }
            break;
        default:
            __NOP();
            break;
    }
}

int verify_i2c_bus_1(void) {
    // pressure sensor
    if (HAL_I2C_IsDeviceReady(&hi2c1, (0x40 << 1), 3, 5) == HAL_OK) {
        CETI_LOG("Pressure sensor address 0x40 on i2c bus 1");
    } else {
        CETI_WARN("Pressure sensor not found on i2c bus 1");
    }

    // verify i2c bus 2
    // ecg
    if (HAL_I2C_IsDeviceReady(&hi2c2, (0x68 << 1), 3, 5) == HAL_OK) {
        CETI_LOG("ecg adc address 0x68 on i2c bus 2");
    } else {
        CETI_WARN("ecg adc not found on i2c bus 2");
    }
    // verify i2c bus 3
    // LED driver
    if (HAL_I2C_IsDeviceReady(&hi2c3, (0x30 << 1), 3, 5) == HAL_OK) {
        CETI_LOG("led driver address 0x30 on i2c bus 3");
    } else {
        CETI_WARN("led driver not found on i2c bus 3");
    }

    // BMS
    if (HAL_I2C_IsDeviceReady(&hi2c3, (0x36 << 1), 3, 5) == HAL_OK) {
        CETI_LOG("bms lower address 0x36 on i2c bus 3");
    } else {
        CETI_WARN("bms driver not found on i2c bus 3");
    }

    // BMS
    if (HAL_I2C_IsDeviceReady(&hi2c3, (0x0b << 1), 3, 5) == HAL_OK) {
        CETI_LOG("bms lower address 0x0b on i2c bus 3");
    } else {
        CETI_WARN("bms driver not found on i2c bus 3");
    }

    return 0;
}


/// @brief This function sets jumps to the built-in STM32 bootloader programmatically 
/// @param  
/// @note Although we CAN reload the flash of this system via our normal tinyUSB DMA
/// this would be an unsafe operation as we will be replacing instructions a method addresses
/// mid program. This bootloader exists seperate from the system's FLASH making it a safe 
/// alternative. (MSH)
static void jump_to_system_bootloader(void) {
    volatile uint32_t sys_mem = STM32U5_SYSTEM_MEMORY;
    uint32_t boot_sp = *(volatile uint32_t *)(sys_mem);
    uint32_t boot_pc = *(volatile uint32_t *)(sys_mem + 4);

    // Reset clocks to default state
    HAL_RCC_DeInit();
    HAL_DeInit();

    // Disable SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // Disable all interrupts and clear pending
    __disable_irq();
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // Re-enable interrupts (system bootloader expects them enabled)
    __enable_irq();

    // Set vector table to system memory
    SCB->VTOR = sys_mem;

    // Set stack pointer and jump to system bootloader
    __set_MSP(boot_sp);
    void (*sys_boot)(void) = (void (*)(void))boot_pc;
    sys_boot();

    while (1); // never reached
}

/// @brief Main
/// @param  
/// @return 
int main(void) {
    HAL_Init();

    /* Check if CDC "update" command requested a reboot into DFU bootloader.
     * TAMP backup registers survive system reset, so we use BKP0R as a flag. */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_RTCAPB_CLK_ENABLE();
    if (TAMP->BKP0R == BOOTLOADER_REQUEST_MAGIC) {
        TAMP->BKP0R = 0; // clear flag so next normal boot works
        jump_to_system_bootloader();
    }

    SystemClock_Config();
    SystemPower_Config();

    MX_GPDMA1_Init();
    MX_ICACHE_Init();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();

    /* initialize i2c for BMS and LEDs*/
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPINIT_CB_ID, HAL_I2C_MspInit);
    HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPDEINIT_CB_ID, HAL_I2C_MspDeInit);
    MX_I2C3_Init();
    led_init();
    led_idle();

    /* setup timing for accurate uS timing */
    timing_init();

    /* open SD card for system logging */
    MX_SDMMC1_SD_Init();
    MX_FileX_Init();
    int filex_status = fx_media_open(&sdio_disk, "", fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *)fx_sd_media_memory, sizeof(fx_sd_media_memory));
    if (filex_status == FX_SUCCESS) {
        syslog_init();
        CETI_LOG("Program started!");
    } else {
        led_error();
    }
    fx_fault_tolerant_enable(&sdio_disk, fault_tolerant_buffer, FAULT_TOLERANT_SIZE); // enable fault tolerance
    
    /* load system configuration from nonvolatile memory */
    config_init();

    // verify and update volume name
    char volume_name[12];
    fx_media_volume_get(&sdio_disk, (char *)volume_name, FX_BOOT_SECTOR);
    if (0 != strcmp(volume_name, (char *)tag_config.hostname)) {
        fx_media_volume_set(&sdio_disk, (char *)tag_config.hostname);
    }

    /* initialize i2c bus from pressure sensor */
    HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPINIT_CB_ID, HAL_I2C_MspInit);
    HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPDEINIT_CB_ID, HAL_I2C_MspDeInit);
    MX_I2C1_Init();

    /* basic BMS validation */
    if (tag_config.battery.enabled) {
        int bms_settings_verified = bms_ctl_verify();
        if (!bms_settings_verified) {
            // CETI_ERR("MAX17320 nonvolatile memory was not as expected: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
            CETI_ERR("Consider rewriting NV memory!!!!");
            CETI_LOG("Attempting to overlay values:");
            bms_ctl_temporary_overwrite_nv_values();
    //        bms_ctl_program_nonvolatile_memory();
        }
        bms_ctl_reset_FETs(); // enable charging and discharging
    }

    /* Initialize system sample acquistion (used by both mission and usb) ****/
    acq_audio_init(&tag_config.audio);

    acq_battery_init(); // initialize battery sample acquisition

#if HW_VERSION == HW_VERSION_3_1_0 || HW_VERSION == HW_VERSION_3_1_0_MSH
    // Note: on these versions of the tag, GPS must be powered to prevent the
    // shared i2c bus from being pulled low.
    HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
#endif
    // acq_ecg_enable();
    gps_init();
    gps_standby();
    acq_pressure_init(tag_config.pressure.samplerate_Hz);
    burnwire_init();
    // if (tag_config.imu.enabled) {

    acq_imu_init(); /// @note IMU must be initialized after audio due to shared SPI bus
    // MX_USART2_UART_Init();
    // satellite_init();

    /* Detect if the external interface is present to enable USB for offload/debug/DFU */
    if (usb_iface_present()) {
        /* Enter USB/User Control loop */
        CETI_LOG("Key detected. Starting USB Device Interface");

        // Release SD card from FileX — USB MSC takes over exclusively
        fx_media_close(&sdio_disk);

        // initialize usb interface
        usb_init();

        led_usb();

        /* main loop while in USB */
        while (usb_iface_present()) {
            usb_task();
        }

        /* reboot system to return to capture state once key is removed */
        NVIC_SystemReset();
    } else {
        /* Enter Mission loop */
        mission_init();

        while (1) {
            /* perform main task */
            mission_task();
            fx_media_flush(&sdio_disk);
            /* sleep until new tasks occur */
            mission_sleep();
        }
    }
    // we should never get here
    Error_Handler();
    return -1;
}

// Generic Error Catcher;
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
        led_error();
    }
    /* USER CODE END Error_Handler_Debug */
}
