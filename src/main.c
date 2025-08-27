//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------

/* Global & HAL includes */
#include <main.h>
#include <gpio.h>
#include <gpdma.h>
#include <icache.h>
#include <i2c.h>
#include <rtc.h>
#include <sai.h>
#include <sdmmc.h>
#include <spi.h>
#include <tim.h>
#include <usb_otg.h>
#include <app_filex.h>

/* Library Includes */
#include "tusb.h"

/* Local Includes */
#include "config.h"
#include "audio/acq_audio.h"
#include "audio/log_audio.h"
#include "battery/acq_battery.h"
#include "battery/log_battery.h"
#include "battery/bms_ctl.h"
#include "ecg/acq_ecg.h"
#include "gps/gps.h"
#include "imu/acq_imu.h"
#include "led/led_ctl.h"
#include "mission.h"
#include "pressure/acq_pressure.h"
#include "pressure/log_pressure.h"
#include "satellite/satellite.h"
#include "timing.h"
#include "usb/usb.h"
#include "version.h"
#include "version_hw.h"

#include "log/log_syslog.h"

void SystemClock_Config(void);

FX_MEDIA sdio_disk = {}; //struct must be initialized or invalid pointers exist
ALIGN_32BYTES (uint32_t fx_sd_media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);



/**
  * @brief Power Configuration
  * @retval None
  */
static void SystemPower_Config(void)
{
  HAL_PWREx_EnableVddIO2();

  /*
   * Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
   */
  HAL_PWREx_DisableUCPDDeadBattery();

  /*
   * Switch to SMPS regulator instead of LDO
   */
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
/* USER CODE BEGIN PWR */
/* USER CODE END PWR */
}

int bms_ctl_verify(void);
int bms_ctl_program_nonvolatile_memory(void);

void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
	switch(GPIO_Pin) {
#ifdef PRESSURE_ENABLED
		case KELLER_DRDY_EXTI9_Pin:
			acq_pressure_EXTI_cb();
			break;
#endif // PRESSURE_ENABLED
		default:
			__NOP();
			break;
	}
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	switch(GPIO_Pin) {
#ifdef ECG_ENABLED
		case ECG_ADC_NDRDY_GPIO_Input_Pin: // 2
			acq_ecg_EXTI_Callback();
			break;
#endif // ECG_ENABLED
#ifdef IMU_ENABLED
		case IMU_NINT_GPIO_EXTI10_Pin: //10
            acq_imu_EXTI_Callback();
			// ToDo: implement IMU data ready interrupt
			break;
#endif // IMU_ENABLED
		default:
			__NOP();
			break;
	}
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    SystemPower_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();

//#define PROGRAM_ARRIBADA //uncomment this line to get the tag in a state to program the arribada module
#ifdef PROGRAM_ARRIBADA
    /* this is an infinite loop*/
    satellite_wait_for_programming();
#endif

    MX_I2C3_Init(); // BMS / LEDs
    led_init();
    led_idle();

    MX_GPDMA1_Init();
    MX_ICACHE_Init();

    /* setup timing for accurate uS timing*/
    MX_RTC_Init();
    MX_TIM4_Init();
    rtc_init();

    MX_I2C1_Init(); // pressure sensor
    
    /* open SD card for system logging */
    MX_SDMMC1_SD_Init();
    MX_FileX_Init();
    int filex_status = fx_media_open(&sdio_disk, "", fx_stm32_sd_driver, (VOID *)FX_NULL, (VOID *) fx_sd_media_memory, sizeof(fx_sd_media_memory));
    if(filex_status == FX_SUCCESS) {
        syslog_init();
        CETI_LOG("Program started!");
    } else {
        led_error();
    }

#ifdef BMS_ENABLED
    /* basic BMS validation */
    int bms_settings_verified = bms_ctl_verify();
    if (!bms_settings_verified) {
        // CETI_ERR("MAX17320 nonvolatile memory was not as expected: %s", wt_strerror_r(hw_result, err_str, sizeof(err_str)));
        CETI_ERR("Consider rewriting NV memory!!!!");
        CETI_LOG("Attempting to overlay values:");
        bms_ctl_temporary_overwrite_nv_values();
    }
    bms_ctl_reset_FETs(); // enable charging and discharging
#endif // BMS_ENABLED

#ifdef USB_ENABLED
    /* Detect if the external interface is present to enable USB for offload/debug/DFU */
#if VERSION != HW_VERSION_3_1_0_MSH
    if (usb_iface_present()) {
#endif
        CETI_LOG("Key detected. Starting USB Device Interface");
      
        /* initialize USB hardware */
        // ToDo: reconfigure gpio for USB here leave USB gpio as high impedence analog in normal config
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        MX_USB_OTG_HS_PCD_Init(); // setup HS USB Hardware
        
        
        // IOSV bit MUST be set to access GPIO port G[2:15] */
        HAL_PWREx_EnableVddIO2();
        
        /* USB clock enable */
        __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
        __HAL_RCC_USBPHYC_CLK_ENABLE();
        
        /* Enable USB power on Pwrctrl CR2 register */
        HAL_PWREx_EnableVddUSB();
        HAL_PWREx_EnableUSBHSTranceiverSupply();
        
        /*Configuring the SYSCFG registers OTG_HS PHY*/
        HAL_SYSCFG_EnableOTGPHY(SYSCFG_OTG_HS_PHY_ENABLE);
        
        // Disable VBUS sense (B device)
        USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;
        
        // B-peripheral session valid override enable
        USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALEXTOEN;
        USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALOVAL;
        HAL_Delay(100);
        usb_init(); // initialize tiny usb library
        
        /* main loop while in USB */
        while (usb_iface_present()) {
            // ToDo: update leds for usb
            tud_task(); // usb device task
            if (tud_mounted()){
              CETI_LOG("usb IS mounted");
            }
            // ToDo: react to incoming CDC requests
            // ToDo: sleep?
        }
        
        /* reboot system to return to capture state once key is removed */
        NVIC_SystemReset();
#if VERSION != HW_VERSION_3_1_0_MSH
    }
