/**
 ******************************************************************************
 * @file   	sstream.h
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_
#define CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_

#include <stdint.h>
#include "globalConfig.h"

#define 	SSTREAM_CONTROL_TASK_NAME					CONF_SSTREAM_CONTROL_TASK_NAME
#define 	SSTREAM_CONTROL_TASK_PRIO					CONF_SSTREAM_CONTROL_TASK_PRIO
#define 	SSTREAM_CONTROL_TASK_STACK_SIZE				CONF_SSTREAM_CONTROL_TASK_STACK_SIZE
#define 	SSTREAM_STREAM_TASK_NAME					CONF_SSTREAM_STREAM_TASK_NAME
#define 	SSTREAM_STREAM_TASK_PRIO					CONF_SSTREAM_STREAM_TASK_PRIO
#define 	SSTREAM_STREAM_TASK_STACK_SIZE				CONF_SSTREAM_STREAM_TASK_STACK_SIZE

#define 	SSTREAM_CONNECTIONS_MAX_NO					CONF_SSTREAM_CONNECTIONS_MAX_NO

#define		SSTREAM_AIN_DEFAULT_RESOLUTION				CONF_SSTREAM_AIN_DEFAULT_RESOLUTION
#define		SSTREAM_AIN_DEFAULT_CLOCK_DIV				CONF_SSTREAM_AIN_DEFAULT_CLOCK_DIV
#define		SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME			CONF_SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME
#define		SSTREAM_AIN_DEFAULT_CH_AVG_RATIO			CONF_SSTREAM_AIN_DEFAULT_CH_AVG_RATIO
#define		SSTREAM_AIN_DEFAULT_SAMPLE_TIME				CONF_SSTREAM_AIN_DEFAULT_SAMPLE_TIME
#define		SSTREAM_AIN_DEFAULT_PRESCALER				CONF_SSTREAM_AIN_DEFAULT_PRESCALER
#define		SSTREAM_AIN_DEFAULT_PERIOD					CONF_SSTREAM_AIN_DEFAULT_PERIOD
#define 	SSTREAM_AIN_VOLTAGE_CHANNEL					CONF_SSTREAM_AIN_VOLTAGE_CHANNEL
#define 	SSTREAM_AIN_CURRENT_CHANNEL					CONF_SSTREAM_AIN_CURRENT_CHANNEL

#define		SSTREAM_LED_PORT							CONF_SSTREAM_LED_PORT
#define		SSTREAM_LED_PIN								CONF_SSTREAM_LED_PIN

/**
 * @brief Samples stream service task state
 */
typedef enum
{
	SSTREAM_STATE_INIT,						/*!< Samples stream service initialization state */
	SSTREAM_STATE_SERVICE,					/*!< Samples stream service is in service state */
	SSTREAM_STATE_ERROR						/*!< Samples stream service is in error state */
}sstream_state_t;

typedef enum
{
	SSTREAM_ADC_INTERNAL = 0,
	SSTREAM_ADC_EXTERNAL
}sstream_adc_t;

typedef enum{
	SSTREAM_STATUS_OK,
	SSTREAM_STATUS_ERROR
}sstream_status_t;

typedef enum{
	SSTREAM_ADC_RESOLUTION_16BIT = 16,
	SSTREAM_ADC_RESOLUTION_14BIT = 14,
	SSTREAM_ADC_RESOLUTION_12BIT = 12,
	SSTREAM_ADC_RESOLUTION_10BIT = 10
}sstream_adc_resolution_t;

typedef enum{
	SSTREAM_ADC_SAMPLING_TIME_1C5 = 1,
	SSTREAM_ADC_SAMPLING_TIME_2C5 = 2,
	SSTREAM_ADC_SAMPLING_TIME_8C5 = 8,
	SSTREAM_ADC_SAMPLING_TIME_16C5 = 16,
	SSTREAM_ADC_SAMPLING_TIME_32C5 = 32,
	SSTREAM_ADC_SAMPLING_TIME_64C5 = 64,
	SSTREAM_ADC_SAMPLING_TIME_387C5 = 387,
	SSTREAM_ADC_SAMPLING_TIME_810C5 = 810
}sstream_adc_sampling_time_t;

