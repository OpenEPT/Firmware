/**
 ******************************************************************************
 * @file   	drv_ain.h
 *
 * @brief  	Analog Input (AIN) driver provides hardware abstraction layer for
 * 			ADC peripherals including internal STM32 ADCs and external ADS9224R.
 * 			This driver supports multi-channel ADC configuration, DMA streaming,
 * 			sampling rate control, resolution settings, channel averaging, and
 * 			real-time data acquisition with callback mechanisms for continuous
 * 			analog signal monitoring and processing.
 * 			All AIN driver interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGIN_H_
#define CORE_DRIVERS_PLATFORM_ANALOGIN_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup DRIVERS Platform Drivers
 * @{
 */

/**
 * @defgroup AIN_DRIVER Analog Input Driver
 * @{
 */

/**
 * @defgroup AIN_PUBLIC_DEFINES AIN driver public defines
 * @{
 */
#define DRV_AIN_ADC_BUFFER_MAX_SIZE			CONF_AIN_MAX_BUFFER_SIZE		/*!< Maximum ADC buffer size */
#define DRV_AIN_ADC_BUFFER_OFFSET			CONF_AIN_ADC_BUFFER_OFFSET		/*!< ADC buffer offset */
#define DRV_AIN_ADC_BUFFER_MARKER			CONF_AIN_ADC_BUFFER_MARKER		/*!< ADC buffer marker value */
#define DRV_AIN_ADC_BUFFER_NO				CONF_AIN_MAX_BUFFER_NO			/*!< Maximum number of ADC buffers */
#define DRV_AIN_ADC_TIM_INPUT_CLK			CONF_DRV_AIN_ADC_TIM_INPUT_CLK	/*!< ADC timer input clock in MHz */
#define DRV_AIN_ADC_BUTTON_ISR_COMPLETED_BIT	5							/*!< Button ISR completion bit position */
/**
 * @}
 */

/**
 * @defgroup AIN_PUBLIC_TYPES AIN driver public data types
 * @{
 */

/**
 * @brief AIN driver return status
 */
typedef enum
{
	DRV_AIN_STATUS_OK,				/*!< AIN operation successful */
	DRV_AIN_STATUS_ERROR			/*!< AIN operation failed */
}drv_ain_status;

/**
 * @brief Available ADC peripherals
 */
typedef enum
{
	DRV_AIN_ADC_1 = 0x01,			/*!< Internal ADC1 peripheral */
	DRV_AIN_ADC_2 = 0x02,			/*!< Internal ADC2 peripheral */
	DRV_AIN_ADC_3 = 0x04,			/*!< Internal ADC3 peripheral */
	DRV_AIN_ADC_ADS9224R = 0x08		/*!< External ADS9224R ADC */
}drv_ain_adc_t;

/**
 * @brief ADC DMA connection status
 */
typedef enum
{
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_CONNECTED,		/*!< DMA is connected to ADC */
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_DISCONNECTED,		/*!< DMA is disconnected from ADC */
	DRV_AIN_ADC_DMA_CONNECTION_STATUS_UNKNOWN			/*!< DMA connection status unknown */
}drv_ain_adc_dma_connection_status_t;

/**
 * @brief ADC data acquisition status
 */
typedef enum
{
	DRV_AIN_ADC_ACQUISITION_STATUS_ACTIVE,				/*!< ADC acquisition is active */
	DRV_AIN_ADC_ACQUISITION_STATUS_INACTIVE,			/*!< ADC acquisition is inactive */
	DRV_AIN_ADC_ACQUISITION_STATUS_UNKNOWN				/*!< ADC acquisition status unknown */
}drv_ain_adc_acquisition_status_t;

/**
 * @brief ADC resolution settings
 */
typedef enum
{
	DRV_AIN_ADC_RESOLUTION_UNKNOWN 	= 0,		/*!< Unknown resolution */
	DRV_AIN_ADC_RESOLUTION_8BIT 	= 8,		/*!< 8-bit resolution */
	DRV_AIN_ADC_RESOLUTION_10BIT 	= 10,		/*!< 10-bit resolution */
	DRV_AIN_ADC_RESOLUTION_12BIT 	= 12,		/*!< 12-bit resolution */
	DRV_AIN_ADC_RESOLUTION_14BIT 	= 14,		/*!< 14-bit resolution */
	DRV_AIN_ADC_RESOLUTION_16BIT 	= 16		/*!< 16-bit resolution */
}drv_ain_adc_resolution_t;

/**
 * @brief ADC sampling time in clock cycles
 */
