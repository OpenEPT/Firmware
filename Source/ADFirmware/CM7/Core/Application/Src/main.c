/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "drv_system.h"
#include "system.h"

#include "drv_gpio.h"


DAC_HandleTypeDef hdac1;

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

	if(DRV_SYSTEM_InitCoreFunc() != DRV_SYSTEM_STATUS_OK)
	{
		while(1);
	}
	if(SYSTEM_Init() != SYSTEM_STATUS_OK)
	{
		while(1);
	}
	if(SYSTEM_Start() != SYSTEM_STATUS_OK)
	{
		while(1);
	}

	while (1);
}
/**
-  * @brief  Period elapsed callback in non blocking mode
-  * @note   This function is called  when TIM6 interrupt took place, inside
-  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
-  * a global variable "uwTick" used as application time base.
-  * @param  htim : TIM handle
-  * @retval None
-  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

