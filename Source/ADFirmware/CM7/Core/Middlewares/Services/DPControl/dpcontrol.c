/**
 ******************************************************************************
 * @file    dpcontrol.c
 *
 * @brief   Discharge Profile Control (DPControl) service is responsible for
 *          managing output DAC values, enabling/disabling load and battery
 *          lines, controlling the power path, and monitoring protection events
 *          such as under-voltage, over-voltage, and over-current conditions.
 *          The service runs as a FreeRTOS task and uses GPIO and DAC drivers
 *          for interacting with hardware.
 *
 * @author  Haris Turkmanovic
 * @date    November 2023
 ******************************************************************************
 */

#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "dpcontrol.h"
#include "logging.h"
#include "system.h"
#include "control.h"
#include "drv_aout.h"
#include "drv_gpio.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup DPCONTROL_SERVICE DPControl Service
 * @{
 */

/**
 * @defgroup DPCONTROL_DEFINES DPControl internal defines
 * @{
 */
#define DPCONTROL_MASK_SET_VALUE              	0x00000001 /**< Set DAC value */
#define DPCONTROL_MASK_SET_ACTIVE_STATUS      	0x00000002 /**< Set DAC active status */
#define DPCONTROL_MASK_SET_LOAD_STATE         	0x00000004 /**< Set load state */
#define DPCONTROL_MASK_SET_BAT_STATE          	0x00000008 /**< Set battery state */
#define DPCONTROL_MASK_SET_PPATH_STATE        	0x00000010 /**< Set power path state */
#define DPCONTROL_MASK_SET_UV_DETECTED        	0x00000020 /**< Under-voltage detected */
#define DPCONTROL_MASK_TRGER_LATCH            	0x00000040 /**< Trigger latch pin */
#define DPCONTROL_MASK_SET_OV_DETECTED        	0x00000080 /**< Over-voltage detected */
#define DPCONTROL_MASK_SET_OC_DETECTED        	0x00000100 /**< Over-current detected */
#define DPCONTROL_MASK_READ_WAVE_CHUNK_MSG    	0x00000200 /**< Read wave chunk message from the queue */
#define DPCONTROL_MASK_WAVE_START    	  		0x00000400 /**< Start wave */
#define DPCONTROL_MASK_WAVE_STOP    	  		0x00000800 /**< Stop wave */
#define DPCONTROL_MASK_WAVE_CLEAR    	  		0x00001000 /**< Stop wave */
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_STRUCTURES DPControl internal structures
 * @{
 */

/**
 * @brief Structure for DAC configuration data
 */
typedef struct
{
    uint32_t data;                     /**< DAC value */
    dpcontrol_dac_status_t active;    /**< DAC enable/disable status */
} dpcontrol_aout_data_t;


typedef struct
{
	char		msg[DPCONTROL_WAVE_CHUNK_MSG_SIZE];
	uint16_t	size;
} dpcontrol_wave_chunk_msg_t;

typedef struct dpcontrol_wave_chunk_t
{
	uint32_t	id;
	uint32_t	baseValue;
	uint32_t		bsDev; //%
	uint32_t    duration; //ms
	uint32_t    	dDev;	//%
	int			repetitionCnt;
	uint8_t		usedFlag;
	struct dpcontrol_wave_chunk_t* next;
	struct dpcontrol_wave_chunk_t* prev;
} dpcontrol_wave_chunk_t;

typedef struct
{
	dpcontrol_wave_chunk_t		chunks[DPCONTROL_WAVE_CHUNK_MAX_NO];
	uint32_t					waveChunksCounter;
	dpcontrol_wave_chunk_t*		first;
	dpcontrol_wave_chunk_t*		last;
	dpcontrol_wave_chunk_t*		current;
	dpcontrol_wave_state_t		state;
	uint32_t					ticks;
	uint32_t					nextEvent;
}dpcontrol_wave_data_t;




/**
 * @brief Internal data structure for DPControl service
 */
typedef struct
{
    dpcontrol_state_t state;                      /**< Current task state */
    QueueHandle_t waveChunkMsgQueue;              /**< Wave chunk message queue*/
    SemaphoreHandle_t initSig;                    /**< Semaphore to signal initialization complete */
    SemaphoreHandle_t guard;                      /**< Mutex for shared data protection */
    TaskHandle_t taskHandle;                      /**< Handle to the FreeRTOS task */
    dpcontrol_aout_data_t aoutData;               /**< DAC value and active status */
    dpcontrol_load_state_t loadState;             /**< Load state */
    dpcontrol_bat_state_t batState;               /**< Battery state */
    dpcontrol_ppath_state_t pathState;            /**< Power path state */
    dpcontrol_protection_state_t underVoltage;    /**< Under-voltage protection flag */
    dpcontrol_protection_state_t overVoltage;     /**< Over-voltage protection flag */
    dpcontrol_protection_state_t overCurrent;     /**< Over-current protection flag */
    dpcontrol_wave_chunk_msg_t		lastWaveChunkMsg;
    char	printBuffer[DPCONTROL_WAVE_CHUNK_PBS];
} dpcontrol_data_t;
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_PRIVATE_DATA DPControl private data
 * @{
 */
/**
 * @brief Static instance of the DPControl service data
 */
static dpcontrol_data_t 		prvDPCONTROL_DATA;

static dpcontrol_wave_data_t	prvDPCONTROL_WAVE_DATA;

TIM_HandleTypeDef 				DPCONTROL_TIM;
/**
 * @}
 */
/**
 * @defgroup DPCONTROL_PRIVATE_FUNCTIONS DPControl private functions
 * @{
 */
static dpcontrol_status_t prvDPCONTROL_ExecuteWaveChunk();

static dpcontrol_status_t prvDPCONTROL_SetLoadState(dpcontrol_load_state_t loadState)
{
	if(loadState == DPCONTROL_LOAD_STATE_ENABLE)
	{
		if(DRV_GPIO_Pin_SetStateFromISR(DPCONTROL_LOAD_DISABLE_PORT,
				DPCONTROL_LOAD_DISABLE_PIN,
				DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
			return DPCONTROL_STATUS_ERROR;
		return DPCONTROL_STATUS_OK;
	}
	else
	{
		if(DRV_GPIO_Pin_SetStateFromISR(DPCONTROL_LOAD_DISABLE_PORT,
				DPCONTROL_LOAD_DISABLE_PIN,
				DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
			return DPCONTROL_STATUS_ERROR;
		return DPCONTROL_STATUS_OK;
	}
}

/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
	//HAL_TIM_IRQHandler(&DPCONTROL_TIM);
	if(DPCONTROL_TIM.Instance->SR & TIM_SR_UIF)
	{
		prvDPCONTROL_WAVE_DATA.ticks += 1;
		if(prvDPCONTROL_WAVE_DATA.ticks == prvDPCONTROL_WAVE_DATA.nextEvent)
		{
			prvDPCONTROL_WAVE_DATA.ticks = 0;
			prvDPCONTROL_ExecuteWaveChunk();
		}

		DPCONTROL_TIM.Instance->SR &= ~TIM_SR_UIF;  // Clear the update interrupt flag (by writing 0)

	}
}

static dpcontrol_status_t prvDPCONTROL_TIM_Init()
{
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	DPCONTROL_TIM.Instance = TIM7;
	DPCONTROL_TIM.Init.Prescaler = 200-1;
	DPCONTROL_TIM.Init.CounterMode = TIM_COUNTERMODE_UP;
	DPCONTROL_TIM.Init.Period = 1000;
	DPCONTROL_TIM.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&DPCONTROL_TIM) != HAL_OK) return DPCONTROL_STATUS_ERROR;
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&DPCONTROL_TIM, &sMasterConfig) != HAL_OK)  return DPCONTROL_STATUS_ERROR;

	 return DPCONTROL_STATUS_OK;
}

/**
 * @brief GPIO interrupt callback for Under Voltage detection.
 *
 * Triggered on rising/falling edge of the under-voltage GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_UV_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_UnderVoltageCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_UV_DETECTED | DPCONTROL_MASK_WAVE_STOP, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

/**
 * @brief GPIO interrupt callback for Over Voltage detection.
 *
 * Triggered on rising/falling edge of the over-voltage GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_OV_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_OverVoltageCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OV_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

/**
 * @brief GPIO interrupt callback for Over Current detection.
 *
 * Triggered on rising/falling edge of the over-current GPIO. Notifies the main task
 * using `xTaskNotifyFromISR` with `DPCONTROL_MASK_SET_OC_DETECTED` flag.
 *
 * @param[in] pin GPIO pin that caused the interrupt
 */
static void prvDPCONTROL_OverCurrentCB(drv_gpio_pin pin)
{
	BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

	xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OC_DETECTED | DPCONTROL_MASK_WAVE_STOP, eSetBits, &pxHigherPriorityTaskWoken);

	portYIELD_FROM_ISR( pxHigherPriorityTaskWoken );
}

static dpcontrol_status_t prvDPCONTROL_ExecuteWaveChunk()
{

	if(prvDPCONTROL_WAVE_DATA.current->repetitionCnt == 0) 	return DPCONTROL_STATUS_ERROR;

	//calculate values

	//Set output
	if(prvDPCONTROL_WAVE_DATA.current->baseValue > 0)
	{
		DRV_AOUT_SetValue(prvDPCONTROL_WAVE_DATA.current->baseValue);
		prvDPCONTROL_DATA.aoutData.data = prvDPCONTROL_WAVE_DATA.current->baseValue;
		if(prvDPCONTROL_DATA.loadState == DPCONTROL_LOAD_STATE_DISABLE)
		{
			DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_ENABLED);
			prvDPCONTROL_SetLoadState(DPCONTROL_LOAD_STATE_ENABLE);
			prvDPCONTROL_DATA.loadState = DPCONTROL_LOAD_STATE_ENABLE;
			prvDPCONTROL_DATA.aoutData.active = DPCONTROL_DAC_STATUS_ENABLE;
			//Add notification to be sent to main task to information about out state changed
		}
	}
	else
	{
		if(prvDPCONTROL_DATA.loadState == DPCONTROL_LOAD_STATE_ENABLE)
		{
			prvDPCONTROL_SetLoadState(DPCONTROL_LOAD_STATE_DISABLE);
			DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED);
			prvDPCONTROL_DATA.loadState = DPCONTROL_LOAD_STATE_DISABLE;
			prvDPCONTROL_DATA.aoutData.active = DPCONTROL_DAC_STATUS_DISABLE;
			//Add notification to be sent to main task to information about out state changed
		}
		DRV_AOUT_SetValue(0);
		prvDPCONTROL_DATA.aoutData.data = 0;
	}
	prvDPCONTROL_WAVE_DATA.nextEvent = prvDPCONTROL_WAVE_DATA.current->duration;

	//Prepare next
	if(prvDPCONTROL_WAVE_DATA.current->next == 0)
	{
		prvDPCONTROL_WAVE_DATA.current = prvDPCONTROL_WAVE_DATA.first;
	}
	else
	{
		prvDPCONTROL_WAVE_DATA.current = prvDPCONTROL_WAVE_DATA.current->next;
	}


	return DPCONTROL_STATUS_OK;
}

