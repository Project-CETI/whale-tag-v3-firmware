/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins
     PC14-OSC32_IN (PC14)   ------> RCC_OSC32_IN
     PC15-OSC32_OUT (PC15)   ------> RCC_OSC32_OUT
     PH0-OSC_IN (PH0)   ------> RCC_OSC_IN
     PH1-OSC_OUT (PH1)   ------> RCC_OSC_OUT
     PA13 (JTMS/SWDIO)   ------> DEBUG_JTMS-SWDIO
     PA14 (JTCK/SWCLK)   ------> DEBUG_JTCK-SWCLK
     PB3 (JTDO/TRACESWO)   ------> DEBUG_JTDO-SWO
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, AUDIO_VP_EN_GPIO_Output_Pin|GPS_PWR_EN_GPIO_Output_Pin|SAT_PWR_EN_GPIO_Output_Pin|GPS_NRST_GPIO_Output_Pin
                          |SAT_PM_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Audio_VN_NEN_GPIO_Output_GPIO_Port, Audio_VN_NEN_GPIO_Output_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BURNWIRE_EN_GPIO_Output_Pin|IMU_NCS_GPIO_Output_Pin|AUDIO_NRST_GPIO_Output_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(IMU_PS0_GPIO_Output_GPIO_Port, IMU_PS0_GPIO_Output_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, AUDIO_NCS_GPIO_Output_Pin|SAT_NRST_GPIO_Output_Pin|SAT_PM_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, ECG_NSD_GPIO_Output_Pin|ECG_ADC_NRSET_GPIO_Output_Pin|IMU_NRESET_GPIO_Output_Pin|SAT_RF_NRST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FLASHER_LED_EN_GPIO_Output_GPIO_Port, FLASHER_LED_EN_GPIO_Output_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : AUDIO_VP_EN_GPIO_Output_Pin Audio_VN_NEN_GPIO_Output_Pin GPS_PWR_EN_GPIO_Output_Pin SAT_PWR_EN_GPIO_Output_Pin
                           GPS_NRST_GPIO_Output_Pin SAT_PM_2_Pin */
  GPIO_InitStruct.Pin = AUDIO_VP_EN_GPIO_Output_Pin|Audio_VN_NEN_GPIO_Output_Pin|GPS_PWR_EN_GPIO_Output_Pin|SAT_PWR_EN_GPIO_Output_Pin
                          |GPS_NRST_GPIO_Output_Pin|SAT_PM_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PC13 PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : BURNWIRE_EN_GPIO_Output_Pin IMU_NCS_GPIO_Output_Pin AUDIO_NRST_GPIO_Output_Pin */
  GPIO_InitStruct.Pin = BURNWIRE_EN_GPIO_Output_Pin|IMU_NCS_GPIO_Output_Pin|AUDIO_NRST_GPIO_Output_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : IMU_PS0_GPIO_Output_Pin AUDIO_NCS_GPIO_Output_Pin SAT_NRST_GPIO_Output_Pin SAT_PM_1_Pin */
  GPIO_InitStruct.Pin = IMU_PS0_GPIO_Output_Pin|AUDIO_NCS_GPIO_Output_Pin|SAT_NRST_GPIO_Output_Pin|SAT_PM_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : ECG_ADC_NDRDY_GPIO_Input_Pin */
  GPIO_InitStruct.Pin = ECG_ADC_NDRDY_GPIO_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ECG_ADC_NDRDY_GPIO_Input_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : GPS_SAFEBOOT_N_GPIO_Input_Pin */
  GPIO_InitStruct.Pin = GPS_SAFEBOOT_N_GPIO_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPS_SAFEBOOT_N_GPIO_Input_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PE11 DRY_GPIO_Analog_Pin PE13 PE14
                           PE15 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|DRY_GPIO_Analog_Pin|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PB13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : ECG_NSD_GPIO_Output_Pin ECG_ADC_NRSET_GPIO_Output_Pin IMU_NRESET_GPIO_Output_Pin SAT_RF_NRST_Pin */
  GPIO_InitStruct.Pin = ECG_NSD_GPIO_Output_Pin|ECG_ADC_NRSET_GPIO_Output_Pin|IMU_NRESET_GPIO_Output_Pin|SAT_RF_NRST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : IMU_NINT_GPIO_EXTI10_Pin */
  GPIO_InitStruct.Pin = IMU_NINT_GPIO_EXTI10_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IMU_NINT_GPIO_EXTI10_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : IFACE_EN_GPIO_Input_Pin ECG_LOD_P_GPIO_Input_Pin ECG_LOD_N_GPIO_Input_Pin SAT_RF_BUSY_GPIO_Input_Pin */
  GPIO_InitStruct.Pin = IFACE_EN_GPIO_Input_Pin|ECG_LOD_P_GPIO_Input_Pin|ECG_LOD_N_GPIO_Input_Pin|SAT_RF_BUSY_GPIO_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PD13 PD14 PD15 PD1 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : FLASHER_LED_EN_GPIO_Output_Pin */
  GPIO_InitStruct.Pin = FLASHER_LED_EN_GPIO_Output_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(FLASHER_LED_EN_GPIO_Output_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA9 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : GPS_EXT_INT_GPIO_Input_Pin */
  GPIO_InitStruct.Pin = GPS_EXT_INT_GPIO_Input_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPS_EXT_INT_GPIO_Input_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PH3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : KELLER_DRDY_EXTI9_Pin */
  GPIO_InitStruct.Pin = KELLER_DRDY_EXTI9_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(KELLER_DRDY_EXTI9_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI9_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_IRQn);

  HAL_NVIC_SetPriority(EXTI10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
