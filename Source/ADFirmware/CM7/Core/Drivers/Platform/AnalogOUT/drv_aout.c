/**
 ******************************************************************************
 * @file    drv_aout.c
 * @brief   Analog Output driver implementation This file contains the 
 *          implementation of the Analog Output driver for STM32H7 microcontrollers.
 *          It provides functionality for controlling the DAC peripheral.
 *
 * @author  Haris
 * @email   haris.turkmanovic@gmail.com
 * @date    Nov 5, 2023
 ******************************************************************************
 */

#include "drv_aout.h"
#include "stm32h7xx_hal.h"
#include "DAC6578/dac6578.h"
#include "drv_i2c.h"

#define DRV_AOUT_WAVE_MAX_POINTS              2048U
#define DRV_AOUT_WAVE_FRAME_COUNT              (DRV_AOUT_WAVE_MAX_POINTS + 1U)
#define DRV_AOUT_WAVE_TIMER_FREQUENCY_HZ      10000U
#define DRV_AOUT_WAVE_TIMER_TICK_US           100U
#define DRV_AOUT_WAVE_TIMER_MAX_TICKS         0x10000U
#define DRV_AOUT_WAVE_STOP_TIMEOUT            10U
/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AOUT_DRIVER AOUT Driver
 * @{
 */

/**
 * @defgroup AOUT_PRIVATE_DATA AOUT driver private data
 * @{
 */
/** @brief DAC handle for hardware access */
static DAC_HandleTypeDef 		prvDRV_AOUT_DAC_HANDLER;

/** @brief Status of the DAC output (enabled/disabled) */
static drv_aout_active_status_t 	prvDRV_AOUT_DAC_ACTIVE_STATUS;

/** @brief Timer used for waveform scheduling */
static TIM_HandleTypeDef prvDRV_AOUT_WAVE_TIMER;

/** @brief Configured waveform points */
static drv_aout_wave_point_t prvDRV_AOUT_WAVE_POINTS[DRV_AOUT_WAVE_MAX_POINTS];

__attribute__((section(".DMABuffer"), aligned(32)))
/** @brief Pre-serialized DAC6578 frames */
static dac6578_frame_t prvDRV_AOUT_WAVE_FRAMES[DRV_AOUT_WAVE_FRAME_COUNT];

/** @brief Number of configured waveform points */
static uint32_t prvDRV_AOUT_WAVE_LENGTH;

/** @brief Current waveform point index */
static volatile uint32_t prvDRV_AOUT_WAVE_INDEX;

/** @brief Waveform execution state */
static volatile uint8_t prvDRV_AOUT_WAVE_RUNNING;

/** @brief Channel used by the configured waveform */
static drv_aout_channel_t prvDRV_AOUT_WAVE_CHANNEL;

/** @brief Configured waveform repetition count */
static uint32_t prvDRV_AOUT_WAVE_REPETITION;

/** @brief Current waveform repetition */
static volatile uint32_t prvDRV_AOUT_WAVE_REPETITION_INDEX;

/** @brief Waveform complete callback */
static drv_aout_wave_complete_callback_t prvDRV_AOUT_WAVE_COMPLETE_CALLBACK;

static volatile uint32_t prvDRV_AOUT_WAVE_STOP_ABORT_COUNTER = 0U;

static volatile uint32_t prvDRV_AOUT_WAVE_REMAINING_TICKS = 0U;

static drv_aout_wave_point_callback_t prvDRV_AOUT_WAVE_POINT_CALLBACK = NULL;

static void prvDRV_AOUT_WaveReportTag(uint32_t tag)
{
    if((tag != 0U) && (prvDRV_AOUT_WAVE_POINT_CALLBACK != NULL))
    {
        prvDRV_AOUT_WAVE_POINT_CALLBACK(tag);
    }
}




void DRV_AOUT_WaveDMACompleteCallback(void)
{
    if((prvDRV_AOUT_WAVE_RUNNING != 0U) &&
       (prvDRV_AOUT_WAVE_INDEX >= prvDRV_AOUT_WAVE_LENGTH))
    {
        prvDRV_AOUT_WAVE_RUNNING = 0U;
        prvDRV_AOUT_WAVE_INDEX = 0U;
        prvDRV_AOUT_WAVE_REPETITION_INDEX = 0U;

        if(prvDRV_AOUT_WAVE_COMPLETE_CALLBACK != NULL)
        {
            prvDRV_AOUT_WAVE_COMPLETE_CALLBACK();
        }
    }
}

/**
 * @}
 */
void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(hdac->Instance==DAC1)
	{
		/* USER CODE BEGIN DAC1_MspInit 0 */

		/* USER CODE END DAC1_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_DAC12_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**DAC1 GPIO Configuration
		PA5     ------> DAC1_OUT2
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* DAC1 interrupt Init */
		HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 15, 0);
		HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	}

}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef* hdac)
{
	if(hdac->Instance==DAC1)
	{
		/* USER CODE BEGIN DAC1_MspDeInit 0 */

		/* USER CODE END DAC1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_DAC12_CLK_DISABLE();

		/**DAC1 GPIO Configuration
		PA5     ------> DAC1_OUT2
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5);

		/* DAC1 interrupt DeInit */
		HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
		/* USER CODE BEGIN DAC1_MspDeInit 1 */

		/* USER CODE END DAC1_MspDeInit 1 */
	}

}