typedef enum{
	SSTREAM_ADC_CLK_DIV_1 			= 1,
	SSTREAM_ADC_CLK_DIV_2 			= 2,
	SSTREAM_ADC_CLK_DIV_4 			= 4,
	SSTREAM_ADC_CLK_DIV_8 			= 8,
	SSTREAM_ADC_CLK_DIV_16 			= 16,
	SSTREAM_ADC_CLK_DIV_32 			= 32,
	SSTREAM_ADC_CLK_DIV_64 			= 64,
	SSTREAM_ADC_CLK_DIV_128 		= 128,
	SSTREAM_ADC_CLK_DIV_256 		= 256,
}sstream_adc_clk_div_t;

typedef enum{
	SSTREAM_ADC_AVG_RATIO_UNDEFINE	= 0,
	SSTREAM_ADC_AVG_RATIO_1 		= 1,
	SSTREAM_ADC_AVG_RATIO_2 		= 2,
	SSTREAM_ADC_AVG_RATIO_4 		= 4,
	SSTREAM_ADC_AVG_RATIO_8 		= 8,
	SSTREAM_ADC_AVG_RATIO_16 		= 16,
	SSTREAM_ADC_AVG_RATIO_32 		= 32,
	SSTREAM_ADC_AVG_RATIO_64 		= 64,
	SSTREAM_ADC_AVG_RATIO_128 		= 128,
	SSTREAM_ADC_AVG_RATIO_256 		= 256,
	SSTREAM_ADC_AVG_RATIO_512 		= 512,
	SSTREAM_ADC_AVG_RATIO_1024 		= 1024
}sstream_adc_ch_avg_ratio_t;

typedef enum{
	SSTREAM_ACQUISITION_STATE_UNDEFINED = 0,
	SSTREAM_ACQUISITION_STATE_STOP,
	SSTREAM_ACQUISITION_STATE_START,
	SSTREAM_ACQUISITION_STATE_STREAM
}sstream_acquisition_state_t;

typedef struct
{
	uint8_t		serverIp[4];
	uint16_t	serverport;
	uint32_t	id;
}sstream_connection_info;



sstream_status_t				SSTREAM_Init(void);
sstream_status_t				SSTREAM_CreateChannel(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_status_t				SSTREAM_GetConnectionByID(sstream_connection_info** connectionHandler, uint32_t id);
sstream_status_t				SSTREAM_GetConnectionByIP(sstream_connection_info* connectionHandler, uint8_t ip[4], uint16_t port);
sstream_status_t				SSTREAM_Start(sstream_connection_info* connectionHandler, sstream_adc_t adc, uint32_t timeout);
sstream_status_t				SSTREAM_StartStream(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_status_t				SSTREAM_Stop(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_status_t				SSTREAM_SetResolution(sstream_connection_info* connectionHandler, sstream_adc_resolution_t resolution, uint32_t timeout);
sstream_status_t				SSTREAM_SetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t prescaller, uint32_t period, uint32_t timeout);
sstream_status_t				SSTREAM_SetClkDiv(sstream_connection_info* connectionHandler, sstream_adc_clk_div_t adcClkDiv, uint32_t timeout);
sstream_status_t				SSTREAM_SetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_sampling_time_t stime, uint32_t timeout);
sstream_status_t				SSTREAM_SetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t offset, uint32_t timeout);
sstream_status_t				SSTREAM_SetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_ch_avg_ratio_t avgRatio, uint32_t timeout);
sstream_adc_resolution_t		SSTREAM_GetResolution(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_adc_clk_div_t			SSTREAM_GetClkDiv(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_status_t				SSTREAM_Calibrate(sstream_connection_info* connectionHandler);
uint32_t						SSTREAM_GetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_adc_sampling_time_t		SSTREAM_GetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);
uint32_t						SSTREAM_GetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);
sstream_adc_ch_avg_ratio_t		SSTREAM_GetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);
uint32_t						SSTREAM_GetAdcInputClk(sstream_connection_info* connectionHandler, uint32_t timeout);
sstream_status_t				SSTREAM_GetAdcValue(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t* value, uint32_t timeout);

#endif /* CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_ */
