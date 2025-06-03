/*
 * eez_dib.h
 *
 *  Created on: May 15, 2025
 *      Author: Haris Turkmanovic & Dimitrije Lilic
 */

#ifndef CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_
#define CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_

#include <stdint.h>
#include <string.h>

#include "globalConfig.h"

#define EEZ_DIB_TASK_NAME				CONF_EEZ_DIB_TASK_NAME
#define EEZ_DIB_TASK_PRIO				CONF_EEZ_DIB_TASK_PRIO
#define EEZ_DIB_STACK_SIZE				CONF_EEZ_DIB_STACK_SIZE
#define	EEZ_DIB_ID_QUEUE_LENTH			CONF_EEZ_DIB_ID_QUEUE_LENGTH

typedef enum
{
	EEZ_DIB_STATE_INIT = 0,
	EEZ_DIB_STATE_SERVICE,
	EEZ_DIB_STATE_ERROR
}eez_dib_state_t;

typedef enum
{
	EEZ_DIB_ACQUISIIION_STATE_UNDEF = 0,
	EEZ_DIB_ACQUISIIION_STATE_INACTIVE = 1,
	EEZ_DIB_ACQUISIIION_STATE_ACTIVE = 2,
}eez_dib_acq_state_t;

typedef enum
{
	EEZ_DIB_STATUS_OK,
	EEZ_DIB_STATUS_ERROR
}eez_dib_status_t;

eez_dib_status_t EEZ_DIB_Init(uint32_t timeout);
eez_dib_status_t EEZ_DIB_SetAcquisitionState(eez_dib_acq_state_t acqState, uint8_t streamID, uint32_t timeout);

#endif /* CORE_MIDDLEWARES_SERVICES_EEZDIB_EEZ_DIB_H_ */
