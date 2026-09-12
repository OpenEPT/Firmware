/**
 ******************************************************************************
 * @file    load.c
 *
 * @brief   Load service provides control of the programmable electronic load
 *          and generation of programmable load current waveforms.
 *
 * @author  Haris Turkmanovic
 * @date    September 2026
 ******************************************************************************
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "load.h"
#include "logging.h"
#include "system.h"
#include "drv_aout.h"
#include "drv_gpio.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup LOAD_SERVICE Load Service
 * @{
 */

/**
 * @defgroup LOAD_PRIVATE_DEFINES Load private defines
 * @{
 */

#define LOAD_MASK_SET_CURRENT                  0x00000001
#define LOAD_MASK_SET_STATE                    0x00000002
#define LOAD_MASK_READ_WAVE_CHUNK_MSG          0x00000004
#define LOAD_MASK_WAVE_START                   0x00000008
#define LOAD_MASK_WAVE_STOP                    0x00000010
#define LOAD_MASK_WAVE_CLEAR                   0x00000020
#define LOAD_MASK_SET_DAC_STATUS               0x00000040
#define LOAD_MASK_SET_DAC_VALUE                0x00000080
#define LOAD_MASK_EXECUTE_WAVE_CHUNK		   0x00000100
/**
 * @}
 */

/**
 * @defgroup LOAD_PRIVATE_TYPES Load private types
 * @{
 */

typedef enum
{
    LOAD_SERVICE_STATE_INIT = 0,
    LOAD_SERVICE_STATE_SERVICE,
    LOAD_SERVICE_STATE_ERROR
} load_service_state_t;

typedef struct
{
    char msg[LOAD_WAVE_CHUNK_MSG_SIZE];
    uint16_t size;
} load_wave_chunk_msg_t;

typedef struct load_wave_chunk_t
{
    uint32_t id;

    uint32_t baseValue;
    uint32_t bsDev;

    uint32_t duration;
    uint32_t dDev;

    int leftRepetitionCnt;
    int maxRepetitionCnt;

    uint32_t lastInGroup;

    struct load_wave_chunk_t* nextGroup;

    uint8_t usedFlag;

    struct load_wave_chunk_t* next;
    struct load_wave_chunk_t* prev;

} load_wave_chunk_t;

typedef struct
{
    load_wave_chunk_t chunks[LOAD_WAVE_CHUNK_MAX_NO];

    uint32_t waveChunksCounter;

    load_wave_chunk_t* first;
    load_wave_chunk_t* firstInChain;
    load_wave_chunk_t* last;
    load_wave_chunk_t* current;

    load_wave_state_t state;

    uint32_t ticks;
    uint32_t nextEvent;

    uint8_t chainEndReached;

    int repetitionCounter;

} load_wave_data_t;

typedef struct
{
    load_service_state_t state;

    QueueHandle_t waveChunkMsgQueue;

    SemaphoreHandle_t initSig;
    SemaphoreHandle_t guard;

    TaskHandle_t taskHandle;

    uint32_t current;
    uint32_t dacValue;
    load_state_t loadState;
    load_dac_status_t requestedDACStatus; /**< Requested DAC active status */
    load_dac_status_t dacStatus; /**< Current DAC active status */

    char printBuffer[LOAD_WAVE_CHUNK_PBS];

} load_data_t;

/**
 * @}
 */

/**
 * @defgroup LOAD_PRIVATE_DATA Load private data
 * @{
 */

static load_data_t prvLOAD_DATA;
static load_wave_data_t prvLOAD_WAVE_DATA;
static TIM_HandleTypeDef prvLOAD_TIM;

/**
 * @}
 */

/**
 * @defgroup LOAD_PRIVATE_FUNCTIONS Load private functions
 * @{
 */

static void prvLOAD_TaskFunc(void* pvParameters);
static load_status_t prvLOAD_TIM_Init(void);
static float prvLOAD_CurrentToVoltage(uint32_t current);
static load_status_t prvLOAD_SetState(load_state_t state);
static load_status_t prvLOAD_ExecuteWaveChunk(void);
static load_status_t prvLOAD_WaveReinit(void);
static load_status_t prvLOAD_ExtractWaveDataFromMsg(load_wave_chunk_t* chunk, load_wave_chunk_msg_t* msg);
static load_status_t prvLOAD_AddWaveData(load_wave_chunk_t* chunk);
static load_status_t prvLOAD_ClearWaveData(void);
static load_status_t prvLOAD_PrintChunk(load_wave_chunk_t* chunk, char* buffer, uint32_t* size);

