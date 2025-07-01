/**
 ******************************************************************************
 * @file	sstream.h
 *
 * @brief	Samples Stream service provides high-speed ADC data acquisition and
 * 			streaming capabilities over network connections. It supports both
 * 			internal and external ADC configurations with configurable resolution,
 * 			sampling rates, and channel parameters. The service manages multiple
 * 			concurrent connections and provides real-time sample streaming with
 * 			calibration and filtering capabilities.
 * 			All samples stream service interface functions, defines, and types
 * 			are declared in this header file.
 *
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_
#define CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_

#include <stdint.h>
#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup SSTREAM_SERVICE Samples Stream Service
 * @{
 */

/**
 * @defgroup SSTREAM_PUBLIC_DEFINES Samples Stream service public defines
 * @{
 */
#define SSTREAM_CONTROL_TASK_NAME					CONF_SSTREAM_CONTROL_TASK_NAME		/*!< Control task name */
#define SSTREAM_CONTROL_TASK_PRIO					CONF_SSTREAM_CONTROL_TASK_PRIO		/*!< Control task priority */
#define SSTREAM_CONTROL_TASK_STACK_SIZE				CONF_SSTREAM_CONTROL_TASK_STACK_SIZE	/*!< Control task stack size */
#define SSTREAM_STREAM_TASK_NAME					CONF_SSTREAM_STREAM_TASK_NAME		/*!< Stream task name */
#define SSTREAM_STREAM_TASK_PRIO					CONF_SSTREAM_STREAM_TASK_PRIO		/*!< Stream task priority */
#define SSTREAM_STREAM_TASK_STACK_SIZE				CONF_SSTREAM_STREAM_TASK_STACK_SIZE	/*!< Stream task stack size */

#define SSTREAM_CONNECTIONS_MAX_NO					CONF_SSTREAM_CONNECTIONS_MAX_NO		/*!< Maximum number of concurrent connections */

#define	SSTREAM_AIN_DEFAULT_RESOLUTION				CONF_SSTREAM_AIN_DEFAULT_RESOLUTION		/*!< Default ADC resolution */
#define	SSTREAM_AIN_DEFAULT_CLOCK_DIV				CONF_SSTREAM_AIN_DEFAULT_CLOCK_DIV		/*!< Default ADC clock divider */
#define	SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME			CONF_SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME	/*!< Default channel sampling time */
#define	SSTREAM_AIN_DEFAULT_CH_AVG_RATIO			CONF_SSTREAM_AIN_DEFAULT_CH_AVG_RATIO	/*!< Default channel averaging ratio */
#define	SSTREAM_AIN_DEFAULT_SAMPLE_TIME				CONF_SSTREAM_AIN_DEFAULT_SAMPLE_TIME	/*!< Default sampling time */
#define	SSTREAM_AIN_DEFAULT_PRESCALER				CONF_SSTREAM_AIN_DEFAULT_PRESCALER		/*!< Default timer prescaler */
#define	SSTREAM_AIN_DEFAULT_PERIOD					CONF_SSTREAM_AIN_DEFAULT_PERIOD			/*!< Default sampling period */
#define SSTREAM_AIN_VOLTAGE_CHANNEL					CONF_SSTREAM_AIN_VOLTAGE_CHANNEL		/*!< Voltage measurement channel */
#define SSTREAM_AIN_CURRENT_CHANNEL					CONF_SSTREAM_AIN_CURRENT_CHANNEL		/*!< Current measurement channel */

#define	SSTREAM_LED_PORT							CONF_SSTREAM_LED_PORT				/*!< Status LED GPIO port */
#define	SSTREAM_LED_PIN								CONF_SSTREAM_LED_PIN				/*!< Status LED GPIO pin */
/**
 * @}
 */

/**
 * @defgroup SSTREAM_PUBLIC_TYPES Samples Stream service public data types
 * @{
 */

/**
 * @brief Samples stream service task state
 */
typedef enum
{
	SSTREAM_STATE_INIT,						/*!< Samples stream service initialization state */
	SSTREAM_STATE_SERVICE,					/*!< Samples stream service is in service state */
	SSTREAM_STATE_ERROR						/*!< Samples stream service is in error state */
}sstream_state_t;

/**
 * @brief ADC type selection
 */
typedef enum
{
	SSTREAM_ADC_INTERNAL = 0,				/*!< Use internal ADC */
	SSTREAM_ADC_EXTERNAL					/*!< Use external ADC */
}sstream_adc_t;

/**
 * @brief Samples stream service return status
 */
typedef enum{
	SSTREAM_STATUS_OK,						/*!< Operation successful */
	SSTREAM_STATUS_ERROR					/*!< Operation failed */
}sstream_status_t;

/**
 * @brief ADC resolution configuration
 */
