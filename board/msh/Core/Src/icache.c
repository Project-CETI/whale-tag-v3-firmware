/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "icache.h"

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/* ICACHE init function */
void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */
  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */
  /* USER CODE END ICACHE_Init 1 */

  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ICACHE_Init 2 */
  /* USER CODE END ICACHE_Init 2 */

}

/* USER CODE BEGIN 1 */
/* USER CODE END 1 */