static float prvLOAD_CurrentToVoltage(uint32_t current)
{
    return ((float)current / 1000.0f) * 8.8f * 0.075f;
}

static load_status_t prvLOAD_SetState(load_state_t state)
{
    drv_gpio_pin_state_t pinState;

    if(state == LOAD_STATE_ENABLE)
    {
        pinState = DRV_GPIO_PIN_STATE_RESET;
    }
    else
    {
        pinState = DRV_GPIO_PIN_STATE_SET;
    }

    if(DRV_GPIO_Pin_SetState(LOAD_DISABLE_PORT, LOAD_DISABLE_PIN, pinState) != DRV_GPIO_STATUS_OK)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_TIM_Init(void)
{
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    prvLOAD_TIM.Instance = TIM7;
    prvLOAD_TIM.Init.Prescaler = 200 - 1;
    prvLOAD_TIM.Init.CounterMode = TIM_COUNTERMODE_UP;
    prvLOAD_TIM.Init.Period = 1000;
    prvLOAD_TIM.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if(HAL_TIM_Base_Init(&prvLOAD_TIM) != HAL_OK)
    {
        return LOAD_STATUS_ERROR;
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if(HAL_TIMEx_MasterConfigSynchronization(&prvLOAD_TIM, &sMasterConfig) != HAL_OK)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

void TIM7_IRQHandler(void)
{
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    if((prvLOAD_TIM.Instance->SR & TIM_SR_UIF) != 0U)
    {
        prvLOAD_WAVE_DATA.ticks += 1U;

        if(prvLOAD_WAVE_DATA.ticks == prvLOAD_WAVE_DATA.nextEvent)
        {
            prvLOAD_WAVE_DATA.ticks = 0U;

            if(prvLOAD_WAVE_DATA.chainEndReached == 1U)
            {
                if(prvLOAD_WAVE_DATA.repetitionCounter == 0)
                {
                    xTaskNotifyFromISR(prvLOAD_DATA.taskHandle, LOAD_MASK_WAVE_STOP, eSetBits, &pxHigherPriorityTaskWoken);
                }
                else
                {
                    if(prvLOAD_WAVE_DATA.repetitionCounter != -1)
                    {
                        prvLOAD_WAVE_DATA.repetitionCounter -= 1;
                    }

                    xTaskNotifyFromISR(prvLOAD_DATA.taskHandle, LOAD_MASK_WAVE_START, eSetBits, &pxHigherPriorityTaskWoken);
                }

                portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
            }
            else
            {
                //prvLOAD_ExecuteWaveChunk();
                xTaskNotifyFromISR(prvLOAD_DATA.taskHandle, LOAD_MASK_EXECUTE_WAVE_CHUNK, eSetBits, &pxHigherPriorityTaskWoken);
            }
        }

        prvLOAD_TIM.Instance->SR &= ~TIM_SR_UIF;
    }
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

static load_status_t prvLOAD_ExecuteWaveChunk(void)
{
    if(prvLOAD_WAVE_DATA.current == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if((prvLOAD_WAVE_DATA.current->leftRepetitionCnt > 0) || (prvLOAD_WAVE_DATA.current->leftRepetitionCnt == -1))
    {
        if(prvLOAD_WAVE_DATA.current->baseValue > 0U)
        {
            if(DRV_AOUT_SetVoltage(prvLOAD_CurrentToVoltage(prvLOAD_WAVE_DATA.current->baseValue), DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
            {
                return LOAD_STATUS_ERROR;
            }

            prvLOAD_DATA.current = prvLOAD_WAVE_DATA.current->baseValue;

            if(prvLOAD_DATA.loadState == LOAD_STATE_DISABLE)
            {
                if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_ENABLED) != DRV_AOUT_STATUS_OK)
                {
                    return LOAD_STATUS_ERROR;
                }
                prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_ACTIVE;


                if(prvLOAD_SetState(LOAD_STATE_ENABLE) != LOAD_STATUS_OK)
                {
                    return LOAD_STATUS_ERROR;
                }

                prvLOAD_DATA.loadState = LOAD_STATE_ENABLE;
            }
        }
        else
        {
            if(prvLOAD_DATA.loadState == LOAD_STATE_ENABLE)
            {
                if(prvLOAD_SetState(LOAD_STATE_DISABLE) != LOAD_STATUS_OK)
                {
                    return LOAD_STATUS_ERROR;
                }

                if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED) != DRV_AOUT_STATUS_OK)
                {
                    return LOAD_STATUS_ERROR;
                }

                prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                prvLOAD_DATA.loadState = LOAD_STATE_DISABLE;
            }

            if(DRV_AOUT_SetValue(0, DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
            {
                return LOAD_STATUS_ERROR;
            }

            prvLOAD_DATA.current = 0U;
        }

        prvLOAD_WAVE_DATA.nextEvent = prvLOAD_WAVE_DATA.current->duration;

        if(prvLOAD_WAVE_DATA.current->leftRepetitionCnt != -1)
        {
            if(prvLOAD_WAVE_DATA.current->leftRepetitionCnt > 0)
            {
                prvLOAD_WAVE_DATA.current->leftRepetitionCnt -= 1;
            }

            if((prvLOAD_WAVE_DATA.current->leftRepetitionCnt == 0) &&
               (prvLOAD_WAVE_DATA.current->nextGroup != NULL) &&
               (prvLOAD_WAVE_DATA.current->next == NULL))
            {
                prvLOAD_WAVE_DATA.first = prvLOAD_WAVE_DATA.current->nextGroup;
            }
        }
    }

    if((prvLOAD_WAVE_DATA.current->leftRepetitionCnt == 0) &&
       (prvLOAD_WAVE_DATA.current->nextGroup == NULL) &&
       (prvLOAD_WAVE_DATA.current->next == NULL))
    {
        prvLOAD_WAVE_DATA.chainEndReached = 1U;
        prvLOAD_WAVE_DATA.current->leftRepetitionCnt = prvLOAD_WAVE_DATA.current->maxRepetitionCnt;
    }
    else
    {
        if(prvLOAD_WAVE_DATA.current->leftRepetitionCnt == 0)
        {
            prvLOAD_WAVE_DATA.current->leftRepetitionCnt = prvLOAD_WAVE_DATA.current->maxRepetitionCnt;
        }

        if(prvLOAD_WAVE_DATA.current->next == NULL)
        {
            prvLOAD_WAVE_DATA.current = prvLOAD_WAVE_DATA.first;
        }
        else
        {
            prvLOAD_WAVE_DATA.current = prvLOAD_WAVE_DATA.current->next;
        }
    }

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_WaveReinit(void)
{
    if(prvLOAD_WAVE_DATA.chainEndReached == 1U)
    {
        prvLOAD_WAVE_DATA.first = prvLOAD_WAVE_DATA.firstInChain;
        prvLOAD_WAVE_DATA.current = prvLOAD_WAVE_DATA.first;
        prvLOAD_WAVE_DATA.chainEndReached = 0U;
    }

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_ExtractWaveDataFromMsg(load_wave_chunk_t* chunk, load_wave_chunk_msg_t* msg)
{
    uint32_t index = 0U;
    uint32_t fieldsNo = 1U;
    char msgToProcess[LOAD_WAVE_CHUNK_MSG_SIZE];

    if((chunk == NULL) || (msg == NULL))
    {
        return LOAD_STATUS_ERROR;
    }

    if(prvLOAD_WAVE_DATA.waveChunksCounter == LOAD_WAVE_CHUNK_MAX_NO)
    {
        return LOAD_STATUS_ERROR;
    }

    memset(msgToProcess, 0, sizeof(msgToProcess));

    while(msg->msg[index] != ';')
    {
        msgToProcess[index] = msg->msg[index];

        if(msgToProcess[index] == ',')
        {
            fieldsNo += 1U;
        }

        index += 1U;

        if(index == LOAD_WAVE_CHUNK_MSG_SIZE)
        {
            return LOAD_STATUS_ERROR;
        }
    }

    msgToProcess[index] = ';';

    if(fieldsNo != LOAD_WAVE_CHUNK_MSG_FIELDS)
    {
        return LOAD_STATUS_ERROR;
    }

    int ret = sscanf(msgToProcess, "%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%d,%" SCNu32 ";",
                     &chunk->baseValue,
                     &chunk->bsDev,
                     &chunk->duration,
                     &chunk->dDev,
                     &chunk->maxRepetitionCnt,
                     &chunk->lastInGroup);

    if(ret != (int)fieldsNo)
    {
        return LOAD_STATUS_ERROR;
    }

    chunk->id = 0U;
    chunk->prev = NULL;
    chunk->next = NULL;
    chunk->nextGroup = NULL;

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_AddWaveData(load_wave_chunk_t* chunk)
{
    load_wave_chunk_t* newChunk;

    if(chunk == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(prvLOAD_WAVE_DATA.waveChunksCounter == LOAD_WAVE_CHUNK_MAX_NO)
    {
        return LOAD_STATUS_ERROR;
    }

    chunk->next = NULL;
    chunk->prev = NULL;

    memcpy(&prvLOAD_WAVE_DATA.chunks[prvLOAD_WAVE_DATA.waveChunksCounter], chunk, sizeof(load_wave_chunk_t));

    newChunk = &prvLOAD_WAVE_DATA.chunks[prvLOAD_WAVE_DATA.waveChunksCounter];

    if(prvLOAD_WAVE_DATA.waveChunksCounter == 0U)
    {
        prvLOAD_WAVE_DATA.current = newChunk;
        prvLOAD_WAVE_DATA.first = newChunk;
        prvLOAD_WAVE_DATA.firstInChain = newChunk;
        prvLOAD_WAVE_DATA.last = newChunk;

        newChunk->leftRepetitionCnt = newChunk->maxRepetitionCnt;

        prvLOAD_WAVE_DATA.waveChunksCounter += 1U;
        prvLOAD_WAVE_DATA.nextEvent = newChunk->duration;
    }
    else
    {
        newChunk->id = prvLOAD_WAVE_DATA.waveChunksCounter;

        if(prvLOAD_WAVE_DATA.last->lastInGroup == 1U)
        {
            prvLOAD_WAVE_DATA.last->nextGroup = newChunk;
            prvLOAD_WAVE_DATA.last->next = NULL;
        }
        else
        {
            prvLOAD_WAVE_DATA.last->next = newChunk;
        }

        newChunk->prev = prvLOAD_WAVE_DATA.last;
        prvLOAD_WAVE_DATA.last = newChunk;
        newChunk->leftRepetitionCnt = newChunk->maxRepetitionCnt;

        prvLOAD_WAVE_DATA.waveChunksCounter += 1U;
    }

    chunk->id = prvLOAD_WAVE_DATA.waveChunksCounter - 1U;

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_ClearWaveData(void)
{
    memset(&prvLOAD_WAVE_DATA, 0, sizeof(load_wave_data_t));

    prvLOAD_WAVE_DATA.state = LOAD_WAVE_STATE_INACTIVE;

    return LOAD_STATUS_OK;
}

static load_status_t prvLOAD_PrintChunk(load_wave_chunk_t* chunk, char* buffer, uint32_t* size)
{
    if((chunk == NULL) || (buffer == NULL) || (size == NULL))
    {
        return LOAD_STATUS_ERROR;
    }

    *size = sprintf(buffer,
                    "Wave chunk info\r\n"
                    "=============\r\n"
                    "ID: %lu;\r\n"
                    "Duration: %lu [ms];\r\n"
                    "Duration Dev: %lu [%%];\r\n"
                    "Base Value: %lu [mA];\r\n"
                    "Base Value Dev: %lu [%%];\r\n"
                    "Repetition counter: %d;\r\n",
                    (unsigned long)chunk->id,
                    (unsigned long)chunk->duration,
                    (unsigned long)chunk->dDev,
                    (unsigned long)chunk->baseValue,
                    (unsigned long)chunk->bsDev,
                    chunk->leftRepetitionCnt);

    return LOAD_STATUS_OK;
}

static void prvLOAD_TaskFunc(void* pvParameters)
{
    uint32_t value;

    (void)pvParameters;

    for(;;)
    {
        switch(prvLOAD_DATA.state)
        {
            case LOAD_SERVICE_STATE_INIT:
            {
                drv_gpio_pin_init_conf_t controlPinConfig;

                controlPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
                controlPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

                if(DRV_GPIO_Port_Init(LOAD_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to initialize load control port\r\n");
                }

                if(DRV_GPIO_Pin_Init(LOAD_DISABLE_PORT, LOAD_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to initialize load control pin\r\n");
                }

                if(prvLOAD_SetState(LOAD_STATE_DISABLE) != LOAD_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to set initial load state\r\n");
                }

                prvLOAD_DATA.loadState = LOAD_STATE_DISABLE;
                prvLOAD_DATA.current = 0U;

                if(DRV_AOUT_SetValue(0, DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to reset load DAC\r\n");
                }

                if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED) != DRV_AOUT_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to disable load DAC\r\n");
                }
                else
                {
                    prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                }

                if(prvLOAD_TIM_Init() != LOAD_STATUS_OK)
                {
                    LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to initialize wave timer\r\n");
                    prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                    break;
                }

                prvLOAD_WAVE_DATA.state = LOAD_WAVE_STATE_INACTIVE;

                LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load service initialized\r\n");

                prvLOAD_DATA.state = LOAD_SERVICE_STATE_SERVICE;
                xSemaphoreGive(prvLOAD_DATA.initSig);

                break;
            }

            case LOAD_SERVICE_STATE_SERVICE:
            {
                xTaskNotifyWait(0x00000000, 0xFFFFFFFF, &value, portMAX_DELAY);

                if((value & LOAD_MASK_EXECUTE_WAVE_CHUNK) != 0U)
                {
                    if(prvLOAD_ExecuteWaveChunk() != LOAD_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to execute wave chunk\r\n");
                    }
                }

                if((value & LOAD_MASK_SET_CURRENT) != 0U)
                {
                    uint32_t current;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    current = prvLOAD_DATA.current;
                    xSemaphoreGive(prvLOAD_DATA.guard);

                    if(DRV_AOUT_SetVoltage(prvLOAD_CurrentToVoltage(current), DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to set load current\r\n");
                    }
                    else
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load current %lu mA successfully set\r\n", (unsigned long)current);
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                if((value & LOAD_MASK_SET_STATE) != 0U)
                {
                    load_state_t state;
                    uint32_t current;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    state = prvLOAD_DATA.loadState;
                    current = prvLOAD_DATA.current;

                    xSemaphoreGive(prvLOAD_DATA.guard);

                    if(state == LOAD_STATE_ENABLE)
                    {
                        if(DRV_AOUT_SetVoltage(prvLOAD_CurrentToVoltage(current), DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to set load DAC value\r\n");
                        }

                        if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_ENABLED) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to enable load DAC\r\n");
                        }
                        else
                        {
                            prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_ACTIVE;
                        }

                        if(prvLOAD_SetState(LOAD_STATE_ENABLE) != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to enable load\r\n");
                        }
                        else
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load successfully enabled\r\n");
                        }
                    }
                    else
                    {
                        if(prvLOAD_SetState(LOAD_STATE_DISABLE) != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to disable load\r\n");
                        }

                        if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to disable load DAC\r\n");
                        }
                        else
                        {
                            prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                        }

                        LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load successfully disabled\r\n");
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                if((value & LOAD_MASK_READ_WAVE_CHUNK_MSG) != 0U)
                {
                    load_wave_chunk_msg_t msg;
                    load_wave_chunk_t chunk;
                    uint32_t printSize = 0U;

                    memset(&msg, 0, sizeof(msg));

                    while(xQueueReceive(prvLOAD_DATA.waveChunkMsgQueue, &msg, 0) == pdTRUE)
                    {
                        memset(&chunk, 0, sizeof(chunk));

                        if(prvLOAD_ExtractWaveDataFromMsg(&chunk, &msg) != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to extract Wave Chunk from Msg\r\n");
                            continue;
                        }

                        if(prvLOAD_AddWaveData(&chunk) != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to add Wave Chunk from Msg\r\n");
                            continue;
                        }

                        memset(prvLOAD_DATA.printBuffer, 0, LOAD_WAVE_CHUNK_PBS);

                        if(prvLOAD_PrintChunk(&chunk, prvLOAD_DATA.printBuffer, &printSize) == LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave chunk successfully added\r\n");
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, prvLOAD_DATA.printBuffer);
                        }
                    }
                }

                if((value & LOAD_MASK_WAVE_START) != 0U)
                {
                    if(prvLOAD_WAVE_DATA.waveChunksCounter < 2U)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to start wave\r\n");
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Please add at least 2 wave chunks\r\n");
                        xSemaphoreGive(prvLOAD_DATA.initSig);
                    }
                    else
                    {
                        if(prvLOAD_WAVE_DATA.chainEndReached == 1U)
                        {
                            prvLOAD_WaveReinit();
                        }

                        if(prvLOAD_ExecuteWaveChunk() != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to execute wave chunk\r\n");
                        }
                        else
                        {
                            HAL_TIM_Base_Start_IT(&prvLOAD_TIM);
                            prvLOAD_WAVE_DATA.state = LOAD_WAVE_STATE_ACTIVE;

                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave started - Iteration %d\r\n", prvLOAD_WAVE_DATA.repetitionCounter);
                        }

                        xSemaphoreGive(prvLOAD_DATA.initSig);
                    }
                }

                if((value & LOAD_MASK_WAVE_STOP) != 0U)
                {
                    HAL_TIM_Base_Stop_IT(&prvLOAD_TIM);

                    prvLOAD_SetState(LOAD_STATE_DISABLE);
                    DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED);
                    DRV_AOUT_SetValue(0, DRV_AOUT_CHANNEL_D);

                    prvLOAD_DATA.loadState = LOAD_STATE_DISABLE;
                    prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                    prvLOAD_DATA.current = 0U;

                    prvLOAD_WAVE_DATA.state = LOAD_WAVE_STATE_INACTIVE;

                    LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave stopped\r\n");

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                if((value & LOAD_MASK_WAVE_CLEAR) != 0U)
                {
                    if(prvLOAD_WAVE_DATA.state == LOAD_WAVE_STATE_ACTIVE)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to clear wave while it is active\r\n");
                    }
                    else
                    {
                        prvLOAD_ClearWaveData();
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave successfully cleared\r\n");
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }
                /**********************************************************************
                 * DAC CONTROL
                 **********************************************************************/
                if(value & LOAD_MASK_SET_DAC_STATUS)
                {
                    load_dac_status_t dacStatus;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to take semaphore\r\n");
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    dacStatus = prvLOAD_DATA.requestedDACStatus;

                    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to return semaphore\r\n");
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    switch(dacStatus)
                    {
                    case LOAD_DAC_STATUS_INACTIVE:
                        if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to disable DAC\r\n");
                        }
                        else
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "DAC successfully disabled\r\n");
                            prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                        }
                        break;

                    case LOAD_DAC_STATUS_ACTIVE:
                        if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_ENABLED) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to enable DAC\r\n");
                        }
                        else
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "DAC successfully enabled\r\n");
                            prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_ACTIVE;
                        }
                        break;
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }
                if((value & LOAD_MASK_SET_DAC_VALUE) != 0U)
                {
                    uint32_t dacValue;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to take semaphore\r\n");
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    dacValue = prvLOAD_DATA.dacValue;

                    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to return semaphore\r\n");
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    if(DRV_AOUT_SetValue(dacValue, DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to set DAC value\r\n");
                    }
                    else
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "DAC value %lu successfully set\r\n", (unsigned long)dacValue);
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                break;
            }

            case LOAD_SERVICE_STATE_ERROR:
            {
                SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
                vTaskDelay(portMAX_DELAY);
                break;
            }

            default:
            {
                prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                break;
            }
        }
    }
}

/**
 * @}
 */

/**
 * @defgroup LOAD_PUBLIC_FUNCTIONS Load public functions
 * @{
 */

load_status_t LOAD_Init(uint32_t initTimeout)
{
    memset(&prvLOAD_DATA, 0, sizeof(prvLOAD_DATA));
    memset(&prvLOAD_WAVE_DATA, 0, sizeof(prvLOAD_WAVE_DATA));

    prvLOAD_DATA.current = 0U;
    prvLOAD_DATA.dacValue = 0U;
    prvLOAD_DATA.loadState = LOAD_STATE_DISABLE;
    prvLOAD_DATA.requestedDACStatus = LOAD_DAC_STATUS_INACTIVE;
    prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;

    prvLOAD_DATA.initSig = xSemaphoreCreateBinary();

    if(prvLOAD_DATA.initSig == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_DATA.guard = xSemaphoreCreateMutex();

    if(prvLOAD_DATA.guard == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_DATA.waveChunkMsgQueue = xQueueCreate(LOAD_WAVE_CHUNK_MSG_QUEUE_LENGTH, sizeof(load_wave_chunk_msg_t));

    if(prvLOAD_DATA.waveChunkMsgQueue == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_DATA.state = LOAD_SERVICE_STATE_INIT;

    if(xTaskCreate(prvLOAD_TaskFunc, LOAD_TASK_NAME, LOAD_TASK_STACK, NULL, LOAD_TASK_PRIO, &prvLOAD_DATA.taskHandle) != pdPASS)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetCurrent(uint32_t current, uint32_t timeout)
{
    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_DATA.current = current;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_SET_CURRENT, eSetBits) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_GetCurrent(uint32_t* current, uint32_t timeout)
{
    if(current == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    *current = prvLOAD_DATA.current;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetState(load_state_t state, uint32_t timeout)
{
    if((state != LOAD_STATE_DISABLE) && (state != LOAD_STATE_ENABLE))
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_DATA.loadState = state;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_SET_STATE, eSetBits) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_GetState(load_state_t* state, uint32_t timeout)
{
    if(state == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    *state = prvLOAD_DATA.loadState;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_AddWaveChunk(char* waveDesc, uint16_t waveDescSize, uint32_t timeout)
{
    load_wave_chunk_msg_t msg;
    uint32_t actualSize = 0U;

    if(waveDesc == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(waveDescSize > LOAD_WAVE_CHUNK_MSG_SIZE)
    {
        return LOAD_STATUS_ERROR;
    }

    memset(&msg, 0, sizeof(msg));

    while((actualSize < waveDescSize) && (waveDesc[actualSize] != ';'))
    {
        msg.msg[actualSize] = waveDesc[actualSize];
        actualSize += 1U;

        if(actualSize >= (LOAD_WAVE_CHUNK_MSG_SIZE - 1U))
        {
            return LOAD_STATUS_ERROR;
        }
    }

    if((actualSize >= waveDescSize) || (waveDesc[actualSize] != ';'))
    {
        return LOAD_STATUS_ERROR;
    }

    msg.msg[actualSize] = ';';
    msg.size = (uint16_t)(actualSize + 1U);

    if(xQueueSend(prvLOAD_DATA.waveChunkMsgQueue, &msg, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_READ_WAVE_CHUNK_MSG, eSetBits) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetWaveState(load_wave_state_t state, uint32_t timeout)
{
    uint32_t notification;

    if(state == LOAD_WAVE_STATE_ACTIVE)
    {
        notification = LOAD_MASK_WAVE_START;
    }
    else if(state == LOAD_WAVE_STATE_INACTIVE)
    {
        notification = LOAD_MASK_WAVE_STOP;
    }
    else
    {
        return LOAD_STATUS_ERROR;
    }

    if(xTaskNotify(prvLOAD_DATA.taskHandle, notification, eSetBits) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetWaveCounter(int counter, uint32_t timeout)
{
    if(prvLOAD_WAVE_DATA.state == LOAD_WAVE_STATE_ACTIVE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(counter < -1)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_WAVE_DATA.repetitionCounter = counter;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}

load_status_t LOAD_ClearWave(uint32_t timeout)
{
    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_WAVE_CLEAR, eSetBits) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    return LOAD_STATUS_OK;
}
load_status_t LOAD_SetDACStatus(load_dac_status_t activeStatus, uint32_t timeout)
{
    if((activeStatus != LOAD_DAC_STATUS_INACTIVE) && (activeStatus != LOAD_DAC_STATUS_ACTIVE)) return LOAD_STATUS_ERROR;

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    prvLOAD_DATA.requestedDACStatus = activeStatus;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE) return LOAD_STATUS_ERROR;

    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_SET_DAC_STATUS, eSetBits) != pdTRUE) return LOAD_STATUS_ERROR;

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    return LOAD_STATUS_OK;
}

load_status_t LOAD_GetDACStatus(load_dac_status_t* activeStatus, uint32_t timeout)
{
    if(activeStatus == NULL) return LOAD_STATUS_ERROR;

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    *activeStatus = prvLOAD_DATA.dacStatus;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE) return LOAD_STATUS_ERROR;

    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetDACValue(uint32_t value, uint32_t timeout)
{
    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    prvLOAD_DATA.dacValue = value;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE) return LOAD_STATUS_ERROR;

    if(xTaskNotify(prvLOAD_DATA.taskHandle, LOAD_MASK_SET_DAC_VALUE, eSetBits) != pdTRUE) return LOAD_STATUS_ERROR;

    if(xSemaphoreTake(prvLOAD_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    return LOAD_STATUS_OK;
}

load_status_t LOAD_GetDACValue(uint32_t* value, uint32_t timeout)
{
    if(value == NULL) return LOAD_STATUS_ERROR;

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return LOAD_STATUS_ERROR;

    *value = prvLOAD_DATA.dacValue;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE) return LOAD_STATUS_ERROR;

    return LOAD_STATUS_OK;
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