static dpcontrol_status_t prvDPCONTROL_ExtractWaveDataFromMsg(dpcontrol_wave_chunk_t* chunk, dpcontrol_wave_chunk_msg_t* msg)
{
	uint32_t index = 0;
	uint32_t fieldsNo = 1;
	char msgToProcess[DPCONTROL_WAVE_CHUNK_MSG_SIZE];

	if(prvDPCONTROL_WAVE_DATA.waveChunksCounter ==  DPCONTROL_WAVE_CHUNK_MAX_NO) return DPCONTROL_STATUS_ERROR;

	memset(msgToProcess, 0, DPCONTROL_WAVE_CHUNK_MSG_SIZE);

	while(msg->msg[index] != ';')
	{
		msgToProcess[index] = msg->msg[index];
		if(msgToProcess[index] == ',') fieldsNo +=1;
		index += 1;
		if(index == DPCONTROL_WAVE_CHUNK_MSG_SIZE) return DPCONTROL_STATUS_ERROR;
	}
	msgToProcess[index] = ';';
	index += 1;
	/*Check that there is detected DPCONTROL_WAVE_CHUNK_MSG_FIELDS of field*/
	if(fieldsNo != (DPCONTROL_WAVE_CHUNK_MSG_FIELDS)) return DPCONTROL_STATUS_ERROR;

    int ret = sscanf(msgToProcess, "%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%d;",
                     &chunk->baseValue,
                     &chunk->bsDev,
                     &chunk->duration,
                     &chunk->dDev,
                     &chunk->repetitionCnt);

    if(ret != fieldsNo)return DPCONTROL_STATUS_ERROR;

	chunk->id = 0;
	chunk->prev = 0;
	chunk->next = 0;
	return DPCONTROL_STATUS_OK;
}

