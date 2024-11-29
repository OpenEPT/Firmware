/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_it.c
  * @brief   Interrupt Service Routines.
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern ETH_HandleTypeDef HETH;
extern DMA_HandleTypeDef hdma_adc1;
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim6;
extern SPI_HandleTypeDef prvADS9224R_SPI_S_SDOB_HANDLER;
extern SPI_HandleTypeDef prvADS9224R_SPI_S_SDOA_HANDLER;

/* USER CODE BEGIN EV */
/**
  * @brief This function handles USART1 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  //HAL_UART_IRQHandler(&HUART3);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}
/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/


/**
  * @brief This function handles TIM6 global interrupt, DAC1_CH1 and DAC1_CH2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
	if (hdac1.State != HAL_DAC_STATE_RESET) {
		HAL_DAC_IRQHandler(&hdac1);
	}
	HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/**
  * @brief This function handles Ethernet global interrupt.
  */
void ETH_IRQHandler(void)
{
	/* USER CODE BEGIN ETH_IRQn 0 */

	/* USER CODE END ETH_IRQn 0 */
	HAL_ETH_IRQHandler(&HETH);
	/* USER CODE BEGIN ETH_IRQn 1 */

	/* USER CODE END ETH_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE10))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE10);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE11))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE11);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE12))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE12);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE13))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE13);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE14))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE14);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE15))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE15);
	}
}
/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE5))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE5);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE6))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE6);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE7))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE7);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE8))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE8);
	}
	if (__HAL_GPIO_EXTI_GET_IT(0x1 << EXTI_LINE9))
	{
		HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
		__HAL_GPIO_EXTI_CLEAR_IT(0x1 << EXTI_LINE9);
	}
}
void EXTI0_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}
void EXTI1_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}
void EXTI2_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}
void EXTI3_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);
}
void EXTI4_IRQHandler(void)
{
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4);
}