/**
 * @defgroup AOUT_PRIVATE_FUNCTIONS AOUT driver private functions
 * @{
 */

/**
 * @brief   Internal DAC initialization
 * @details This private function initializes the DAC hardware and configures channel 2
 *          with default settings.
 *
 * @return  DRV_AOUT_STATUS_OK if successful, DRV_AOUT_STATUS_ERROR otherwise
 */
/**
 * @brief Initialize waveform scheduler timer
 * @retval ::drv_aout_status_t
 */
static drv_aout_status_t prvDRV_AOUT_WaveTimerInit(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    __HAL_RCC_TIM7_CLK_ENABLE();

    prvDRV_AOUT_WAVE_TIMER.Instance = TIM7;
    prvDRV_AOUT_WAVE_TIMER.Init.Prescaler = 20000U - 1U;
    prvDRV_AOUT_WAVE_TIMER.Init.CounterMode = TIM_COUNTERMODE_UP;
    prvDRV_AOUT_WAVE_TIMER.Init.Period = 1U;
    prvDRV_AOUT_WAVE_TIMER.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if(HAL_TIM_Base_Init(&prvDRV_AOUT_WAVE_TIMER) != HAL_OK)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if(HAL_TIMEx_MasterConfigSynchronization(&prvDRV_AOUT_WAVE_TIMER, &sMasterConfig) != HAL_OK)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    HAL_NVIC_SetPriority(TIM7_IRQn, 15U, 0U);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);

    return DRV_AOUT_STATUS_OK;
}

static uint8_t prvDRV_AOUT_WaveProgramNextSegment(void)
{
    uint32_t segmentTicks;

    if(prvDRV_AOUT_WAVE_REMAINING_TICKS == 0U)
    {
        return 0U;
    }

    segmentTicks = prvDRV_AOUT_WAVE_REMAINING_TICKS;
    if(segmentTicks > DRV_AOUT_WAVE_TIMER_MAX_TICKS)
    {
        segmentTicks = DRV_AOUT_WAVE_TIMER_MAX_TICKS;
    }
    prvDRV_AOUT_WAVE_REMAINING_TICKS -= segmentTicks;

    __HAL_TIM_SET_AUTORELOAD(&prvDRV_AOUT_WAVE_TIMER, segmentTicks - 1U);
    __HAL_TIM_SET_COUNTER(&prvDRV_AOUT_WAVE_TIMER, 0U);

    return 1U;
}

static drv_aout_status_t prvDRV_AOUT_WaveSetDuration(uint32_t duration)
{
    if(duration == 0U)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    prvDRV_AOUT_WAVE_REMAINING_TICKS = (duration + DRV_AOUT_WAVE_TIMER_TICK_US - 1U) / DRV_AOUT_WAVE_TIMER_TICK_US;

    if(prvDRV_AOUT_WaveProgramNextSegment() == 0U)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    return DRV_AOUT_STATUS_OK;
}

