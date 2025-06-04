/**
 ******************************************************************************
 * @file	eez_dib.h
 *
 * @brief	EEZ DIB service provides an interface for communication with the
 * 			main EEZ DIB microcontroller. It is responsible for initializing the
 * 			service, managing the acquisition process, and tracking acquisition
 * 			state across streams.
 * 			All EEZ DIB service interface functions, defines, and types are
 * 			declared in this header file.
 *
 * @author	Haris Turkmanovic & Dimitrije Lilic
 * @date	May 2025
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_
#define CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_

#include <stdint.h>
#include <string.h>
#include "globalConfig.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup EEZ_DIB_SERVICE EEZ DIB Service
 * @{
 */

/**
 * @defgroup EEZ_DIB_PUBLIC_DEFINES EEZ DIB service public defines
 * @{
 */
#define EEZ_DIB_TASK_NAME				CONF_EEZ_DIB_TASK_NAME			/*!< EEZ DIB service task name */
#define EEZ_DIB_TASK_PRIO				CONF_EEZ_DIB_TASK_PRIO			/*!< EEZ DIB service task priority */
#define EEZ_DIB_STACK_SIZE				CONF_EEZ_DIB_STACK_SIZE			/*!< EEZ DIB service stack size */
#define	EEZ_DIB_ID_QUEUE_LENTH			CONF_EEZ_DIB_ID_QUEUE_LENGTH	/*!< EEZ DIB stream ID queue length */
/**
 * @}
 */

/**
 * @defgroup EEZ_DIB_PUBLIC_TYPES EEZ DIB service public data types
 * @{
 */

/**
 * @brief EEZ DIB service state
 */
typedef enum
{
	EEZ_DIB_STATE_INIT = 0,				/*!< Initialization state */
	EEZ_DIB_STATE_SERVICE,				/*!< EEZ DIB service active */
	EEZ_DIB_STATE_ERROR					/*!< EEZ DIB service in error state */
} eez_dib_state_t;

/**
 * @brief Acquisition state for a given data stream
 */
typedef enum
{
	EEZ_DIB_ACQUISIIION_STATE_UNDEF = 0,	/*!< Undefined acquisition state */
	EEZ_DIB_ACQUISIIION_STATE_INACTIVE = 1,	/*!< Acquisition inactive */
	EEZ_DIB_ACQUISIIION_STATE_ACTIVE = 2	/*!< Acquisition active */
} eez_dib_acq_state_t;

/**
 * @brief EEZ DIB service status return type
 */
typedef enum
{
	EEZ_DIB_STATUS_OK,					/*!< Operation successful */
	EEZ_DIB_STATUS_ERROR				/*!< Operation failed */
} eez_dib_status_t;

/**
 * @}
 */

/**
 * @defgroup EEZ_DIB_PUBLIC_FUNCTIONS EEZ DIB service interface functions
 * @{
 */

/**
 * @brief	Initialize the EEZ DIB service
 * @param	timeout: Timeout to complete initialization
 * @retval	::eez_dib_status_t
 */
eez_dib_status_t EEZ_DIB_Init(uint32_t timeout);

/**
 * @brief	Set the acquisition state for a specific data stream
 * @param	acqState: Desired acquisition state. See ::eez_dib_acq_state_t
 * @param	streamID: Stream identifier for which the state is applied
 * @param	timeout: Timeout for operation
 * @retval	::eez_dib_status_t
 */
eez_dib_status_t EEZ_DIB_SetAcquisitionState(eez_dib_acq_state_t acqState, uint8_t streamID, uint32_t timeout);

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_ */