typedef enum
{
	DRV_AIN_ADC_SAMPLE_TIME_UNKNOWN	= 0,		/*!< Unknown sampling time */
	DRV_AIN_ADC_SAMPLE_TIME_1C5 	= 1,		/*!< 1.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_2C5 	= 2,		/*!< 2.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_8C5 	= 8,		/*!< 8.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_16C5 	= 16,		/*!< 16.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_32C5 	= 32,		/*!< 32.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_64C5 	= 64,		/*!< 64.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_387C5 	= 387,		/*!< 387.5 clock cycles */
	DRV_AIN_ADC_SAMPLE_TIME_810C5 	= 810		/*!< 810.5 clock cycles */
}drv_ain_adc_sample_time_t;

/**
 * @brief ADC clock division ratios
 */
typedef enum
{
	DRV_AIN_ADC_CLOCK_DIV_UNKNOWN	=	0,		/*!< Unknown clock division */
	DRV_AIN_ADC_CLOCK_DIV_1			=	1,		/*!< Clock divided by 1 */
	DRV_AIN_ADC_CLOCK_DIV_2			=	2,		/*!< Clock divided by 2 */
	DRV_AIN_ADC_CLOCK_DIV_4			=	4,		/*!< Clock divided by 4 */
	DRV_AIN_ADC_CLOCK_DIV_8			=	8,		/*!< Clock divided by 8 */
	DRV_AIN_ADC_CLOCK_DIV_16		=	16,		/*!< Clock divided by 16 */
	DRV_AIN_ADC_CLOCK_DIV_32		=	32,		/*!< Clock divided by 32 */
	DRV_AIN_ADC_CLOCK_DIV_64		=	64,		/*!< Clock divided by 64 */
	DRV_AIN_ADC_CLOCK_DIV_128		=	128,	/*!< Clock divided by 128 */
	DRV_AIN_ADC_CLOCK_DIV_256		=	256		/*!< Clock divided by 256 */
}drv_ain_adc_clock_div_t;

/**
 * @brief ADC channel averaging ratios
 */
typedef enum{
	DRV_AIN_ADC_AVG_RATIO_UNDEFINED	= 0,		/*!< Undefined averaging ratio */
	DRV_AIN_ADC_AVG_RATIO_1 		= 1,		/*!< No averaging (1 sample) */
	DRV_AIN_ADC_AVG_RATIO_2 		= 2,		/*!< Average 2 samples */
	DRV_AIN_ADC_AVG_RATIO_4 		= 4,		/*!< Average 4 samples */
	DRV_AIN_ADC_AVG_RATIO_8 		= 8,		/*!< Average 8 samples */
	DRV_AIN_ADC_AVG_RATIO_16 		= 16,		/*!< Average 16 samples */
	DRV_AIN_ADC_AVG_RATIO_32 		= 32,		/*!< Average 32 samples */
	DRV_AIN_ADC_AVG_RATIO_64 		= 64,		/*!< Average 64 samples */
	DRV_AIN_ADC_AVG_RATIO_128 		= 128,		/*!< Average 128 samples */
	DRV_AIN_ADC_AVG_RATIO_256 		= 256,		/*!< Average 256 samples */
	DRV_AIN_ADC_AVG_RATIO_512 		= 512,		/*!< Average 512 samples */
	DRV_AIN_ADC_AVG_RATIO_1024 		= 1024		/*!< Average 1024 samples */
}drv_adc_ch_avg_ratio_t;

/**
 * @brief ADC channel identifier type
 */
typedef uint8_t	drv_ain_adc_channel_t;

/**
 * @brief ADC stream callback function pointer type
 * @param param1: First callback parameter (typically buffer address)
 * @param param2: Second callback parameter (typically buffer ID)
 * @param param3: Third callback parameter (typically sample count)
 */
typedef void (*drv_ain_adc_stream_callback)(uint32_t, uint8_t, uint32_t);

/**
 * @brief ADC channel configuration structure
 */
typedef struct
{
	drv_ain_adc_channel_t			channel;		/*!< ADC channel number */
	drv_ain_adc_sample_time_t		sampleTime;		/*!< Channel sampling time */
	uint32_t						offset;			/*!< Channel offset value */
	drv_adc_ch_avg_ratio_t			avgRatio;		/*!< Channel averaging ratio */
}drv_ain_adc_channel_config_t;

/**
 * @brief ADC configuration structure
 */
typedef struct
{
	drv_ain_adc_t					adc;			/*!< ADC peripheral to configure */
	drv_ain_adc_clock_div_t 		clockDiv;		/*!< ADC clock division ratio */
	drv_ain_adc_resolution_t 		resolution;		/*!< ADC resolution setting */
	drv_ain_adc_channel_config_t 	ch1;			/*!< Channel 1 configuration */
	drv_ain_adc_channel_config_t 	ch2;			/*!< Channel 2 configuration */
	uint32_t						inputClk;		/*!< Input clock frequency */
	uint32_t						samplingTime;	/*!< Sampling time in microseconds */
	uint32_t						prescaler;		/*!< Timer prescaler value */
	uint32_t						period;			/*!< Sampling period in nanoseconds */
	uint32_t						samplesNo;		/*!< Number of samples to acquire */
}drv_ain_adc_config_t;

