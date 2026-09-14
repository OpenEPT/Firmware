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
#include "energy_debugger.h"

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


#define LOAD_AOUT_MAX_CHUNKS                    2048U

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
    char markerName[LOAD_WAVE_MARKER_NAME_SIZE];
    uint8_t markerNameSize;
    char markerPos;
} load_wave_chunk_msg_t;

typedef struct load_wave_chunk_t
{
    uint32_t id;

    uint32_t baseValue;
    uint32_t bsDev;

    uint32_t duration;
    uint32_t dDev;

    int maxRepetitionCnt;

    uint32_t lastInGroup;

    char markerStartName[LOAD_WAVE_MARKER_NAME_SIZE];
    uint8_t markerStartNameSize;
    char markerEndName[LOAD_WAVE_MARKER_NAME_SIZE];
    uint8_t markerEndNameSize;

    struct load_wave_chunk_t* nextGroup;

    struct load_wave_chunk_t* next;

} load_wave_chunk_t;

typedef struct
{
    load_wave_chunk_t chunks[LOAD_WAVE_CHUNK_MAX_NO];

    uint32_t waveChunksCounter;

    load_wave_chunk_t* firstInChain;
    load_wave_chunk_t* last;

    load_wave_state_t state;



    int repetitionCounter;

    uint32_t seed;
    uint32_t rngState;

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
static drv_aout_wave_point_t prvLOAD_AOUT_CHUNK_BUFFER[LOAD_AOUT_MAX_CHUNKS];

/**
 * @}
 */

/**
 * @defgroup LOAD_PRIVATE_FUNCTIONS Load private functions
 * @{
 */


static load_status_t prvLOAD_SetWaveState(load_wave_state_t state)
{
    if((state != LOAD_WAVE_STATE_INACTIVE) && (state != LOAD_WAVE_STATE_ACTIVE))
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_WAVE_DATA.state = state;

    return LOAD_STATUS_OK;
}



static load_wave_complete_callback_t prvLOAD_WAVE_COMPLETE_CALLBACK = NULL;

#define LOAD_WAVE_TAG_END_FLAG                 0x80000000U

static void prvLOAD_WavePointCallback(uint32_t tag)
{
    load_wave_chunk_t* chunk;
    uint32_t chunkId = tag & ~LOAD_WAVE_TAG_END_FLAG;

    if((chunkId == 0U) || (chunkId > prvLOAD_WAVE_DATA.waveChunksCounter))
    {
        return;
    }

    chunk = &prvLOAD_WAVE_DATA.chunks[chunkId - 1U];

    if((tag & LOAD_WAVE_TAG_END_FLAG) != 0U)
    {
        if(chunk->markerEndNameSize != 0U)
        {
            ENERGY_DEBUGGER_MarkFromISR(chunk->markerEndName, chunk->markerEndNameSize);
        }
    }
    else
    {
        if(chunk->markerStartNameSize != 0U)
        {
            ENERGY_DEBUGGER_MarkFromISR(chunk->markerStartName, chunk->markerStartNameSize);
        }
    }
}

static void prvLOAD_TrimMarkerName(const char* src, uint8_t srcSize, char* dst, uint8_t* dstSize)
{
    uint8_t start = 0U;
    uint8_t end = srcSize;

    while((start < end) && (src[start] == ' ')) start++;
    while((end > start) && (src[end - 1U] == ' ')) end--;

    *dstSize = end - start;
    memset(dst, 0, LOAD_WAVE_MARKER_NAME_SIZE);
    memcpy(dst, &src[start], *dstSize);
}

static void prvLOAD_WaveCompleteCallback(void)
{
    prvLOAD_SetWaveState(LOAD_WAVE_STATE_INACTIVE);

    if(prvLOAD_WAVE_COMPLETE_CALLBACK != NULL)
    {
        prvLOAD_WAVE_COMPLETE_CALLBACK();
    }
}

static float prvLOAD_CurrentToVoltage(uint32_t current)
{
    return ((float)current / 1000.0f) * 8.8f * 0.075f;
}

static uint32_t prvLOAD_Random(void)
{
    uint32_t x = prvLOAD_WAVE_DATA.rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prvLOAD_WAVE_DATA.rngState = x;
    return x;
}

static uint32_t prvLOAD_ApplyDeviation(uint32_t base, uint32_t deviation, uint32_t minValue)
{
    int32_t offset;
    int32_t result;

    if(deviation == 0U)
    {
        return base;
    }

    offset = (int32_t)(prvLOAD_Random() % (2U * deviation + 1U)) - (int32_t)deviation;
    result = (int32_t)base + offset;

    if(result < (int32_t)minValue)
    {
        return minValue;
    }

    return (uint32_t)result;
}

static load_status_t prvLOAD_SerializeWave(uint32_t* waveLength)
{
    load_wave_chunk_t* currentChunk;
    uint32_t bufferIndex = 0U;
    int repetitionIndex;
    float voltage;
    uint32_t chunkValue;
    uint32_t chunkDuration;

    if(waveLength == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(prvLOAD_WAVE_DATA.firstInChain == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    currentChunk = prvLOAD_WAVE_DATA.firstInChain;

    prvLOAD_WAVE_DATA.rngState = (prvLOAD_WAVE_DATA.seed != 0U) ? prvLOAD_WAVE_DATA.seed : LOAD_WAVE_DEFAULT_SEED;

    while(currentChunk != NULL)
    {
        if(currentChunk->maxRepetitionCnt <= 0)
        {
            return LOAD_STATUS_ERROR;
        }

        for(repetitionIndex = 0; repetitionIndex < currentChunk->maxRepetitionCnt; repetitionIndex++)
        {
            if(bufferIndex >= LOAD_AOUT_MAX_CHUNKS)
            {
                return LOAD_STATUS_ERROR;
            }

            chunkValue = prvLOAD_ApplyDeviation(currentChunk->baseValue, currentChunk->bsDev, 0U);
            chunkDuration = prvLOAD_ApplyDeviation(currentChunk->duration, currentChunk->dDev, 1U);

            voltage = prvLOAD_CurrentToVoltage(chunkValue);

            prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].value = DRV_AOUT_ConvertFloatToDigital(voltage);
            prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].duration = chunkDuration * 1000U;
            prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].startTag = 0U;
            prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].endTag = 0U;

            if((currentChunk->markerStartNameSize != 0U) && (repetitionIndex == 0))
            {
                prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].startTag = currentChunk->id + 1U;
            }
            if((currentChunk->markerEndNameSize != 0U) && (repetitionIndex == (currentChunk->maxRepetitionCnt - 1)))
            {
                prvLOAD_AOUT_CHUNK_BUFFER[bufferIndex].endTag = (currentChunk->id + 1U) | LOAD_WAVE_TAG_END_FLAG;
            }

            bufferIndex++;
        }

        if(currentChunk->next != NULL)
        {
            currentChunk = currentChunk->next;
        }
        else
        {
            currentChunk = currentChunk->nextGroup;
        }
    }

    if(bufferIndex == 0U)
    {
        return LOAD_STATUS_ERROR;
    }

    *waveLength = bufferIndex;

    return LOAD_STATUS_OK;
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
    chunk->next = NULL;
    chunk->nextGroup = NULL;
    chunk->markerStartNameSize = 0U;
    chunk->markerEndNameSize = 0U;
    if(msg->markerNameSize != 0U)
    {
        if(msg->markerPos == LOAD_WAVE_MARKER_POS_BOTH)
        {
            uint8_t commaIndex = 0U;
            while((commaIndex < msg->markerNameSize) && (msg->markerName[commaIndex] != ',')) commaIndex++;
            if(commaIndex >= msg->markerNameSize)
            {
                return LOAD_STATUS_ERROR;
            }
            prvLOAD_TrimMarkerName(msg->markerName, commaIndex, chunk->markerStartName, &chunk->markerStartNameSize);
            prvLOAD_TrimMarkerName(&msg->markerName[commaIndex + 1U], msg->markerNameSize - commaIndex - 1U, chunk->markerEndName, &chunk->markerEndNameSize);
            if((chunk->markerStartNameSize == 0U) || (chunk->markerEndNameSize == 0U))
            {
                return LOAD_STATUS_ERROR;
            }
        }
        else if(msg->markerPos == LOAD_WAVE_MARKER_POS_END)
        {
            prvLOAD_TrimMarkerName(msg->markerName, msg->markerNameSize, chunk->markerEndName, &chunk->markerEndNameSize);
        }
        else
        {
            prvLOAD_TrimMarkerName(msg->markerName, msg->markerNameSize, chunk->markerStartName, &chunk->markerStartNameSize);
        }
    }

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

    memcpy(&prvLOAD_WAVE_DATA.chunks[prvLOAD_WAVE_DATA.waveChunksCounter], chunk, sizeof(load_wave_chunk_t));

    newChunk = &prvLOAD_WAVE_DATA.chunks[prvLOAD_WAVE_DATA.waveChunksCounter];

    if(prvLOAD_WAVE_DATA.waveChunksCounter == 0U)
    {
        prvLOAD_WAVE_DATA.firstInChain = newChunk;
        prvLOAD_WAVE_DATA.last = newChunk;


        prvLOAD_WAVE_DATA.waveChunksCounter += 1U;
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

        prvLOAD_WAVE_DATA.last = newChunk;

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
                    chunk->maxRepetitionCnt);

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

                if(DRV_AOUT_WaveRegisterCompleteCallback(prvLOAD_WaveCompleteCallback) != DRV_AOUT_STATUS_OK)
                {
                    prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                    break;
                }

                if(DRV_AOUT_WaveRegisterPointCallback(prvLOAD_WavePointCallback) != DRV_AOUT_STATUS_OK)
                {
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

                if((value & LOAD_MASK_SET_CURRENT) != 0U)
                {
                    uint32_t current;
                    uint32_t dacValue;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    current = prvLOAD_DATA.current;
                    dacValue = DRV_AOUT_ConvertFloatToDigital(prvLOAD_CurrentToVoltage(current));
                    prvLOAD_DATA.dacValue = dacValue;

                    xSemaphoreGive(prvLOAD_DATA.guard);

                    if(prvLOAD_DATA.dacStatus == LOAD_DAC_STATUS_ACTIVE)
                    {
                        if(DRV_AOUT_SetValue(dacValue, DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to apply load current\r\n");
                        }
                    }

                    LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load current %lu mA stored (DAC value %lu)\r\n", (unsigned long)current, (unsigned long)dacValue);

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                if((value & LOAD_MASK_SET_STATE) != 0U)
                {
                    load_state_t state;

                    if(xSemaphoreTake(prvLOAD_DATA.guard, portMAX_DELAY) != pdTRUE)
                    {
                        prvLOAD_DATA.state = LOAD_SERVICE_STATE_ERROR;
                        break;
                    }

                    state = prvLOAD_DATA.loadState;

                    xSemaphoreGive(prvLOAD_DATA.guard);

                    if(state == LOAD_STATE_ENABLE)
                    {


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

                        else
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Load successfully disabled\r\n");
                        }

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

//                        if(prvLOAD_PrintChunk(&chunk, prvLOAD_DATA.printBuffer, &printSize) == LOAD_STATUS_OK)
//                        {
//                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave chunk successfully added\r\n");
//                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, prvLOAD_DATA.printBuffer);
//                        }
                    }
                }

                if((value & LOAD_MASK_WAVE_START) != 0U)
                {
                    uint32_t waveLength = 0U;

                    if(prvLOAD_SerializeWave(&waveLength) != LOAD_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to serialize wave\r\n");
                    }
                    else if(DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_ENABLED) != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to enable DAC\r\n");
                    }
                    else if(DRV_AOUT_WaveConfigure(DRV_AOUT_CHANNEL_D, prvLOAD_AOUT_CHUNK_BUFFER, waveLength, prvLOAD_WAVE_DATA.repetitionCounter) != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to configure wave\r\n");
                    }
                    else if(DRV_AOUT_WaveStart() != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to start wave\r\n");
                    }
                    else
                    {
                        prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_ACTIVE;
                        prvLOAD_SetWaveState(LOAD_WAVE_STATE_ACTIVE);
                        if(prvLOAD_SetState(LOAD_STATE_ENABLE) != LOAD_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to enable load\r\n");
                        }
                        else
                        {
                            prvLOAD_DATA.loadState = LOAD_STATE_ENABLE;
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave started\r\n");
                        }
                    }

                    xSemaphoreGive(prvLOAD_DATA.initSig);
                }

                if((value & LOAD_MASK_WAVE_STOP) != 0U)
                {
                    uint32_t stopAbortCounter = 0U;

                    if(DRV_AOUT_WaveStop() != DRV_AOUT_STATUS_OK)
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Wave stop reported error\r\n");
                    }

                    if((DRV_AOUT_WaveGetStopAbortCounter(&stopAbortCounter) == DRV_AOUT_STATUS_OK) && (stopAbortCounter != 0U))
                    {
                        LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Wave stop abort counter: %lu\r\n", (unsigned long)stopAbortCounter);
                    }

                    prvLOAD_SetState(LOAD_STATE_DISABLE);
                    DRV_AOUT_SetEnable(DRV_AOUT_ACTIVE_STATUS_DISABLED);
                    DRV_AOUT_SetValue(0U, DRV_AOUT_CHANNEL_D);

                    prvLOAD_DATA.loadState = LOAD_STATE_DISABLE;
                    prvLOAD_DATA.dacStatus = LOAD_DAC_STATUS_INACTIVE;
                    prvLOAD_DATA.current = 0U;

                    prvLOAD_SetWaveState(LOAD_WAVE_STATE_INACTIVE);

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
                        if(DRV_AOUT_WaveClear() != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_ERROR, "Unable to clear AOUT wave\r\n");
                        }
                        else
                        {
                            memset(prvLOAD_AOUT_CHUNK_BUFFER, 0, sizeof(prvLOAD_AOUT_CHUNK_BUFFER));
                            prvLOAD_ClearWaveData();
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_INFO, "Wave successfully cleared\r\n");
                        }
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
                        if(DRV_AOUT_SetValue(prvLOAD_DATA.dacValue, DRV_AOUT_CHANNEL_D) != DRV_AOUT_STATUS_OK)
                        {
                            LOGGING_Write("Load", LOGGING_MSG_TYPE_WARNING, "Unable to set DAC value\r\n");
                        }

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
    return LOAD_AddWaveChunkWithMarker(waveDesc, waveDescSize, NULL, 0U, LOAD_WAVE_MARKER_POS_NONE, timeout);
}