typedef enum{
	SSTREAM_ADC_RESOLUTION_16BIT = 16,		/*!< 16-bit ADC resolution */
	SSTREAM_ADC_RESOLUTION_14BIT = 14,		/*!< 14-bit ADC resolution */
	SSTREAM_ADC_RESOLUTION_12BIT = 12,		/*!< 12-bit ADC resolution */
	SSTREAM_ADC_RESOLUTION_10BIT = 10		/*!< 10-bit ADC resolution */
}sstream_adc_resolution_t;

/**
 * @brief ADC sampling time configuration (in ADC clock cycles)
 */
typedef enum{
	SSTREAM_ADC_SAMPLING_TIME_1C5 = 1,		/*!< 1.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_2C5 = 2,		/*!< 2.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_8C5 = 8,		/*!< 8.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_16C5 = 16,	/*!< 16.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_32C5 = 32,	/*!< 32.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_64C5 = 64,	/*!< 64.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_387C5 = 387,	/*!< 387.5 ADC clock cycles */
	SSTREAM_ADC_SAMPLING_TIME_810C5 = 810	/*!< 810.5 ADC clock cycles */
}sstream_adc_sampling_time_t;

/**
 * @brief ADC clock divider configuration
 */
typedef enum{
	SSTREAM_ADC_CLK_DIV_1 			= 1,	/*!< Divide by 1 */
	SSTREAM_ADC_CLK_DIV_2 			= 2,	/*!< Divide by 2 */
	SSTREAM_ADC_CLK_DIV_4 			= 4,	/*!< Divide by 4 */
	SSTREAM_ADC_CLK_DIV_8 			= 8,	/*!< Divide by 8 */
	SSTREAM_ADC_CLK_DIV_16 			= 16,	/*!< Divide by 16 */
	SSTREAM_ADC_CLK_DIV_32 			= 32,	/*!< Divide by 32 */
	SSTREAM_ADC_CLK_DIV_64 			= 64,	/*!< Divide by 64 */
	SSTREAM_ADC_CLK_DIV_128 		= 128,	/*!< Divide by 128 */
	SSTREAM_ADC_CLK_DIV_256 		= 256,	/*!< Divide by 256 */
}sstream_adc_clk_div_t;

/**
 * @brief ADC channel averaging ratio configuration
 */
typedef enum{
	SSTREAM_ADC_AVG_RATIO_UNDEFINE	= 0,	/*!< No averaging */
	SSTREAM_ADC_AVG_RATIO_1 		= 1,	/*!< Average 1 sample */
	SSTREAM_ADC_AVG_RATIO_2 		= 2,	/*!< Average 2 samples */
	SSTREAM_ADC_AVG_RATIO_4 		= 4,	/*!< Average 4 samples */
	SSTREAM_ADC_AVG_RATIO_8 		= 8,	/*!< Average 8 samples */
	SSTREAM_ADC_AVG_RATIO_16 		= 16,	/*!< Average 16 samples */
	SSTREAM_ADC_AVG_RATIO_32 		= 32,	/*!< Average 32 samples */
	SSTREAM_ADC_AVG_RATIO_64 		= 64,	/*!< Average 64 samples */
	SSTREAM_ADC_AVG_RATIO_128 		= 128,	/*!< Average 128 samples */
	SSTREAM_ADC_AVG_RATIO_256 		= 256,	/*!< Average 256 samples */
	SSTREAM_ADC_AVG_RATIO_512 		= 512,	/*!< Average 512 samples */
	SSTREAM_ADC_AVG_RATIO_1024 		= 1024	/*!< Average 1024 samples */
}sstream_adc_ch_avg_ratio_t;

/**
 * @brief Data acquisition state
 */
typedef enum{
	SSTREAM_ACQUISITION_STATE_UNDEFINED = 0,	/*!< Undefined state */
	SSTREAM_ACQUISITION_STATE_INACTIVE,			/*!< Acquisition inactive */
	SSTREAM_ACQUISITION_STATE_ACTIVE,			/*!< Acquisition active */
	SSTREAM_ACQUISITION_STATE_STREAM			/*!< Streaming mode active */
}sstream_acquisition_state_t;

/**
 * @brief Connection information structure
 */
typedef struct
{
	uint8_t		serverIp[4];					/*!< Server IP address (4 bytes for IPv4) */
	uint16_t	serverport;						/*!< Server port number */
	uint32_t	id;								/*!< Connection identifier */
}sstream_connection_info;

/**
 * @brief Acquisition state change callback function type
 * @param	id: Connection ID
 * @param	state: New acquisition state. See ::sstream_acquisition_state_t
 */
typedef void (*sstream_acquistion_state_changed_callback)(uint32_t id, sstream_acquisition_state_t state);

/**
 * @}
 */

/**
 * @defgroup SSTREAM_PUBLIC_FUNCTIONS Samples Stream service interface functions
 * @{
 */