static dpcontrol_status_t prvDPCONTROL_AddWaveData(dpcontrol_wave_chunk_t* chunk)
{
	if(prvDPCONTROL_WAVE_DATA.waveChunksCounter ==  DPCONTROL_WAVE_CHUNK_MAX_NO) return DPCONTROL_STATUS_ERROR;

	chunk->next = 0;
	chunk->prev = 0;

	memcpy(&prvDPCONTROL_WAVE_DATA.chunks[prvDPCONTROL_WAVE_DATA.waveChunksCounter],
			chunk, sizeof(dpcontrol_wave_chunk_t));


	if(prvDPCONTROL_WAVE_DATA.waveChunksCounter == 0)
	{
		/*Add first data*/
		prvDPCONTROL_WAVE_DATA.current = &prvDPCONTROL_WAVE_DATA.chunks[0];
		prvDPCONTROL_WAVE_DATA.first = &prvDPCONTROL_WAVE_DATA.chunks[0];
		prvDPCONTROL_WAVE_DATA.last = &prvDPCONTROL_WAVE_DATA.chunks[0];
		prvDPCONTROL_WAVE_DATA.waveChunksCounter += 1;
		prvDPCONTROL_WAVE_DATA.nextEvent = prvDPCONTROL_WAVE_DATA.current->duration;
	}
	else
	{
		/*Set ID*/
		prvDPCONTROL_WAVE_DATA.chunks[prvDPCONTROL_WAVE_DATA.waveChunksCounter].id = prvDPCONTROL_WAVE_DATA.waveChunksCounter;
		prvDPCONTROL_WAVE_DATA.last->next = &prvDPCONTROL_WAVE_DATA.chunks[prvDPCONTROL_WAVE_DATA.waveChunksCounter];
		prvDPCONTROL_WAVE_DATA.chunks[prvDPCONTROL_WAVE_DATA.waveChunksCounter].prev = prvDPCONTROL_WAVE_DATA.last;
		prvDPCONTROL_WAVE_DATA.last = &prvDPCONTROL_WAVE_DATA.chunks[prvDPCONTROL_WAVE_DATA.waveChunksCounter];
		prvDPCONTROL_WAVE_DATA.waveChunksCounter += 1;
	}

	chunk->id = prvDPCONTROL_WAVE_DATA.waveChunksCounter - 1;
	return DPCONTROL_STATUS_OK;
}
static dpcontrol_status_t prvDPCONTROL_ClearWaveData()
{
	memset(&prvDPCONTROL_WAVE_DATA, 0, sizeof(dpcontrol_wave_data_t));
	return DPCONTROL_STATUS_OK;
}
static dpcontrol_status_t prvDPCONTROL_PrintChunk(dpcontrol_wave_chunk_t* chunk, char* buffer, uint32_t* size)
{
	if(prvDPCONTROL_WAVE_DATA.waveChunksCounter ==  DPCONTROL_WAVE_CHUNK_MAX_NO) return DPCONTROL_STATUS_ERROR;

	*size = sprintf(buffer, "Wave chunk info\r\n"
			"=============\r\n"
			"ID: %d;\r\n"
			"Duration: %d [ms];\r\n"
			"Duration Dev: %d [%%];\r\n"
			"Base Value: %d [mA];\r\n"
			"Base Value Dev: %d [%%];\r\n"
			"Repetition counter: %d;\r\n",
			chunk->id,
			chunk->duration,
			chunk->dDev,
			chunk->baseValue,
			chunk->bsDev,
			chunk->repetitionCnt);

	return DPCONTROL_STATUS_OK;
}



