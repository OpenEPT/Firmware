/**
 ******************************************************************************
 * @file   	drv_ain.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGIN_H_
#define CORE_DRIVERS_PLATFORM_ANALOGIN_H_

#include <stdint.h>
#include "globalConfig.h"


#define DRV_AIN_ADC_BUFFER_MAX_SIZE	CONF_AIN_MAX_BUFFER_SIZE
#define DRV_AIN_ADC_BUFFER_OFFSET	CONF_AIN_ADC_BUFFER_OFFSET
#define DRV_AIN_ADC_BUFFER_MARKER	CONF_AIN_ADC_BUFFER_MARKER
#define DRV_AIN_ADC_BUFFER_NO		CONF_AIN_MAX_BUFFER_NO
#define DRV_AIN_ADC_TIM_INPUT_CLK	CONF_DRV_AIN_ADC_TIM_INPUT_CLK //MHZ
#define DRV_AIN_ADC_BUTTON_ISR_COMPLETED_BIT	5

typedef enum
{
	DRV_AIN_STATUS_OK,
	DRV_AIN_STATUS_ERROR
}drv_ain_status;

typedef enum
{
	DRV_AIN_ADC_1 = 0x01,
	DRV_AIN_ADC_2 = 0x02,
	DRV_AIN_ADC_3 = 0x04,
	DRV_AIN_ADC_ADS9224R = 0x08
}drv_ain_adc_t;

typedef enum
{
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_CONNECTED,
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_DISCONNECTED,
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_UKNOWN
}drv_ain_adc_dma_connection_status_t;

typedef enum
{
	DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE,
	DRV_AIN_ADC_ACQUISITION_STATUS_INACTIVE,
	DRV_AIN_ADC_ACQUISITION_STATUS_UKNOWN
}drv_ain_adc_acquisition_status_t;

typedef enum
{
	DRV_AIN_ADC_RESOLUTION_UKNOWN 	= 0,
	DRV_AIN_ADC_RESOLUTION_8BIT 	= 8,
	DRV_AIN_ADC_RESOLUTION_10BIT 	= 10,
	DRV_AIN_ADC_RESOLUTION_12BIT 	= 12,
	DRV_AIN_ADC_RESOLUTION_14BIT 	= 14,
	DRV_AIN_ADC_RESOLUTION_16BIT 	= 16
}drv_ain_adc_resolution_t;

typedef enum
{
	DRV_AIN_ADC_SAMPLE_TIME_UKNOWN	= 0,
	DRV_AIN_ADC_SAMPLE_TIME_1C5 	= 1,
	DRV_AIN_ADC_SAMPLE_TIME_2C5 	= 2,
	DRV_AIN_ADC_SAMPLE_TIME_8C5 	= 8,
	DRV_AIN_ADC_SAMPLE_TIME_16C5 	= 16,
	DRV_AIN_ADC_SAMPLE_TIME_32C5 	= 32,
	DRV_AIN_ADC_SAMPLE_TIME_64C5 	= 64,
	DRV_AIN_ADC_SAMPLE_TIME_387C5 	= 387,
	DRV_AIN_ADC_SAMPLE_TIME_810C5 	= 810
}drv_ain_adc_sample_time_t;
typedef enum
{
	DRV_AIN_ADC_CLOCK_DIV_UKNOWN	=	0,
	DRV_AIN_ADC_CLOCK_DIV_1			=	1,
	DRV_AIN_ADC_CLOCK_DIV_2			=	2,
	DRV_AIN_ADC_CLOCK_DIV_4			=	4,
	DRV_AIN_ADC_CLOCK_DIV_8			=	8,
	DRV_AIN_ADC_CLOCK_DIV_16		=	16,
	DRV_AIN_ADC_CLOCK_DIV_32		=	32,
	DRV_AIN_ADC_CLOCK_DIV_64		=	64,
	DRV_AIN_ADC_CLOCK_DIV_128		=	128,
	DRV_AIN_ADC_CLOCK_DIV_256		=	256
}drv_ain_adc_clock_div_t;

typedef enum{
	DRV_AIN_ADC_AVG_RATIO_UNDEFINED	= 0,
	DRV_AIN_ADC_AVG_RATIO_1 		= 1,
	DRV_AIN_ADC_AVG_RATIO_2 		= 2,
	DRV_AIN_ADC_AVG_RATIO_4 		= 4,
	DRV_AIN_ADC_AVG_RATIO_8 		= 8,
	DRV_AIN_ADC_AVG_RATIO_16 		= 16,
	DRV_AIN_ADC_AVG_RATIO_32 		= 32,
	DRV_AIN_ADC_AVG_RATIO_64 		= 64,
	DRV_AIN_ADC_AVG_RATIO_128 		= 128,
	DRV_AIN_ADC_AVG_RATIO_256 		= 256,
	DRV_AIN_ADC_AVG_RATIO_512 		= 512,
	DRV_AIN_ADC_AVG_RATIO_1024 		= 1024
}drv_adc_ch_avg_ratio_t;

typedef uint8_t	drv_ain_adc_channel_t;

typedef void (*drv_ain_adc_stream_callback)(uint32_t, uint8_t, uint32_t);

typedef struct
{
	drv_ain_adc_channel_t			channel;
	drv_ain_adc_sample_time_t		sampleTime;
	uint32_t						offset;
	drv_adc_ch_avg_ratio_t			avgRatio;
}drv_ain_adc_channel_config_t;
typedef struct
{
	drv_ain_adc_t					adc;
	drv_ain_adc_clock_div_t 		clockDiv;
	drv_ain_adc_resolution_t 		resolution;
	drv_ain_adc_channel_config_t 	ch1;
	drv_ain_adc_channel_config_t 	ch2;
	uint32_t						inputClk;
	uint32_t						samplingTime; // us
	uint32_t						prescaler;
	uint32_t						period; //nS
	uint32_t						samplesNo;
}drv_ain_adc_config_t;


drv_ain_status 						DRV_AIN_Init(drv_ain_adc_t adc, drv_ain_adc_config_t* configuration);
drv_ain_status 						DRV_AIN_Start(drv_ain_adc_t adc);
drv_ain_status 						DRV_AIN_Stop(drv_ain_adc_t adc);
drv_ain_status						DRV_AIN_SetSamplesNo(drv_ain_adc_t adc, uint32_t samplesNo);
drv_ain_adc_acquisition_status_t 	DRV_AIN_GetAcquisitonStatus(drv_ain_adc_t adc);
drv_ain_status 						DRV_AIN_SetResolution(drv_ain_adc_t adc, drv_ain_adc_resolution_t res);
drv_ain_status 						DRV_AIN_SetClockDiv(drv_ain_adc_t adc, drv_ain_adc_clock_div_t div);
drv_ain_status 						DRV_AIN_SetChannelsSamplingTime(drv_ain_adc_t adc, drv_ain_adc_sample_time_t stime);
drv_ain_status 						DRV_AIN_SetChannelOffset(drv_ain_adc_t adc, uint32_t channel,  uint32_t offset);
drv_ain_status 						DRV_AIN_SetChannelAvgRatio(drv_ain_adc_t adc, drv_adc_ch_avg_ratio_t avgRatio);
//Time in uS
drv_ain_status 						DRV_AIN_SetSamplingPeriod(drv_ain_adc_t adc, uint32_t period, uint32_t prescaller);
drv_ain_adc_resolution_t 			DRV_AIN_GetResolution(drv_ain_adc_t adc);
drv_ain_adc_sample_time_t 			DRV_AIN_GetSamplingTime(drv_ain_adc_t adc, drv_ain_adc_channel_t channel);
drv_ain_status 						DRV_AIN_GetADCClk(drv_ain_adc_t adc, uint32_t *clk);
drv_ain_status						DRV_AIN_GetADCValue(drv_ain_adc_t adc, drv_ain_adc_channel_t channel, uint32_t* value);

drv_ain_status 						DRV_AIN_Stream_Enable(drv_ain_adc_t adc, uint32_t sampleSize); // Enable DMA
drv_ain_status 						DRV_AIN_Stream_RegisterCallback(drv_ain_adc_t adc, drv_ain_adc_stream_callback cbfunction);
drv_ain_status 						DRV_AIN_Stream_SubmitAddr(drv_ain_adc_t adc, uint32_t addr, uint8_t bufferID);
drv_ain_status 						DRV_AIN_Stream_SetCapture(uint32_t* packetCounter);

#endif /* CORE_DRIVERS_PLATFORM_ANALOGIN_AIN_H_ */