load_status_t LOAD_AddWaveChunkWithMarker(char* waveDesc, uint16_t waveDescSize, const char* markerName, uint8_t markerNameSize, char markerPos, uint32_t timeout)
{
    load_wave_chunk_msg_t msg;
    uint32_t actualSize = 0U;

    if(waveDesc == NULL)
    {
        return LOAD_STATUS_ERROR;
    }

    if(markerName != NULL)
    {
        if((markerNameSize == 0U) || (markerNameSize >= LOAD_WAVE_MARKER_NAME_SIZE))
        {
            return LOAD_STATUS_ERROR;
        }
        if((markerPos != LOAD_WAVE_MARKER_POS_START) && (markerPos != LOAD_WAVE_MARKER_POS_END) && (markerPos != LOAD_WAVE_MARKER_POS_BOTH))
        {
            return LOAD_STATUS_ERROR;
        }
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

    if(markerName != NULL)
    {
        memcpy(msg.markerName, markerName, markerNameSize);
        msg.markerNameSize = markerNameSize;
        msg.markerPos = markerPos;
    }

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

load_status_t LOAD_RegisterWaveCompleteCallback(load_wave_complete_callback_t callback)
{
    prvLOAD_WAVE_COMPLETE_CALLBACK = callback;
    return LOAD_STATUS_OK;
}

load_status_t LOAD_SetWaveSeed(uint32_t seed, uint32_t timeout)
{
    if(prvLOAD_WAVE_DATA.state == LOAD_WAVE_STATE_ACTIVE)
    {
        return LOAD_STATUS_ERROR;
    }

    if(xSemaphoreTake(prvLOAD_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE)
    {
        return LOAD_STATUS_ERROR;
    }

    prvLOAD_WAVE_DATA.seed = seed;

    if(xSemaphoreGive(prvLOAD_DATA.guard) != pdTRUE)
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