#endif
#endif // USB_ENABLED
      
#ifdef AUDIO_ENABLED
    /* enable Audio -5V */
    CETI_LOG("Initializing Audio");
    acq_audio_init();
    log_audio_init();
#endif // AUDIO_ENABLED

    /* perform runtime system hardware test to detect available systems */
#ifdef BMS_ENABLED
    CETI_LOG("Initializing BMS");
    acq_battery_init();
    log_battery_enable();
#endif // BMS_ENABLED


#ifdef PRESSURE_ENABLED
    CETI_LOG("Initializing Pressure");
#if HW_VERSION == HW_VERSION_3_1_0
    // Note: on this version of the tag, GPS must be powered to prevent the
    // shared i2c bus from being pulled low.
    HAL_GPIO_WritePin(GPS_PWR_EN_GPIO_Output_GPIO_Port, GPS_PWR_EN_GPIO_Output_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPS_NRST_GPIO_Output_GPIO_Port, GPS_NRST_GPIO_Output_Pin, GPIO_PIN_SET);
#endif
    acq_pressure_init();
    log_pressure_init();
#endif // PRESSURE_ENABLED

#ifdef IMU_ENABLED
    CETI_LOG("Initializing IMU");
    // acq_imu_init();
#endif // IMU_ENABLED

#ifdef ECG_ENABLED
    CETI_LOG("Initializing ECG");
    MX_I2C2_Init(); // ECG ADC
    // acq_ecg_enable();
#endif // ECG_ENABLED

#ifdef GPS_ENABLED
    CETI_LOG("Initializing GPS");
    gps_init();
#endif // GPS_ENABLED

#ifdef SATELLITE_ENABLED
    CETI_LOG("Initializing ARGOS");
    satellite_init();
#endif // SATELLITE_ENABLED

    CETI_LOG("Enabling Antenna Flasher");

    mission_set_state(MISSION_STATE_SURFACE);

/* system verification */
    // verify i2c bus 1
        // pressure sensor
        if (HAL_I2C_IsDeviceReady(&hi2c1, (0x40 << 1), 3, 5) == HAL_OK) {
            CETI_LOG("Pressure sensor address 0x40 on i2c bus 1");
        } else {
            CETI_WARN("Pressure sensor not found on i2c bus 1");
        }
        
        // gps
        if (HAL_I2C_IsDeviceReady(&hi2c1,  (0x42 << 1), 3, 5) == HAL_OK) {
            CETI_LOG("Gps sensor address 0x42 on i2c bus 1");
        } else {
            CETI_WARN("Pressure sensor not found on i2c bus 1");
        }

    // verify i2c bus 2
        // ecg
        if (HAL_I2C_IsDeviceReady(&hi2c2,  (0x68 << 1), 3, 5) == HAL_OK) {
            CETI_LOG("ecg adc address 0x68 on i2c bus 2");
        } else {
            CETI_WARN("ecg adc not found on i2c bus 2");
        }
    // verify i2c bus 3
        // LED driver
        if (HAL_I2C_IsDeviceReady(&hi2c3,  (0x30 << 1), 3, 5) == HAL_OK) {
            CETI_LOG("led driver address 0x30 on i2c bus 3");
        } else {
            CETI_WARN("led driver not found on i2c bus 3");
        }

        // BMS
        if (HAL_I2C_IsDeviceReady(&hi2c3,  (0x36 << 1), 3, 5) == HAL_OK) {
            CETI_LOG("bms lower address 0x36 on i2c bus 3");
        } else {
            CETI_WARN("bms driver not found on i2c bus 3");
        }

        
        // BMS
        if (HAL_I2C_IsDeviceReady(&hi2c3,  (0x0b << 1), 3, 5) == HAL_OK) {
            CETI_LOG("bms lower address 0x0b on i2c bus 3");
        } else {
            CETI_WARN("bms driver not found on i2c bus 3");
        }

        // UART Arribada
        // ToDo: ping arribada and expect response

        // UART GPS

        // SPI IMU

/* BEGIN ACQUISITION */
#ifdef BMS_ENABLED
    acq_battery_start();
#endif

#ifdef PRESSURE_ENABLED
    acq_pressure_start();
#endif

#ifdef GPS_ENABLE
    gps_start();
#endif

#ifdef IMU_ENABLED
    // acq_imu_start();
#endif //IMU_ENABLED

#ifdef ECG_ENABLED
    // acq_ecg_start();
#endif

#ifdef AUDIO_ENABLED
    acq_audio_start();
    led_heartbeat();
#endif // AUDIO_ENABLED

    while(1){
        mission_task();
    }

    // we should never get here 
    return -1;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
//  blink_interval_ms = BLINK_MOUNTED;
	led_blink(LED_YELLOW);
	CETI_LOG("USB mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
	led_on(LED_YELLOW);
	CETI_LOG("USB unmounted");
//  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
//  (void) remote_wakeup_en;
//  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
//  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}


// Generic Error Catcher; 
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    led_error();
  }
  /* USER CODE END Error_Handler_Debug */
}
