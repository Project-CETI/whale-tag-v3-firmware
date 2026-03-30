/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : linked_list.c
  * Description        : This file provides code for the configuration
  *                      of the LinkedList.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "linked_list.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

DMA_NodeTypeDef Node1;
DMA_QListTypeDef YourQueueName;
DMA_NodeTypeDef Node2;

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/**
  * @brief  DMA Linked-list YourQueueName configuration
  * @param  None
  * @retval None
  */
HAL_StatusTypeDef MX_YourQueueName_Config(void)
{
  HAL_StatusTypeDef ret = HAL_OK;
  /* DMA node configuration declaration */
  DMA_NodeConfTypeDef pNodeConfig;

  /* Set node configuration ################################################*/
  pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
  pNodeConfig.Init.Request = GPDMA1_REQUEST_SAI1_A;
  pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
  pNodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
  pNodeConfig.Init.SrcInc = DMA_SINC_FIXED;
  pNodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
  pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
  pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
  pNodeConfig.Init.SrcBurstLength = 1;
  pNodeConfig.Init.DestBurstLength = 1;
  pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
  pNodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
  pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  pNodeConfig.SrcAddress = 0;
  // pNodeConfig.DstAddress = test_buffer[0];
  pNodeConfig.DataSize = 0xFFF0;

  /* Build Node1 Node */
  ret |= HAL_DMAEx_List_BuildNode(&pNodeConfig, &Node1);

  /* Insert Node1 to Queue */
  ret |= HAL_DMAEx_List_InsertNode_Tail(&YourQueueName, &Node1);

  /* Set node configuration ################################################*/
  pNodeConfig.Init.Request = DMA_REQUEST_SW;
  pNodeConfig.Init.Direction = DMA_MEMORY_TO_MEMORY;
  pNodeConfig.Init.DestInc = DMA_DINC_FIXED;
  // pNodeConfig.DstAddress = test_buffer[1];
  pNodeConfig.DataSize = 0xFFFF;

  /* Build Node2 Node */
  ret |= HAL_DMAEx_List_BuildNode(&pNodeConfig, &Node2);

  /* Insert Node2 to Queue */
  ret |= HAL_DMAEx_List_InsertNode_Tail(&YourQueueName, &Node2);

   return ret;
}