/**
 * @brief Main task function for Discharge Profile Control service.
 *
 * This task handles:
 * - Initialization of GPIOs for controlling load, battery, and power path
 * - DAC value and enable control
 * - Registration and handling of protection interrupts (under/over-voltage, over-current)
 * - Processing notifications for various control commands
 * - Sending status updates through the control link
 *
 * The task operates in three states:
 * - `DPCONTROL_STATE_INIT`: Initializes all control and protection hardware
 * - `DPCONTROL_STATE_SERVICE`: Waits for commands via `xTaskNotifyWait` and executes them
 * - `DPCONTROL_STATE_ERROR`: Signals a low-level system error and suspends indefinitely
 *
 * @param[in] pvParameters Unused task parameter
 */
static void prvDPCONTROL_TaskFunc(void* pvParameters){
	uint32_t	value;
	uint32_t	aoutValue;
	dpcontrol_dac_status_t 	activeStatus;
	dpcontrol_load_state_t		loadState;
	dpcontrol_bat_state_t		batState;
	dpcontrol_ppath_state_t		ppathState;
	drv_gpio_pin_init_conf_t 	controlPinConfig;
	drv_gpio_pin_init_conf_t 	protectionPinConfig;
	drv_gpio_pin_init_conf_t 	latchPinConfig;

	for(;;){
		switch(prvDPCONTROL_DATA.state)
		{
		case DPCONTROL_STATE_INIT:
			controlPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			controlPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			/*Initialize Load Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_LOAD_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize load control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize load control pin\r\n");
			}

			switch(prvDPCONTROL_DATA.loadState)
			{
			case DPCONTROL_LOAD_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable load\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully disabled\r\n");
				}
				break;
			case DPCONTROL_LOAD_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable load\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully enabled\r\n");
				}
				break;
			}


			/*Initialize Bat Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_BAT_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize battery control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize battery control pin\r\n");
			}
			switch(prvDPCONTROL_DATA.batState)
			{
			case DPCONTROL_LOAD_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable Battery\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully disabled\r\n");
				}
				break;
			case DPCONTROL_LOAD_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable Battery\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully enabled\r\n");
				}
				break;
			}

			/*Initialize PPath Disable pin*/
			if(DRV_GPIO_Port_Init(DPCONTROL_GPIO_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize ppath control port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize ppath control pin\r\n");
			}
			switch(prvDPCONTROL_DATA.pathState)
			{
			case DPCONTROL_PPATH_STATE_DISABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable Power Path\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power Path successfully disabled\r\n");
				}
				break;
			case DPCONTROL_PPATH_STATE_ENABLE:
				if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable Power Path\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power Path successfully enabled\r\n");
				}
				break;
			}

			latchPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
			latchPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

			if(DRV_GPIO_Port_Init(DPCONTROL_LATCH_PORT) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize latch port\r\n");
			}
			if(DRV_GPIO_Pin_Init(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, &latchPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize latch pin\r\n");
			}

			/* Initialize Under voltage protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Under Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, prvDPCONTROL_UnderVoltageCB, CONF_DPCONTROL_UV_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Under Voltage callback\r\n");
			}
			drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);
			prvDPCONTROL_DATA.underVoltage = pinState;
			if(prvDPCONTROL_DATA.underVoltage == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNNING,  "Under voltage protection is active\r\n");
			}

			/* Initialize Over voltage protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, prvDPCONTROL_OverVoltageCB, CONF_DPCONTROL_OV_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage callback\r\n");
			}
			pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);
			prvDPCONTROL_DATA.overVoltage = pinState;
			if(prvDPCONTROL_DATA.overVoltage == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNNING,  "Over voltage protection is active\r\n");
			}

			/* Initialize Over Current protection interrupt pin*/
			protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
			protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;
			if (DRV_GPIO_Pin_Init(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, &protectionPinConfig) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage GPIO\r\n");
			}

			if (DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, prvDPCONTROL_OverCurrentCB, CONF_DPCONTROL_OC_ISR_PRIO) != DRV_GPIO_STATUS_OK)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_ERROR,  "Unable to register Over Voltage callback\r\n");
			}
			pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);
			prvDPCONTROL_DATA.overCurrent = pinState;
			if(prvDPCONTROL_DATA.overCurrent == DPCONTROL_PROTECTION_STATE_ENABLE)
			{
				LOGGING_Write("DPControl",LOGGING_MSG_TYPE_WARNNING,  "Over Current protection is active\r\n");
			}

			if(DRV_AOUT_SetValue(prvDPCONTROL_DATA.aoutData.data) != DRV_AOUT_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to set DAC value\r\n");
			}

			if(prvDPCONTROL_TIM_Init() != DPCONTROL_STATUS_OK)
			{
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to initialize wave timer\r\n");
			}

			LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Discharge Profile Control service successfully initialized\r\n");
			prvDPCONTROL_DATA.state	= DPCONTROL_STATE_SERVICE;
			xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			break;
		case DPCONTROL_STATE_SERVICE:
			xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);
			if(value & DPCONTROL_MASK_SET_VALUE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				aoutValue = prvDPCONTROL_DATA.aoutData.data;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetValue(aoutValue) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to set DAC value\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "DAC value %d successfully set\r\n", aoutValue);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_ACTIVE_STATUS)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				activeStatus = prvDPCONTROL_DATA.aoutData.active;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				if(DRV_AOUT_SetEnable(activeStatus) != DRV_AOUT_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to set active status\r\n");
				}
				else
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Active status (%d) successfully set\r\n", activeStatus);
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_SET_LOAD_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				loadState = prvDPCONTROL_DATA.loadState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(loadState)
				{
				case DPCONTROL_LOAD_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully disabled\r\n");
					}
					break;
				case DPCONTROL_LOAD_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LOAD_DISABLE_PORT, DPCONTROL_LOAD_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable load\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Load successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_SET_BAT_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				batState = prvDPCONTROL_DATA.batState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(batState)
				{
				case DPCONTROL_BAT_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully disabled\r\n");
					}
					break;
				case DPCONTROL_BAT_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable battery\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Battery successfully enabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_SET_PPATH_STATE)
			{
				if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to take semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}

				ppathState = prvDPCONTROL_DATA.pathState;

				if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR,  "Unable to return semaphore\r\n");
					prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
					break;
				}
				switch(ppathState)
				{
				case DPCONTROL_PPATH_STATE_ENABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to enable power path\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power path successfully enabled\r\n");
					}
					break;
				case DPCONTROL_PPATH_STATE_DISABLE:
					if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to disable power path\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Power path successfully disabled\r\n");
					}
					break;
				}
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_TRGER_LATCH)
			{
				if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to reset latch\r\n");
				}
				else
				{
					vTaskDelay(pdMS_TO_TICKS(5));
					if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNNING,  "Unable to reset latch\r\n");
					}
					else
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO,  "Latch successfully reset\r\n");
					}
				}
				//xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}

			if(value & DPCONTROL_MASK_SET_UV_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Under Voltage protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("uvoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Under Voltage protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("uvoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			if(value & DPCONTROL_MASK_SET_OV_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Voltage protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("ovoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Voltage protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("ovoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			if(value & DPCONTROL_MASK_SET_OC_DETECTED)
			{
				drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);
				if (pinState == DRV_GPIO_PIN_STATE_SET)
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Current protection enabled\r\n");
					CONTROL_StatusLinkSendMessage("ocurrent enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
				else
				{
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Over Current protection disabled\r\n");
					CONTROL_StatusLinkSendMessage("ocurrent disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
				}
			}
			if(value & DPCONTROL_MASK_READ_WAVE_CHUNK_MSG)
			{
				dpcontrol_wave_chunk_msg_t msg;
				dpcontrol_wave_chunk_t chunk;
				uint32_t printSize = 0;
				memset(&msg, 0, sizeof(dpcontrol_wave_chunk_msg_t));
				while(xQueueReceive(prvDPCONTROL_DATA.waveChunkMsgQueue, &msg, 0) == pdTRUE)
				{
					memset(&chunk, 0, sizeof(dpcontrol_wave_chunk_t));
					LOGGING_Write("DPControl",LOGGING_MSG_TYPE_INFO,  "Wave chunk read from the queue\r\n");
					if(prvDPCONTROL_ExtractWaveDataFromMsg(&chunk, &msg) != DPCONTROL_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to extract Wave Chunk from Msg\r\n");
						continue;
					}
					if(prvDPCONTROL_AddWaveData(&chunk) != DPCONTROL_STATUS_OK)
					{
						LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to add Wave Chunk from Msg\r\n");
						continue;
					}
					memset(prvDPCONTROL_DATA.printBuffer, 0, DPCONTROL_WAVE_CHUNK_PBS);
					prvDPCONTROL_PrintChunk(&chunk, prvDPCONTROL_DATA.printBuffer, &printSize);
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Wave chunk successfully added\r\n");
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, prvDPCONTROL_DATA.printBuffer);
				}

			}
			if(value & DPCONTROL_MASK_WAVE_START)
			{
				if(prvDPCONTROL_WAVE_DATA.waveChunksCounter < 2)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to start wave\r\n");
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Please add at least 2 wave chunks\r\n");
				}
				else
				{
					HAL_TIM_Base_Start_IT(&DPCONTROL_TIM);
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Wave started\r\n");
					prvDPCONTROL_WAVE_DATA.state = DPCONTROL_WAVE_STATE_ACTIVE;
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
				}
			}
			if(value & DPCONTROL_MASK_WAVE_STOP)
			{
				HAL_TIM_Base_Stop_IT(&DPCONTROL_TIM);

				prvDPCONTROL_SetLoadState(DPCONTROL_LOAD_STATE_DISABLE);
				DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED);
				DRV_AOUT_SetValue(0);
				prvDPCONTROL_DATA.loadState = DPCONTROL_LOAD_STATE_DISABLE;
				prvDPCONTROL_DATA.aoutData.active = DPCONTROL_DAC_STATUS_DISABLE;
				prvDPCONTROL_DATA.aoutData.data = 0;
				LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Wave stopped\r\n");
				prvDPCONTROL_WAVE_DATA.state = DPCONTROL_WAVE_STATE_INACTIVE;
				xSemaphoreGive(prvDPCONTROL_DATA.initSig);
			}
			if(value & DPCONTROL_MASK_WAVE_CLEAR)
			{
				if(prvDPCONTROL_WAVE_DATA.state == DPCONTROL_WAVE_STATE_ACTIVE)
				{
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to clear wave while it is active\r\n");
				}
				else
				{
					prvDPCONTROL_ClearWaveData();
					xSemaphoreGive(prvDPCONTROL_DATA.initSig);
					LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Wave successfully cleared\r\n");
				}
			}
			break;
		case DPCONTROL_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}
	}
}
/**
 * @}
 */
