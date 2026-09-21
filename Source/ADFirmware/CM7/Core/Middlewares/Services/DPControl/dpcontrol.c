/**
 ******************************************************************************
 * @file    dpcontrol.c
 *
 * @brief   Discharge Profile Control (DPControl) service is responsible for
 *          enabling/disabling battery and power paths, controlling the
 *          protection latch, and monitoring protection events such as
 *          under-voltage, over-voltage, and over-current conditions.
 *          The service runs as a FreeRTOS task and uses GPIO and DAC drivers
 *          for interacting with hardware.
 *
 * @author  Haris Turkmanovic
 * @date    November 2023
 ******************************************************************************
 */

#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "dpcontrol.h"
#include "logging.h"
#include "system.h"
#include "control.h"
#include "drv_aout.h"
#include "drv_gpio.h"
#include "configuration.h"

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
#define DPCONTROL_MASK_SET_BAT_STATE           0x00000001 /**< Set battery state */
#define DPCONTROL_MASK_SET_PPATH_STATE         0x00000002 /**< Set power path state */
#define DPCONTROL_MASK_SET_UV_DETECTED         0x00000004 /**< Under-voltage detected */
#define DPCONTROL_MASK_TRGER_LATCH             0x00000008 /**< Trigger latch pin */
#define DPCONTROL_MASK_SET_OV_DETECTED         0x00000010 /**< Over-voltage detected */
#define DPCONTROL_MASK_SET_OC_DETECTED         0x00000020 /**< Over-current detected */
#define DPCONTROL_MASK_SET_OV_VALUE            0x00000040 /**< Set Over Voltage Protection On value */
#define DPCONTROL_MASK_SET_UV_VALUE            0x00000080 /**< Set Under Voltage Protection On value */
#define DPCONTROL_MASK_SET_OC_VALUE            0x00000100 /**< Set Over Current Protection On value */
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_STRUCTURES DPControl internal structures
 * @{
 */

/**
 * @brief Internal data structure for DPControl service
 */
typedef struct
{
    dpcontrol_state_t state;                      /**< Current task state */
    SemaphoreHandle_t initSig;                    /**< Semaphore to signal initialization complete */
    SemaphoreHandle_t guard;                      /**< Mutex for shared data protection */
    TaskHandle_t taskHandle;                      /**< Handle to the FreeRTOS task */
    dpcontrol_bat_state_t batState;               /**< Battery state */
    dpcontrol_ppath_state_t pathState;            /**< Power path state */
    dpcontrol_protection_state_t underVoltage;    /**< Under-voltage protection flag */
    dpcontrol_protection_state_t overVoltage;     /**< Over-voltage protection flag */
    dpcontrol_protection_state_t overCurrent;     /**< Over-current protection flag */
    float ovValue;                                /**< Over-voltage protection threshold */
    float uvValue;                                /**< Under-voltage protection threshold */
    int32_t ocValue;                              /**< Over-current protection threshold */
    float shuntValue;                             /**< Current sensing shunt resistance */
    float gainValue;                              /**< Current sensing amplifier gain */
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
static dpcontrol_data_t prvDPCONTROL_DATA;
/**
 * @}
 */

/**
 * @defgroup DPCONTROL_PRIVATE_FUNCTIONS DPControl private functions
 * @{
 */

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

    (void)pin;

    xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_UV_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
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

    (void)pin;

    xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OV_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
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

    (void)pin;

    xTaskNotifyFromISR(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OC_DETECTED, eSetBits, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

/**
 * @brief Main task function for Discharge Profile Control service.
 *
 * This task handles:
 * - Initialization of GPIOs for controlling battery and power path
 * - Protection latch control
 * - Registration and handling of protection interrupts (under/over-voltage, over-current)
 * - Configuration of protection thresholds through DAC channels
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
static void prvDPCONTROL_TaskFunc(void* pvParameters)
{
    uint32_t value;
    dpcontrol_bat_state_t batState;
    dpcontrol_ppath_state_t ppathState;
    drv_gpio_pin_init_conf_t controlPinConfig;
    drv_gpio_pin_init_conf_t protectionPinConfig;
    drv_gpio_pin_init_conf_t latchPinConfig;

    (void)pvParameters;

    for(;;)
    {
        switch(prvDPCONTROL_DATA.state)
        {
        case DPCONTROL_STATE_INIT:

            /**********************************************************************
             * GPIO CONTROL OUTPUT INITIALIZATION (BAT / PPATH)
             **********************************************************************/
            controlPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
            controlPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

            /* --- BATTERY CONTROL --- */
            if(DRV_GPIO_Port_Init(DPCONTROL_BAT_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
                LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to initialize battery control port\r\n");

            if(DRV_GPIO_Pin_Init(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
                LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to initialize battery control pin\r\n");

            switch(prvDPCONTROL_DATA.batState)
            {
            case DPCONTROL_BAT_STATE_DISABLE:
                DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET);
                break;
            case DPCONTROL_BAT_STATE_ENABLE:
                DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET);
                break;
            }

            /* --- POWER PATH CONTROL --- */
            if(DRV_GPIO_Port_Init(DPCONTROL_GPIO_DISABLE_PORT) != DRV_GPIO_STATUS_OK)
                LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to initialize ppath control port\r\n");

            if(DRV_GPIO_Pin_Init(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, &controlPinConfig) != DRV_GPIO_STATUS_OK)
                LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to initialize ppath control pin\r\n");

            switch(prvDPCONTROL_DATA.pathState)
            {
            case DPCONTROL_PPATH_STATE_DISABLE:
                DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET);
                break;
            case DPCONTROL_PPATH_STATE_ENABLE:
                DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET);
                break;
            }

            /**********************************************************************
             * LATCH CONTROL INITIALIZATION
             **********************************************************************/
            latchPinConfig.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
            latchPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

            DRV_GPIO_Port_Init(DPCONTROL_LATCH_PORT);
            DRV_GPIO_Pin_Init(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, &latchPinConfig);

            /**********************************************************************
             * PROTECTION INPUTS (UV / OV / OC) INITIALIZATION
             **********************************************************************/
            protectionPinConfig.mode = DRV_GPIO_PIN_MODE_IT_RISING_FALLING;
            protectionPinConfig.pullState = DRV_GPIO_PIN_PULL_NOPULL;

            /* --- UNDER VOLTAGE --- */
            DRV_GPIO_Port_Init(CONF_DPCONTROL_UV_PORT);
            DRV_GPIO_RegisterCallback(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, prvDPCONTROL_UnderVoltageCB, CONF_DPCONTROL_UV_ISR_PRIO);
            DRV_GPIO_Pin_Init(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN, &protectionPinConfig);
            prvDPCONTROL_DATA.underVoltage = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);

            /* --- OVER VOLTAGE --- */
            DRV_GPIO_Port_Init(CONF_DPCONTROL_OV_PORT);
            DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, prvDPCONTROL_OverVoltageCB, CONF_DPCONTROL_OV_ISR_PRIO);
            DRV_GPIO_Pin_Init(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN, &protectionPinConfig);
            prvDPCONTROL_DATA.overVoltage = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);

            /* --- OVER CURRENT --- */
            DRV_GPIO_Port_Init(CONF_DPCONTROL_OC_PORT);
            DRV_GPIO_RegisterCallback(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, prvDPCONTROL_OverCurrentCB, CONF_DPCONTROL_OC_ISR_PRIO);
            DRV_GPIO_Pin_Init(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN, &protectionPinConfig);
            prvDPCONTROL_DATA.overCurrent = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);

            /**********************************************************************
             * PROTECTION THRESHOLD INITIALIZATION (UV / OV / OC VALUES)
             **********************************************************************/
            {
                uint8_t def;
                float ocVoltage;

                CONFIGURATION_GetParameter_Float("PROTECTIONS_OVOLTAGE_VALUE", &prvDPCONTROL_DATA.ovValue, &def);
                CONFIGURATION_GetParameter_Float("PROTECTIONS_UVOLTAGE_VALUE", &prvDPCONTROL_DATA.uvValue, &def);
                CONFIGURATION_GetParameter_Int("PROTECTIONS_OCURRENT_VALUE", &prvDPCONTROL_DATA.ocValue, &def);

                CONFIGURATION_GetParameter_Float("SENS_SHUNT", &prvDPCONTROL_DATA.shuntValue, &def);
                CONFIGURATION_GetParameter_Float("SENS_GAIN", &prvDPCONTROL_DATA.gainValue, &def);

                /* Apply thresholds to DAC */
                DRV_AOUT_SetVoltage(prvDPCONTROL_DATA.ovValue, DRV_AOUT_CHANNEL_C);
                DRV_AOUT_SetVoltage(prvDPCONTROL_DATA.uvValue, DRV_AOUT_CHANNEL_B);

                /* 1.625 V is the current sensing amplifier output offset */
                ocVoltage = 1.625f + (prvDPCONTROL_DATA.shuntValue * prvDPCONTROL_DATA.gainValue * (float)prvDPCONTROL_DATA.ocValue / 1000.0f);

                DRV_AOUT_SetVoltage(ocVoltage, DRV_AOUT_CHANNEL_A);

                LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Init: OV=%.2fV UV=%.2fV OC=%dmA | SHUNT=%.4f GAIN=%.2f\r\n", prvDPCONTROL_DATA.ovValue, prvDPCONTROL_DATA.uvValue, prvDPCONTROL_DATA.ocValue, prvDPCONTROL_DATA.shuntValue, prvDPCONTROL_DATA.gainValue);
            }

            LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "DPControl service initialized\r\n");

            prvDPCONTROL_DATA.state = DPCONTROL_STATE_SERVICE;
            xSemaphoreGive(prvDPCONTROL_DATA.initSig);
            break;

        case DPCONTROL_STATE_SERVICE:

            xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, portMAX_DELAY);

            /**********************************************************************
             * BATTERY CONTROL
             **********************************************************************/
            if(value & DPCONTROL_MASK_SET_BAT_STATE)
            {
                if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to take semaphore\r\n");
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                batState = prvDPCONTROL_DATA.batState;

                if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to return semaphore\r\n");
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                switch(batState)
                {
                case DPCONTROL_BAT_STATE_DISABLE:
                    if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to disable battery\r\n");
                    }
                    else
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Battery successfully disabled\r\n");
                    }
                    break;

                case DPCONTROL_BAT_STATE_ENABLE:
                    if(DRV_GPIO_Pin_SetState(DPCONTROL_BAT_DISABLE_PORT, DPCONTROL_BAT_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to enable battery\r\n");
                    }
                    else
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Battery successfully enabled\r\n");
                    }
                    break;
                }

                xSemaphoreGive(prvDPCONTROL_DATA.initSig);
            }

            /**********************************************************************
             * POWER PATH CONTROL
             **********************************************************************/
            if(value & DPCONTROL_MASK_SET_PPATH_STATE)
            {
                if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to take semaphore\r\n");
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                ppathState = prvDPCONTROL_DATA.pathState;

                if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to return semaphore\r\n");
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                switch(ppathState)
                {
                case DPCONTROL_PPATH_STATE_ENABLE:
                    if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to enable power path\r\n");
                    }
                    else
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Power path successfully enabled\r\n");
                    }
                    break;

                case DPCONTROL_PPATH_STATE_DISABLE:
                    if(DRV_GPIO_Pin_SetState(DPCONTROL_GPIO_DISABLE_PORT, DPCONTROL_GPIO_DISABLE_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to disable power path\r\n");
                    }
                    else
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Power path successfully disabled\r\n");
                    }
                    break;
                }

                xSemaphoreGive(prvDPCONTROL_DATA.initSig);
            }

            /**********************************************************************
             * PROTECTION LATCH CONTROL
             **********************************************************************/
            if(value & DPCONTROL_MASK_TRGER_LATCH)
            {
                if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_SET) != DRV_GPIO_STATUS_OK)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to reset latch\r\n");
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(5));

                    if(DRV_GPIO_Pin_SetState(DPCONTROL_LATCH_PORT, DPCONTROL_LATCH_PIN, DRV_GPIO_PIN_STATE_RESET) != DRV_GPIO_STATUS_OK)
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_WARNING, "Unable to reset latch\r\n");
                    }
                    else
                    {
                        LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Latch successfully reset\r\n");
                    }
                }

                xSemaphoreGive(prvDPCONTROL_DATA.initSig);
            }

            /**********************************************************************
             * PROTECTION EVENT HANDLING
             **********************************************************************/
            if(value & DPCONTROL_MASK_SET_UV_DETECTED)
            {
                drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_UV_PORT, CONF_DPCONTROL_UV_PIN);
                prvDPCONTROL_DATA.underVoltage = (dpcontrol_protection_state_t)pinState;

                if(pinState == DRV_GPIO_PIN_STATE_SET)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Under Voltage protection enabled\r\n");
                    CONTROL_StatusLinkSendMessage("uvoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
                else
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Under Voltage protection disabled\r\n");
                    CONTROL_StatusLinkSendMessage("uvoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
            }

            if(value & DPCONTROL_MASK_SET_OV_DETECTED)
            {
                drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OV_PORT, CONF_DPCONTROL_OV_PIN);
                prvDPCONTROL_DATA.overVoltage = (dpcontrol_protection_state_t)pinState;

                if(pinState == DRV_GPIO_PIN_STATE_SET)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Over Voltage protection enabled\r\n");
                    CONTROL_StatusLinkSendMessage("ovoltage enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
                else
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Over Voltage protection disabled\r\n");
                    CONTROL_StatusLinkSendMessage("ovoltage disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
            }

            if(value & DPCONTROL_MASK_SET_OC_DETECTED)
            {
                drv_gpio_pin_state_t pinState = DRV_GPIO_Pin_ReadState(CONF_DPCONTROL_OC_PORT, CONF_DPCONTROL_OC_PIN);
                prvDPCONTROL_DATA.overCurrent = (dpcontrol_protection_state_t)pinState;

                if(pinState == DRV_GPIO_PIN_STATE_SET)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Over Current protection enabled\r\n");
                    CONTROL_StatusLinkSendMessage("ocurrent enabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
                else
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Over Current protection disabled\r\n");
                    CONTROL_StatusLinkSendMessage("ocurrent disabled\r\n", CONTROL_STATUS_MESSAGE_TYPE_ACTION, 1000);
                }
            }

            /**********************************************************************
             * PROTECTION THRESHOLD CONTROL
             **********************************************************************/
            if(value & DPCONTROL_MASK_SET_OV_VALUE)
            {
                float ov;

                if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                ov = prvDPCONTROL_DATA.ovValue;

                xSemaphoreGive(prvDPCONTROL_DATA.guard);

                if(DRV_AOUT_SetVoltage(ov, DRV_AOUT_CHANNEL_C) != DRV_AOUT_STATUS_OK)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to set Over Voltage\r\n");
                }
                else
                {
                    CONFIGURATION_SetParameter_Float("PROTECTIONS_OVOLTAGE_VALUE", ov, 1000);
                    xSemaphoreGive(prvDPCONTROL_DATA.initSig);
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "OV threshold set to %.3f V\r\n", ov);
                }
            }

            if(value & DPCONTROL_MASK_SET_UV_VALUE)
            {
                float uv;

                if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                uv = prvDPCONTROL_DATA.uvValue;

                xSemaphoreGive(prvDPCONTROL_DATA.guard);

                if(DRV_AOUT_SetVoltage(uv, DRV_AOUT_CHANNEL_B) != DRV_AOUT_STATUS_OK)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to set Under Voltage\r\n");
                }
                else
                {
                    CONFIGURATION_SetParameter_Float("PROTECTIONS_UVOLTAGE_VALUE", uv, 1000);
                    xSemaphoreGive(prvDPCONTROL_DATA.initSig);
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Under Voltage threshold set to %.3f V\r\n", uv);
                }
            }

            if(value & DPCONTROL_MASK_SET_OC_VALUE)
            {
                int32_t oc;
                float voltageValue;

                if(xSemaphoreTake(prvDPCONTROL_DATA.guard, portMAX_DELAY) != pdTRUE)
                {
                    prvDPCONTROL_DATA.state = DPCONTROL_STATE_ERROR;
                    break;
                }

                oc = prvDPCONTROL_DATA.ocValue;

                xSemaphoreGive(prvDPCONTROL_DATA.guard);

                /* 1.625 V is the current sensing amplifier output offset */
                voltageValue = 1.625f + (prvDPCONTROL_DATA.shuntValue * prvDPCONTROL_DATA.gainValue * (float)oc / 1000.0f);

                if(DRV_AOUT_SetVoltage(voltageValue, DRV_AOUT_CHANNEL_A) != DRV_AOUT_STATUS_OK)
                {
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_ERROR, "Unable to set Over Current threshold\r\n");
                }
                else
                {
                    CONFIGURATION_SetParameter_Int("PROTECTIONS_OCURRENT_VALUE", oc, 1000);
                    xSemaphoreGive(prvDPCONTROL_DATA.initSig);
                    LOGGING_Write("DPControl", LOGGING_MSG_TYPE_INFO, "Over Current threshold set to %d mA\r\n", oc);
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

/**
 * @defgroup DPCONTROL_PUBLIC_FUNCTIONS DPControl public functions
 * @{
 */

/**
 * @brief Initialize DPControl service.
 *
 * Creates the synchronization objects and starts the DPControl service task.
 *
 * @param initTimeout Initialization timeout in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_Init(uint32_t initTimeout)
{
    memset(&prvDPCONTROL_DATA, 0, sizeof(dpcontrol_data_t));

    prvDPCONTROL_DATA.batState = DPCONTROL_BAT_STATE_ENABLE;
    prvDPCONTROL_DATA.pathState = DPCONTROL_PPATH_STATE_ENABLE;

    prvDPCONTROL_DATA.initSig = xSemaphoreCreateBinary();

    if(prvDPCONTROL_DATA.initSig == NULL) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.guard = xSemaphoreCreateMutex();

    if(prvDPCONTROL_DATA.guard == NULL) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.state = DPCONTROL_STATE_INIT;

    if(xTaskCreate(prvDPCONTROL_TaskFunc, DPCONTROL_TASK_NAME, DPCONTROL_TASK_STACK, NULL, DPCONTROL_TASK_PRIO, &prvDPCONTROL_DATA.taskHandle) != pdPASS) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(initTimeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Set battery path state.
 *
 * @param state Desired battery state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetBatState(dpcontrol_bat_state_t state, uint32_t timeout)
{
    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.batState = state;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_BAT_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get battery path state.
 *
 * @param state Pointer to store battery state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetBatState(dpcontrol_bat_state_t* state, uint32_t timeout)
{
    if(state == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *state = prvDPCONTROL_DATA.batState;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Set primary power path state.
 *
 * @param state Desired power path state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetPPathState(dpcontrol_ppath_state_t state, uint32_t timeout)
{
    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.pathState = state;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_PPATH_STATE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdPASS) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get primary power path state.
 *
 * @param state Pointer to store power path state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetPPathState(dpcontrol_ppath_state_t* state, uint32_t timeout)
{
    if(state == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *state = prvDPCONTROL_DATA.pathState;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get under-voltage protection state.
 *
 * @param state Pointer to store protection state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetUVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
    if(state == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *state = prvDPCONTROL_DATA.underVoltage;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get over-voltage protection state.
 *
 * @param state Pointer to store protection state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOVoltageState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
    if(state == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *state = prvDPCONTROL_DATA.overVoltage;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get over-current protection state.
 *
 * @param state Pointer to store protection state
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOCurrentState(dpcontrol_protection_state_t* state, uint32_t timeout)
{
    if(state == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *state = prvDPCONTROL_DATA.overCurrent;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Trigger protection latch.
 *
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_LatchTriger(uint32_t timeout)
{
    (void)timeout;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_TRGER_LATCH, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Set over-voltage protection threshold.
 *
 * @param value Over-voltage threshold in volts
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetOVValue(float value, uint32_t timeout)
{
    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.ovValue = value;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OV_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Set under-voltage protection threshold.
 *
 * @param value Under-voltage threshold in volts
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetUVValue(float value, uint32_t timeout)
{
    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.uvValue = value;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_UV_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Set over-current protection threshold.
 *
 * @param value Over-current threshold in milliamperes
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_SetOCValue(int32_t value, uint32_t timeout)
{
    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    prvDPCONTROL_DATA.ocValue = value;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    if(xTaskNotify(prvDPCONTROL_DATA.taskHandle, DPCONTROL_MASK_SET_OC_VALUE, eSetBits) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get over-voltage protection threshold.
 *
 * @param value Pointer to store over-voltage threshold
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOVValue(float* value, uint32_t timeout)
{
    if(value == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *value = prvDPCONTROL_DATA.ovValue;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get under-voltage protection threshold.
 *
 * @param value Pointer to store under-voltage threshold
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetUVValue(float* value, uint32_t timeout)
{
    if(value == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *value = prvDPCONTROL_DATA.uvValue;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
}

/**
 * @brief Get over-current protection threshold.
 *
 * @param value Pointer to store over-current threshold
 * @param timeout Timeout for operation in milliseconds
 * @retval ::DPCONTROL_STATUS_OK or ::DPCONTROL_STATUS_ERROR
 */
dpcontrol_status_t DPCONTROL_GetOCValue(int32_t* value, uint32_t timeout)
{
    if(value == NULL) return DPCONTROL_STATUS_ERROR;

    if(xSemaphoreTake(prvDPCONTROL_DATA.guard, pdMS_TO_TICKS(timeout)) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    *value = prvDPCONTROL_DATA.ocValue;

    if(xSemaphoreGive(prvDPCONTROL_DATA.guard) != pdTRUE) return DPCONTROL_STATUS_ERROR;

    return DPCONTROL_STATUS_OK;
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
