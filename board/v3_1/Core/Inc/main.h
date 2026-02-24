/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AUDIO_VP_EN_GPIO_Output_Pin GPIO_PIN_2
#define AUDIO_VP_EN_GPIO_Output_GPIO_Port GPIOE
#define Audio_VN_NEN_GPIO_Output_Pin GPIO_PIN_3
#define Audio_VN_NEN_GPIO_Output_GPIO_Port GPIOE
#define AUDIO_NCS_GPIO_Output_Pin GPIO_PIN_1
#define AUDIO_NCS_GPIO_Output_GPIO_Port GPIOH
#define BMS_I2C3_SCL_Pin GPIO_PIN_0
#define BMS_I2C3_SCL_GPIO_Port GPIOC
#define BMS_I2C3_SDA_Pin GPIO_PIN_1
#define BMS_I2C3_SDA_GPIO_Port GPIOC
#define OPTICAL_SPI2_MISO_Pin GPIO_PIN_2
#define OPTICAL_SPI2_MISO_GPIO_Port GPIOC
#define OPTICAL_SPI2_MOSI_Pin GPIO_PIN_3
#define OPTICAL_SPI2_MOSI_GPIO_Port GPIOC
#define BURNWIRE_EN_GPIO_Output_Pin GPIO_PIN_0
#define BURNWIRE_EN_GPIO_Output_GPIO_Port GPIOA
#define AUDIO_SPI1_SCK_Pin GPIO_PIN_1
#define AUDIO_SPI1_SCK_GPIO_Port GPIOA
#define SATELLITE_USART2_TX_Pin GPIO_PIN_2
#define SATELLITE_USART2_TX_GPIO_Port GPIOA
#define SATELLITE_USART2_RX_Pin GPIO_PIN_3
#define SATELLITE_USART2_RX_GPIO_Port GPIOA
#define IMU_NCS_GPIO_Output_Pin GPIO_PIN_4
#define IMU_NCS_GPIO_Output_GPIO_Port GPIOA
#define AUDIO_NRST_GPIO_Output_Pin GPIO_PIN_5
#define AUDIO_NRST_GPIO_Output_GPIO_Port GPIOA
#define AUDIO_SPI1_MISO_Pin GPIO_PIN_6
#define AUDIO_SPI1_MISO_GPIO_Port GPIOA
#define AUDIO_SPI1_MOSI_Pin GPIO_PIN_7
#define AUDIO_SPI1_MOSI_GPIO_Port GPIOA
#define IMU_PS0_GPIO_Output_Pin GPIO_PIN_0
#define IMU_PS0_GPIO_Output_GPIO_Port GPIOB
#define ECG_ADC_NDRDY_GPIO_Input_Pin GPIO_PIN_2
#define ECG_ADC_NDRDY_GPIO_Input_GPIO_Port GPIOB
#define ECG_ADC_NDRDY_GPIO_Input_EXTI_IRQn EXTI2_IRQn
#define GPS_PWR_EN_GPIO_Output_Pin GPIO_PIN_7
#define GPS_PWR_EN_GPIO_Output_GPIO_Port GPIOE
#define SAT_PWR_EN_GPIO_Output_Pin GPIO_PIN_8
#define SAT_PWR_EN_GPIO_Output_GPIO_Port GPIOE
#define GPS_SAFEBOOT_N_GPIO_Input_Pin GPIO_PIN_9
#define GPS_SAFEBOOT_N_GPIO_Input_GPIO_Port GPIOE
#define GPS_NRST_GPIO_Output_Pin GPIO_PIN_10
#define GPS_NRST_GPIO_Output_GPIO_Port GPIOE
#define DRY_GPIO_Analog_Pin GPIO_PIN_12
#define DRY_GPIO_Analog_GPIO_Port GPIOE
#define ECG_I2C2_SCL_Pin GPIO_PIN_10
#define ECG_I2C2_SCL_GPIO_Port GPIOB
#define SAT_NRST_GPIO_Output_Pin GPIO_PIN_11
#define SAT_NRST_GPIO_Output_GPIO_Port GPIOB
#define ECG_I2C2_SDA_Pin GPIO_PIN_14
#define ECG_I2C2_SDA_GPIO_Port GPIOB
#define ECG_NSD_GPIO_Output_Pin GPIO_PIN_8
#define ECG_NSD_GPIO_Output_GPIO_Port GPIOD
#define ECG_ADC_NRSET_GPIO_Output_Pin GPIO_PIN_9
#define ECG_ADC_NRSET_GPIO_Output_GPIO_Port GPIOD
#define IMU_NINT_GPIO_EXTI10_Pin GPIO_PIN_10
#define IMU_NINT_GPIO_EXTI10_GPIO_Port GPIOD
#define IMU_NINT_GPIO_EXTI10_EXTI_IRQn EXTI10_IRQn
#define IMU_NRESET_GPIO_Output_Pin GPIO_PIN_11
#define IMU_NRESET_GPIO_Output_GPIO_Port GPIOD
#define IFACE_EN_GPIO_Input_Pin GPIO_PIN_12
#define IFACE_EN_GPIO_Input_GPIO_Port GPIOD
#define FLASHER_LED_EN_GPIO_Output_Pin GPIO_PIN_7
#define FLASHER_LED_EN_GPIO_Output_GPIO_Port GPIOC
#define AUDIO_CLK_Pin GPIO_PIN_8
#define AUDIO_CLK_GPIO_Port GPIOA
#define GPS_USART1_RX_Pin GPIO_PIN_10
#define GPS_USART1_RX_GPIO_Port GPIOA
#define OPTICAL_SPI2_SCK_Pin GPIO_PIN_3
#define OPTICAL_SPI2_SCK_GPIO_Port GPIOD
#define ECG_LOD_P_GPIO_Input_Pin GPIO_PIN_4
#define ECG_LOD_P_GPIO_Input_GPIO_Port GPIOD
#define ECG_LOD_N_GPIO_Input_Pin GPIO_PIN_5
#define ECG_LOD_N_GPIO_Input_GPIO_Port GPIOD
#define SAT_RF_NRST_Pin GPIO_PIN_6
#define SAT_RF_NRST_GPIO_Port GPIOD
#define SAT_RF_BUSY_GPIO_Input_Pin GPIO_PIN_7
#define SAT_RF_BUSY_GPIO_Input_GPIO_Port GPIOD
#define SAT_PM_1_Pin GPIO_PIN_4
#define SAT_PM_1_GPIO_Port GPIOB
#define GPS_EXT_INT_GPIO_Input_Pin GPIO_PIN_5
#define GPS_EXT_INT_GPIO_Input_GPIO_Port GPIOB
#define GPS_USART1_TX_Pin GPIO_PIN_6
#define GPS_USART1_TX_GPIO_Port GPIOB
#define SENSOR_I2C_SDA_Pin GPIO_PIN_7
#define SENSOR_I2C_SDA_GPIO_Port GPIOB
#define SENSOR_I2C1_SCL_Pin GPIO_PIN_8
#define SENSOR_I2C1_SCL_GPIO_Port GPIOB
#define KELLER_DRDY_EXTI9_Pin GPIO_PIN_9
#define KELLER_DRDY_EXTI9_GPIO_Port GPIOB
#define KELLER_DRDY_EXTI9_EXTI_IRQn EXTI9_IRQn
#define SAT_PM_2_Pin GPIO_PIN_0
#define SAT_PM_2_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
#define AUDIO_hspi hspi1
#define IMU_hspi hspi1
#define BMS_hi2c hi2c3
#define ECG_hi2c hi2c2
#define KELLER_hi2c hi2c1

#define BATTERY_htim htim2
#define PRESSURE_htim htim3
#define uS_htim htim4
//#define FLASHER_htim htim5

#define GPS_huart huart1

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