/**
 * @}
 */

/**
 * @defgroup AIN_PUBLIC_FUNCTIONS AIN driver interface functions
 * @{
 */

/**
 * @brief	Initialize ADC peripheral with specified configuration
 * @param	adc: ADC peripheral to initialize. See ::drv_ain_adc_t
 * @param	configuration: Pointer to ADC configuration structure. See ::drv_ain_adc_config_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Init(drv_ain_adc_t adc, drv_ain_adc_config_t* configuration);

/**
 * @brief	Start ADC data acquisition
 * @param	adc: ADC peripheral to start. See ::drv_ain_adc_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Start(drv_ain_adc_t adc);

/**
 * @brief	Stop ADC data acquisition
 * @param	adc: ADC peripheral to stop. See ::drv_ain_adc_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Stop(drv_ain_adc_t adc);

/**
 * @brief	Set number of samples to acquire
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	samplesNo: Number of samples to acquire
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetSamplesNo(drv_ain_adc_t adc, uint32_t samplesNo);

/**
 * @brief	Get current ADC acquisition status
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @retval	::drv_ain_adc_acquisition_status_t
 */
drv_ain_adc_acquisition_status_t DRV_AIN_GetAcquisitionStatus(drv_ain_adc_t adc);
/**
 * @brief	Set ADC resolution
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	res: ADC resolution to set. See ::drv_ain_adc_resolution_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetResolution(drv_ain_adc_t adc, drv_ain_adc_resolution_t res);

/**
 * @brief	Set ADC clock division ratio
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	div: Clock division ratio. See ::drv_ain_adc_clock_div_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetClockDiv(drv_ain_adc_t adc, drv_ain_adc_clock_div_t div);

/**
 * @brief	Set sampling time for all ADC channels
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	stime: Sampling time to set. See ::drv_ain_adc_sample_time_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetChannelsSamplingTime(drv_ain_adc_t adc, drv_ain_adc_sample_time_t stime);

/**
 * @brief	Set offset value for specific ADC channel
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	channel: ADC channel number
 * @param	offset: Offset value to set
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetChannelOffset(drv_ain_adc_t adc, uint32_t channel, uint32_t offset);

/**
 * @brief	Set averaging ratio for ADC channels
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	avgRatio: Averaging ratio to set. See ::drv_adc_ch_avg_ratio_t
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetChannelAvgRatio(drv_ain_adc_t adc, drv_adc_ch_avg_ratio_t avgRatio);

/**
 * @brief	Set ADC sampling period
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	period: Sampling period in microseconds
 * @param	prescaller: Timer prescaler value
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_SetSamplingPeriod(drv_ain_adc_t adc, uint32_t period, uint32_t prescaller);

/**
 * @brief	Get current ADC resolution setting
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @retval	::drv_ain_adc_resolution_t
 */
drv_ain_adc_resolution_t DRV_AIN_GetResolution(drv_ain_adc_t adc);

/**
 * @brief	Get sampling time for specific ADC channel
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	channel: ADC channel to query. See ::drv_ain_adc_channel_t
 * @retval	::drv_ain_adc_sample_time_t
 */
drv_ain_adc_sample_time_t DRV_AIN_GetSamplingTime(drv_ain_adc_t adc, drv_ain_adc_channel_t channel);

/**
 * @brief	Get ADC clock frequency
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	clk: Pointer to variable to store clock frequency
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_GetADCClk(drv_ain_adc_t adc, uint32_t *clk);

/**
 * @brief	Get single ADC conversion value
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	channel: ADC channel to read. See ::drv_ain_adc_channel_t
 * @param	value: Pointer to variable to store ADC value
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_GetADCValue(drv_ain_adc_t adc, drv_ain_adc_channel_t channel, uint32_t* value);

/**
 * @brief	Enable ADC DMA streaming mode
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	sampleSize: Size of each sample in bytes
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Stream_Enable(drv_ain_adc_t adc, uint32_t sampleSize);

/**
 * @brief	Register callback function for ADC stream events
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	cbfunction: Callback function pointer. See ::drv_ain_adc_stream_callback
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Stream_RegisterCallback(drv_ain_adc_t adc, drv_ain_adc_stream_callback cbfunction);

/**
 * @brief	Submit buffer address for DMA transfer
 * @param	adc: ADC peripheral. See ::drv_ain_adc_t
 * @param	addr: Buffer memory address
 * @param	bufferID: Buffer identifier
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Stream_SubmitAddr(drv_ain_adc_t adc, uint32_t addr, uint8_t bufferID);

/**
 * @brief	Set packet counter for data capture
 * @param	packetCounter: Pointer to packet counter variable
 * @retval	::drv_ain_status
 */
drv_ain_status DRV_AIN_Stream_SetCapture(uint32_t* packetCounter);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_DRIVERS_PLATFORM_ANALOGIN_AIN_H_ */