/**
 * @brief	Initialize the samples stream service
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_Init(void);

/**
 * @brief	Create a new streaming channel/connection
 * @param	connectionHandler: Pointer to connection information structure. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_CreateChannel(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Get connection handler by connection ID
 * @param	connectionHandler: Pointer to store connection handler
 * @param	id: Connection identifier
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_GetConnectionByID(sstream_connection_info** connectionHandler, uint32_t id);

/**
 * @brief	Get current acquisition state for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_acquisition_state_t
 */
sstream_acquisition_state_t SSTREAM_GetAcquisitionState(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Get connection by IP address and port
 * @param	connectionHandler: Pointer to connection information structure. See ::sstream_connection_info
 * @param	ip: IP address array (4 bytes)
 * @param	port: Port number
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_GetConnectionByIP(sstream_connection_info* connectionHandler, uint8_t ip[4], uint16_t port);

/**
 * @brief	Start data acquisition for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	adc: ADC type selection. See ::sstream_adc_t
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_Start(sstream_connection_info* connectionHandler, sstream_adc_t adc, uint32_t timeout);

/**
 * @brief	Start streaming mode for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_StartStream(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Stop data acquisition for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_Stop(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Set ADC resolution for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	resolution: ADC resolution. See ::sstream_adc_resolution_t
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetResolution(sstream_connection_info* connectionHandler, sstream_adc_resolution_t resolution, uint32_t timeout);

/**
 * @brief	Set sampling period using prescaler and period values
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	prescaller: Timer prescaler value
 * @param	period: Timer period value
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t prescaller, uint32_t period, uint32_t timeout);

/**
 * @brief	Set ADC clock divider
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	adcClkDiv: ADC clock divider. See ::sstream_adc_clk_div_t
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetClkDiv(sstream_connection_info* connectionHandler, sstream_adc_clk_div_t adcClkDiv, uint32_t timeout);

/**
 * @brief	Set number of samples to acquire
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	samplesNo: Number of samples
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetSamplesNo(sstream_connection_info* connectionHandler, uint32_t samplesNo, uint32_t timeout);

/**
 * @brief	Set sampling time for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	stime: Sampling time. See ::sstream_adc_sampling_time_t
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_sampling_time_t stime, uint32_t timeout);

/**
 * @brief	Set offset value for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	offset: Offset value
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t offset, uint32_t timeout);

/**
 * @brief	Set averaging ratio for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	avgRatio: Averaging ratio. See ::sstream_adc_ch_avg_ratio_t
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_SetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, sstream_adc_ch_avg_ratio_t avgRatio, uint32_t timeout);

/**
 * @brief	Get current ADC resolution setting
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_adc_resolution_t
 */
sstream_adc_resolution_t SSTREAM_GetResolution(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Get current ADC clock divider setting
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	::sstream_adc_clk_div_t
 */
sstream_adc_clk_div_t SSTREAM_GetClkDiv(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Calibrate ADC for a connection
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_Calibrate(sstream_connection_info* connectionHandler);

/**
 * @brief	Get current sampling period
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	Sampling period value
 */
uint32_t SSTREAM_GetSamplingPeriod(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Get sampling time for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	timeout: Timeout for operation
 * @retval	::sstream_adc_sampling_time_t
 */
sstream_adc_sampling_time_t SSTREAM_GetChannelSamplingTime(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);

/**
 * @brief	Get offset value for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	timeout: Timeout for operation
 * @retval	Channel offset value
 */
uint32_t SSTREAM_GetChannelOffset(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);

/**
 * @brief	Get averaging ratio for a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	timeout: Timeout for operation
 * @retval	::sstream_adc_ch_avg_ratio_t
 */
sstream_adc_ch_avg_ratio_t SSTREAM_GetChannelAvgRatio(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t timeout);

/**
 * @brief	Get ADC input clock frequency
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	timeout: Timeout for operation
 * @retval	ADC input clock frequency in Hz
 */
uint32_t SSTREAM_GetAdcInputClk(sstream_connection_info* connectionHandler, uint32_t timeout);

/**
 * @brief	Get single ADC value from a specific channel
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	channel: ADC channel number
 * @param	value: Pointer to store ADC value
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_GetAdcValue(sstream_connection_info* connectionHandler, uint32_t channel, uint32_t* value, uint32_t timeout);

/**
 * @brief	Register callback for acquisition state changes
 * @param	cb: Callback function. See ::sstream_acquistion_state_changed_callback
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_RegisterAcquisitionStateChangeCB(sstream_acquistion_state_changed_callback cb);

/**
 * @brief	Get last acquired samples from buffer
 * @param	connectionHandler: Pointer to connection information. See ::sstream_connection_info
 * @param	buffer: Buffer to store sample data
 * @param	size: Buffer size in bytes
 * @param	timeout: Timeout for operation
 * @retval	::sstream_status_t
 */
sstream_status_t SSTREAM_GetLastSamples(sstream_connection_info* connectionHandler, uint8_t* buffer, uint32_t size, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_SAMPLESSTREAM_SSTREAM_H_ */