dpcontrol_status_t DPCONTROL_Init(uint32_t initTimeout)
{
	memset(&prvDPCONTROL_DATA, 0, sizeof(dpcontrol_data_t));
	memset(&prvDPCONTROL_WAVE_DATA, 0, sizeof(dpcontrol_wave_data_t));

	prvDPCONTROL_DATA.loadState = DPCONTROL_LOAD_STATE_DISABLE;
	prvDPCONTROL_DATA.batState = DPCONTROL_BAT_STATE_ENABLE;
	prvDPCONTROL_DATA.pathState = DPCONTROL_PPATH_STATE_ENABLE;
	prvDPCONTROL_DATA.aoutData.data = 85; //100mA

	prvDPCONTROL_DATA.initSig = xSemaphoreCreateBinary();

	if(prvDPCONTROL_DATA.initSig == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.guard = xSemaphoreCreateMutex();

	if(prvDPCONTROL_DATA.guard == NULL) return DPCONTROL_STATUS_ERROR;

	prvDPCONTROL_DATA.waveChunkMsgQueue = xQueueCreate(
			DPCONTROL_WAVE_CHUNK_MSG_QUEUE_LENGTH,
			sizeof(dpcontrol_wave_chunk_msg_t));

	if(prvDPCONTROL_DATA.waveChunkMsgQueue == NULL) return DPCONTROL_STATUS_ERROR;



	prvDPCONTROL_DATA.state = DPCONTROL_STATE_INIT;

	if(xTaskCreate(
			prvDPCONTROL_TaskFunc,
			DPCONTROL_TASK_NAME,
			DPCONTROL_TASK_STACK,
			NULL,
			DPCONTROL_TASK_PRIO,
			&prvDPCONTROL_DATA.taskHandle) != pdPASS) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}


dpcontrol_status_t 	DPCONTROL_SetValue(uint32_t value, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.data = value;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t 	DPCONTROL_GetValue(uint32_t* value, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*value = prvDPCONTROL_DATA.aoutData.data;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t 	DPCONTROL_SetDACStatus(dpcontrol_dac_status_t activeStatus, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.aoutData.active = activeStatus;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_ACTIVE_STATUS, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;
	return DPCONTROL_STATUS_OK;

}
dpcontrol_status_t 	DPCONTROL_GetDACStatus(dpcontrol_dac_status_t* activeStatus, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*activeStatus = prvDPCONTROL_DATA.aoutData.active;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t  DPCONTROL_SetLoadState(dpcontrol_load_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.loadState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_LOAD_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetLoadState(dpcontrol_load_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.loadState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t  DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.batState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_BAT_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetBatState(dpcontrol_bat_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.batState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_SetPPathState(dpcontrol_ppath_state_t state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	prvDPCONTROL_DATA.pathState = state;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_PPATH_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetPPathState(dpcontrol_ppath_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.pathState;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetUVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.underVoltage;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetOVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.overVoltage;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_GetOCurrentState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
	if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_OK;

	*state = prvDPCONTROL_DATA.overCurrent;

	if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_OK;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t  DPCONTROL_LatchTriger(uint32_t timeout)
{
	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_TRGER_LATCH, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	//if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t DPCONTROL_AddWaveChunk(char* waveDesc, uint16_t waveDescSize, uint32_t timeout)
{
	dpcontrol_wave_chunk_msg_t msg;
	uint32_t actualSize = 0;

	if(waveDescSize > DPCONTROL_WAVE_CHUNK_MSG_SIZE) return DPCONTROL_STATUS_ERROR;

	memset(&msg, 0, sizeof(dpcontrol_wave_chunk_msg_t));
	while(waveDesc[actualSize] != ';')
	{
		msg.msg[actualSize] = waveDesc[actualSize];
		actualSize += 1;
		if(actualSize == DPCONTROL_WAVE_CHUNK_MSG_SIZE) return DPCONTROL_STATUS_ERROR;
	};
	msg.msg[actualSize] = ';';
	msg.size = actualSize + 1;

	if(xQueueSend(prvDPCONTROL_DATA.waveChunkMsgQueue,
			&msg,
			pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_READ_WAVE_CHUNK_MSG, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}
dpcontrol_status_t DPCONTROL_SetWaveState(dpcontrol_wave_state_t state, uint32_t timeout)
{
	if(state == DPCONTROL_WAVE_STATE_ACTIVE)
	{
		if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_WAVE_START, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;
	}
	else
	{
		if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_WAVE_STOP, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;
	}

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}

dpcontrol_status_t DPCONTROL_ClearWave(uint32_t timeout)
{
	if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_WAVE_CLEAR, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

	if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

	return DPCONTROL_STATUS_OK;
}