static drv_aout_status_t prvDRV_AOUT_Init_Internal()
{
	DAC_ChannelConfTypeDef sConfig = {0};

	prvDRV_AOUT_DAC_HANDLER.Instance = DAC1;
	if (HAL_DAC_Init(&prvDRV_AOUT_DAC_HANDLER) != HAL_OK) return DRV_AOUT_STATUS_ERROR;

	/** DAC channel OUT2 config	*/
	sConfig.DAC_SampleAndHold	 = DAC_SAMPLEANDHOLD_DISABLE;
	sConfig.DAC_Trigger			 = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer	 = DAC_OUTPUTBUFFER_ENABLE;
	sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
	sConfig.DAC_UserTrimming	 = DAC_TRIMMING_FACTORY;
	if (HAL_DAC_ConfigChannel(&prvDRV_AOUT_DAC_HANDLER, &sConfig, DAC_CHANNEL_2) != HAL_OK) return DRV_AOUT_STATUS_ERROR;



	return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_Init()
{
	/*Set internal DAC*/
	if(prvDRV_AOUT_Init_Internal() != DRV_AOUT_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_DISABLED;
	if(HAL_DAC_SetValue(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0) != HAL_OK) return DRV_AOUT_STATUS_ERROR;
	HAL_DAC_Stop(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);

	/*Set external DAC*/
	if(DAC6578_Init() != DAC6578_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	if(DAC6578_Reset(1000) != DAC6578_STATUS_OK) return DRV_AOUT_STATUS_ERROR;
	if(prvDRV_AOUT_WaveTimerInit() != DRV_AOUT_STATUS_OK) return DRV_AOUT_STATUS_ERROR;

	if(DRV_I2C_RegisterTxDMACompleteCallback(DRV_I2C_INSTANCE_2, DRV_AOUT_WaveDMACompleteCallback) != DRV_I2C_STATUS_OK)
	{
	    return DRV_AOUT_STATUS_ERROR;
	}
	prvDRV_AOUT_WAVE_LENGTH = 0U;
	prvDRV_AOUT_WAVE_INDEX = 0U;
	prvDRV_AOUT_WAVE_RUNNING = 0U;

	prvDRV_AOUT_WAVE_COMPLETE_CALLBACK = NULL;
	return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_SetEnable(drv_aout_active_status_t aStatus)
{
	switch(aStatus)
	{
	case DRV_AOUT_ACTIVE_STATUS_DISABLED:
		prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_DISABLED;
		HAL_DAC_Stop(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);
		DAC6578_SetChannelState(DRV_AOUT_CHANNEL_D, DAC6578_CHANNEL_DISABLED, 1000);
		break;
	case DRV_AOUT_ACTIVE_STATUS_ENABLED:
		prvDRV_AOUT_DAC_ACTIVE_STATUS = DRV_AOUT_ACTIVE_STATUS_ENABLED;
		HAL_DAC_Start(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2);
		DAC6578_SetChannelState(DRV_AOUT_CHANNEL_D, DAC6578_CHANNEL_ENABLED, 1000);
		break;
	}
	return DRV_AOUT_STATUS_OK;
}
drv_aout_status_t DRV_AOUT_SetValue(uint32_t value, drv_aout_channel_t channel)
{
	uint8_t returnValue = 0;
	switch(channel)
	{
	case DRV_AOUT_CHANNEL_A:
		if(DAC6578_SetAndUpdateChannelValue(DRV_AOUT_CHANNEL_A, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_B:
		if(DAC6578_SetAndUpdateChannelValue(DRV_AOUT_CHANNEL_B, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_C:
		if(DAC6578_SetAndUpdateChannelValue(DRV_AOUT_CHANNEL_C, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
		break;
	case DRV_AOUT_CHANNEL_D:
		if(DAC6578_SetAndUpdateChannelValue(DRV_AOUT_CHANNEL_D, value, 1000) != DAC6578_STATUS_OK)
		{
			returnValue = 1;
		}
//		if(HAL_DAC_SetValue(&prvDRV_AOUT_DAC_HANDLER, DAC_CHANNEL_2, DAC_ALIGN_12B_R, value) != HAL_OK) return DRV_AOUT_STATUS_ERROR;
		break;
	case DRV_AOUT_CHANNEL_E:
	case DRV_AOUT_CHANNEL_F:
	case DRV_AOUT_CHANNEL_G:
	case DRV_AOUT_CHANNEL_H:
		returnValue = 1;
		break;
	}
	return returnValue > 0 ? DRV_AOUT_STATUS_ERROR : DRV_AOUT_STATUS_OK;
}
drv_aout_status_t DRV_AOUT_SetVoltage(float voltage, drv_aout_channel_t channel)
{
    uint16_t dval = DRV_AOUT_ConvertFloatToDigital(voltage);
    return DRV_AOUT_SetValue(dval, channel);
}
uint16_t DRV_AOUT_ConvertFloatToDigital(float value)
{
    return DAC6578_FLOAT_TO_DVALUE(value);
}
drv_aout_status_t DRV_AOUT_WaveConfigure(drv_aout_channel_t channel, const drv_aout_wave_point_t* wave, uint32_t waveLength, uint32_t repetition)
{
    uint32_t i;

    if((wave == NULL) ||
       (waveLength == 0U) ||
       (waveLength > DRV_AOUT_WAVE_MAX_POINTS) ||
       (repetition == 0U))
    {
        return DRV_AOUT_STATUS_ERROR;
    }

//    if(channel >= DRV_AOUT_CHANNEL_MAX)
//    {
//        return DRV_AOUT_STATUS_ERROR;
//    }

    if(prvDRV_AOUT_WAVE_RUNNING != 0U)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    for(i = 0U; i < waveLength; i++)
    {
        if((wave[i].value > DAC6578_MAX_VALUE) ||
           (wave[i].duration == 0U))
        {
            return DRV_AOUT_STATUS_ERROR;
        }

        prvDRV_AOUT_WAVE_POINTS[i] = wave[i];

        if(DAC6578_SerializeSetAndUpdate(channel, wave[i].value, &prvDRV_AOUT_WAVE_FRAMES[i]) != DAC6578_STATUS_OK)
        {
            return DRV_AOUT_STATUS_ERROR;
        }
    }

    if(DAC6578_SerializeSetAndUpdate(channel, 0U, &prvDRV_AOUT_WAVE_FRAMES[waveLength]) != DAC6578_STATUS_OK)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    prvDRV_AOUT_WAVE_CHANNEL = channel;
    prvDRV_AOUT_WAVE_LENGTH = waveLength;
    prvDRV_AOUT_WAVE_REPETITION = repetition;
    prvDRV_AOUT_WAVE_INDEX = 0U;
    prvDRV_AOUT_WAVE_REPETITION_INDEX = 0U;

    return DRV_AOUT_STATUS_OK;
}


drv_aout_status_t DRV_AOUT_WaveStart(void)
{
	if((prvDRV_AOUT_WAVE_LENGTH == 0U) ||
	   (prvDRV_AOUT_WAVE_RUNNING != 0U))
	{
	    return DRV_AOUT_STATUS_ERROR;
	}

    prvDRV_AOUT_WAVE_INDEX = 0U;

    if(DRV_AOUT_SetValue(prvDRV_AOUT_WAVE_POINTS[0].value, prvDRV_AOUT_WAVE_CHANNEL) != DRV_AOUT_STATUS_OK)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    if(prvDRV_AOUT_WaveSetDuration(prvDRV_AOUT_WAVE_POINTS[0].duration) != DRV_AOUT_STATUS_OK)
    {
        (void)DAC6578_AbortDMA();
        return DRV_AOUT_STATUS_ERROR;
    }

    __HAL_TIM_CLEAR_FLAG(&prvDRV_AOUT_WAVE_TIMER, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE);

    prvDRV_AOUT_WAVE_RUNNING = 1U;

    prvDRV_AOUT_WaveReportTag(prvDRV_AOUT_WAVE_POINTS[0].startTag);

    if(HAL_TIM_Base_Start(&prvDRV_AOUT_WAVE_TIMER) != HAL_OK)
    {
        __HAL_TIM_DISABLE_IT(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE);
        (void)DAC6578_AbortDMA();
        prvDRV_AOUT_WAVE_RUNNING = 0U;
        return DRV_AOUT_STATUS_ERROR;
    }

    return DRV_AOUT_STATUS_OK;
}
static volatile uint32_t prvDRV_AOUT_DMA_ERROR_COUNTER = 0U;
void TIM7_IRQHandler(void)
{
    if((__HAL_TIM_GET_FLAG(&prvDRV_AOUT_WAVE_TIMER, TIM_FLAG_UPDATE) != RESET) &&
       (__HAL_TIM_GET_IT_SOURCE(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE) != RESET))
    {
        __HAL_TIM_CLEAR_IT(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE);

        if(prvDRV_AOUT_WAVE_RUNNING == 0U)
        {
            return;
        }

        if(prvDRV_AOUT_WaveProgramNextSegment() != 0U)
        {
            return;
        }

        prvDRV_AOUT_WaveReportTag(prvDRV_AOUT_WAVE_POINTS[prvDRV_AOUT_WAVE_INDEX].endTag);

        prvDRV_AOUT_WAVE_INDEX++;

        if(prvDRV_AOUT_WAVE_INDEX < prvDRV_AOUT_WAVE_LENGTH)
        {
        	if(DAC6578_TransmitFrameDMA(&prvDRV_AOUT_WAVE_FRAMES[prvDRV_AOUT_WAVE_INDEX]) != DAC6578_STATUS_OK)
        	{
        	    prvDRV_AOUT_DMA_ERROR_COUNTER++;
        	}
            prvDRV_AOUT_WaveReportTag(prvDRV_AOUT_WAVE_POINTS[prvDRV_AOUT_WAVE_INDEX].startTag);
            (void)prvDRV_AOUT_WaveSetDuration(prvDRV_AOUT_WAVE_POINTS[prvDRV_AOUT_WAVE_INDEX].duration);
        }
        else
        {
            prvDRV_AOUT_WAVE_REPETITION_INDEX++;

            if(prvDRV_AOUT_WAVE_REPETITION_INDEX < prvDRV_AOUT_WAVE_REPETITION)
            {
                prvDRV_AOUT_WAVE_INDEX = 0U;

                if(DAC6578_TransmitFrameDMA(&prvDRV_AOUT_WAVE_FRAMES[prvDRV_AOUT_WAVE_INDEX]) != DAC6578_STATUS_OK)
                {
                    prvDRV_AOUT_DMA_ERROR_COUNTER++;
                }
                prvDRV_AOUT_WaveReportTag(prvDRV_AOUT_WAVE_POINTS[0].startTag);
                (void)prvDRV_AOUT_WaveSetDuration(prvDRV_AOUT_WAVE_POINTS[0].duration);
            }
            else
            {
                __HAL_TIM_DISABLE_IT(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE);

                (void)HAL_TIM_Base_Stop(&prvDRV_AOUT_WAVE_TIMER);

                __HAL_TIM_SET_COUNTER(&prvDRV_AOUT_WAVE_TIMER, 0U);

                if(DAC6578_TransmitFrameDMA(&prvDRV_AOUT_WAVE_FRAMES[prvDRV_AOUT_WAVE_LENGTH]) != DAC6578_STATUS_OK)
                {
                    prvDRV_AOUT_WAVE_RUNNING = 0U;
                    prvDRV_AOUT_WAVE_INDEX = 0U;
                    prvDRV_AOUT_WAVE_REPETITION_INDEX = 0U;
                }
            }
        }
    }
}



drv_aout_status_t DRV_AOUT_WaveStop(void)
{
    drv_aout_status_t status = DRV_AOUT_STATUS_OK;

    prvDRV_AOUT_WAVE_RUNNING = 0U;

    __HAL_TIM_DISABLE_IT(&prvDRV_AOUT_WAVE_TIMER, TIM_IT_UPDATE);

    if(HAL_TIM_Base_Stop(&prvDRV_AOUT_WAVE_TIMER) != HAL_OK)
    {
        status = DRV_AOUT_STATUS_ERROR;
    }

    if(DAC6578_WaitIdle(DRV_AOUT_WAVE_STOP_TIMEOUT) != DAC6578_STATUS_OK)
    {
        prvDRV_AOUT_WAVE_STOP_ABORT_COUNTER++;

        if(DAC6578_AbortDMA() != DAC6578_STATUS_OK)
        {
            status = DRV_AOUT_STATUS_ERROR;
        }
    }

    prvDRV_AOUT_WAVE_INDEX = 0U;
    prvDRV_AOUT_WAVE_REPETITION_INDEX = 0U;
    prvDRV_AOUT_WAVE_REMAINING_TICKS = 0U;
    __HAL_TIM_SET_COUNTER(&prvDRV_AOUT_WAVE_TIMER, 0U);

    return status;
}

drv_aout_status_t DRV_AOUT_WaveRegisterPointCallback(drv_aout_wave_point_callback_t callback)
{
    prvDRV_AOUT_WAVE_POINT_CALLBACK = callback;
    return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_WaveGetStopAbortCounter(uint32_t* counter)
{
    if(counter == NULL)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    *counter = prvDRV_AOUT_WAVE_STOP_ABORT_COUNTER;

    return DRV_AOUT_STATUS_OK;
}
drv_aout_status_t DRV_AOUT_WaveRegisterCompleteCallback(drv_aout_wave_complete_callback_t callback)
{
    prvDRV_AOUT_WAVE_COMPLETE_CALLBACK = callback;

    return DRV_AOUT_STATUS_OK;
}

drv_aout_status_t DRV_AOUT_WaveClear(void)
{
    if(prvDRV_AOUT_WAVE_RUNNING != 0U)
    {
        return DRV_AOUT_STATUS_ERROR;
    }

    memset(prvDRV_AOUT_WAVE_POINTS, 0, sizeof(prvDRV_AOUT_WAVE_POINTS));
    memset(prvDRV_AOUT_WAVE_FRAMES, 0, sizeof(prvDRV_AOUT_WAVE_FRAMES));

    prvDRV_AOUT_WAVE_LENGTH = 0U;
    prvDRV_AOUT_WAVE_INDEX = 0U;
    prvDRV_AOUT_WAVE_REPETITION = 0U;
    prvDRV_AOUT_WAVE_REPETITION_INDEX = 0U;

    return DRV_AOUT_STATUS_OK;
}

/**
 * @}
 */
/**
 * @}
 */
/**
 * @}
 */

