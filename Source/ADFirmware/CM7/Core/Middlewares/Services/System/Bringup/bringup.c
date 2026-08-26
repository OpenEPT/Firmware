/**
 ******************************************************************************
 * @file    bringup.c
 *
 * @brief   Hardware bring-up service implementation.
 *
 *          Bring-up service performs initial hardware
 *          validation and peripheral communication checks
 *          during early startup phase.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    May 2026
 ******************************************************************************
 */

#ifndef CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_C_
#define CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_C_

#include "bringup.h"
#include "stm32h7xx_hal.h"

#if(CONF_BRINGUP_ENABLE == 1)

#include "FreeRTOS.h"
#include "task.h"

#include "logging.h"
#include "drv_gpio.h"
#include "drv_timer.h"
#include "system.h"
#include "lan8742.h"
#include "globalConfig.h"
#include "lwipopts.h"
#include "m24c32.h"
#include "drv_ain.h"

/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @defgroup BRINGUP_SERVICE Bring-up service
 * @{
 */

/**
 * @defgroup BRINGUP_PRIVATE_DATA Bring-up service private data
 * @{
 */

static TaskHandle_t prvBRINGUP_TASK_HANDLE; /*!< Bring-up task handle */

static uint8_t prvBRINGUP_STATUS = 0;
static uint32_t prvBRINGUP_STATUS_ERROR = 0;


extern uint8_t                 	NETWORK_MAC_ADDR[6];

extern ETH_HandleTypeDef 		HETH;
extern lan8742_Object_t 		LAN8742;
extern ETH_DMADescTypeDef 		DMARxDscrTab[ETH_RX_DESC_CNT];  /* Ethernet Rx DMA Descriptors */
extern ETH_DMADescTypeDef 		DMATxDscrTab[ETH_TX_DESC_CNT];   /* Ethernet Tx DMA Descriptors */
extern lan8742_IOCtx_t  		LAN8742_IOCtx;

/**
 * @}
 */

/**
 * @defgroup BRINGUP_PRIVATE_FUNCTIONS Bring-up service private functions
 * @{
 */

/**
 * @brief Set RGB LED PWM values
 *
 * This function updates RGB LED PWM duty cycle values.
 *
 * @param r Red channel intensity
 * @param g Green channel intensity
 * @param b Blue channel intensity
 *
 * @retval None
 */
static void prvBRINGUP_RGB_Set(uint8_t r, uint8_t g, uint8_t b)
{
    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, b, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 4);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, g, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 5);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, r, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 6);
    }
}

/**
 * @brief Perform GPIO validation test
 *
 * This function initializes RGB GPIO pins and
 * performs simple LED sequence validation.
 *
 * @retval None
 */
static void prvBRINGUP_GPIO_Test(void)
{
    drv_gpio_pin_init_conf_t gpioConf;

    gpioConf.mode = DRV_GPIO_PIN_MODE_OUTPUT_PP;
    gpioConf.pullState = DRV_GPIO_PIN_PULL_NOPULL;

    if(DRV_GPIO_Port_Init(DRV_GPIO_PORT_E) != DRV_GPIO_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 0);
    }

    if(DRV_GPIO_Pin_Init(DRV_GPIO_PORT_E, 11, &gpioConf) != DRV_GPIO_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 1);
    }

    if(DRV_GPIO_Pin_Init(DRV_GPIO_PORT_E, 13, &gpioConf) != DRV_GPIO_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 2);
    }

    if(DRV_GPIO_Pin_Init(DRV_GPIO_PORT_E, 14, &gpioConf) != DRV_GPIO_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 3);
    }

    for(uint8_t counter = 0; counter < 3; counter++)
    {
        DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 11, DRV_GPIO_PIN_STATE_RESET);
        DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 13, DRV_GPIO_PIN_STATE_RESET);
        DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 14, DRV_GPIO_PIN_STATE_RESET);

        switch(counter)
        {
        case 0:
            DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 11, DRV_GPIO_PIN_STATE_SET);
            break;

        case 1:
            DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 14, DRV_GPIO_PIN_STATE_SET);
            break;

        case 2:
            DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 13, DRV_GPIO_PIN_STATE_SET);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 11, DRV_GPIO_PIN_STATE_RESET);
    DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 13, DRV_GPIO_PIN_STATE_RESET);
    DRV_GPIO_Pin_SetState(DRV_GPIO_PORT_E, 14, DRV_GPIO_PIN_STATE_RESET);
}

/**
 * @brief Perform RGB LED PWM validation test
 *
 * This function initializes timer peripheral and
 * validates RGB LED PWM control sequence.
 *
 * @retval None
 */
static void prvBRINGUP_RGB_Test(void)
{
    drv_timer_channel_config_t pwmTimerChConfig;
    drv_timer_config_t pwmTimerConfig;

    if(DRV_Timer_Init() != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 0);
    }

    pwmTimerConfig.mode = DRV_TIMER_COUNTER_MODE_UP;
    pwmTimerConfig.prescaler = 2000;
    pwmTimerConfig.preload = DRV_TIMER_PRELOAD_DISABLE;
    pwmTimerConfig.div = DRV_TIMER_DIV_1;
    pwmTimerConfig.period = 256;

    if(DRV_Timer_Init_Instance(DRV_TIMER_1, &pwmTimerConfig) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 0);
    }

    pwmTimerChConfig.mode = DRV_TIMER_CHANNEL_MODE_PWM1;

    if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 1);
    }

    if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 2);
    }

    if(DRV_Timer_Channel_Init(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, &pwmTimerChConfig) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 3);
    }

    /**
     * Turn OFF all RGB channels
     */
    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 4);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 5);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 6);
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    for(uint8_t counter = 0; counter < 3; counter++)
    {
        switch(counter)
        {
        case 0:
            r = 255;
            g = 0;
            b = 0;
            break;

        case 1:
            r = 0;
            g = 0;
            b = 255;
            break;

        case 2:
            r = 0;
            g = 255;
            b = 0;
            break;
        }

        if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, b, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
        {
            prvBRINGUP_STATUS = 1;
            prvBRINGUP_STATUS_ERROR |= (1 << 5);
        }

        if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, g, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
        {
            prvBRINGUP_STATUS = 1;
            prvBRINGUP_STATUS_ERROR |= (1 << 6);
        }

        if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, r, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
        {
            prvBRINGUP_STATUS = 1;
            prvBRINGUP_STATUS_ERROR |= (1 << 7);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /**
     * Turn OFF all RGB channels
     */
    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_2, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 4);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_3, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 5);
    }

    if(DRV_Timer_Channel_PWM_Start(DRV_TIMER_1, DRV_TIMER_CHANNEL_4, 0, portMAX_DELAY) != DRV_TIMER_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 6);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
}

/**
 * @brief Perform LAN9252 / Ethernet PHY validation test
 *
 * This function initializes Ethernet PHY layer and
 * validates Ethernet link configuration.
 *
 * @retval None
 */
static uint8_t prvBRINGUP_LAN9252_Test(void)
{
    uint32_t duplex = 0;
    uint32_t speed = 0;

    int32_t phyLinkState = 0;
    /**
     * ---------------------------------------------------------
     * PHY initialization
     * ---------------------------------------------------------
     */
    uint8_t macaddress[6]= {
    			NETWORK_MAC_ADDR[0],
    			NETWORK_MAC_ADDR[1],
    			NETWORK_MAC_ADDR[2],
    			NETWORK_MAC_ADDR[3],
    			NETWORK_MAC_ADDR[4],
    			NETWORK_MAC_ADDR[5]};
	HETH.Instance = ETH;
	HETH.Init.MACAddr = macaddress;
	HETH.Init.MediaInterface = HAL_ETH_RMII_MODE;
	HETH.Init.RxDesc = DMARxDscrTab;
	HETH.Init.TxDesc = DMATxDscrTab;
	HETH.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;

	/* configure ethernet peripheral (GPIOs, clocks, MAC, DMA) */
	HAL_ETH_Init(&HETH);
    LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);

    if(LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 10);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "LAN8742 initialization failed\r\n");

        return 1;
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "LAN8742 initialized\r\n");

    /**
     * ---------------------------------------------------------
     * Link state validation
     * ---------------------------------------------------------
     */

    phyLinkState = LAN8742_GetLinkState(&LAN8742);

    if(phyLinkState <= LAN8742_STATUS_LINK_DOWN)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 11);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "Ethernet link down\r\n");

        return 1;
    }

    switch(phyLinkState)
    {
    case LAN8742_STATUS_100MBITS_FULLDUPLEX:
        duplex = ETH_FULLDUPLEX_MODE;
        speed = ETH_SPEED_100M;
        break;

    case LAN8742_STATUS_100MBITS_HALFDUPLEX:
        duplex = ETH_HALFDUPLEX_MODE;
        speed = ETH_SPEED_100M;
        break;

    case LAN8742_STATUS_10MBITS_FULLDUPLEX:
        duplex = ETH_FULLDUPLEX_MODE;
        speed = ETH_SPEED_10M;
        break;

    case LAN8742_STATUS_10MBITS_HALFDUPLEX:
        duplex = ETH_HALFDUPLEX_MODE;
        speed = ETH_SPEED_10M;
        break;

    default:
        duplex = ETH_FULLDUPLEX_MODE;
        speed = ETH_SPEED_100M;
        break;
    }

    return 0;
}

/**
 * @brief Perform EEPROM communication validation test
 *
 * This function initializes EEPROM driver and
 * validates basic read/write communication.
 *
 * @retval 0 if successful, 1 otherwise
 */
static uint8_t prvBRINGUP_EEPROM_Test(void)
{
    uint8_t txData[8] = {0x11, 0x22, 0x33, 0x44,
                         0x55, 0x66, 0x77, 0x88};

    uint8_t rxData[8] = {0};


    if(M24C32_Init() != M24C32_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 20);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM initialization failed\r\n");

        return 1;
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "EEPROM initialized\r\n");


    if(M24C32_Ping(1000) != M24C32_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 21);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM communication failed\r\n");

        return 1;
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "EEPROM communication established\r\n");


    if(M24C32_Write(0x0000, txData, sizeof(txData), 1000) != M24C32_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 22);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM write failed\r\n");

        return 1;
    }


    if(M24C32_Read(0x0000, rxData, sizeof(rxData), 1000) != M24C32_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 23);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM read failed\r\n");

        return 1;
    }


    if(memcmp(txData, rxData, sizeof(txData)) != 0)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 24);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM data validation failed\r\n");

        return 1;
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "EEPROM validation successful\r\n");

    return 0;
}

/**
 * @brief Perform analog input validation test
 *
 * This function initializes analog input subsystem
 * and validates ADC communication and acquisition.
 *
 * @retval 0 if successful, 1 otherwise
 */
static uint8_t prvBRINGUP_AIN_Test(void)
{
    if(DRV_AIN_Init(DRV_AIN_ADC_3, NULL) != DRV_AIN_STATUS_OK)
    {
        prvBRINGUP_STATUS = 1;
        prvBRINGUP_STATUS_ERROR |= (1 << 30);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "AIN initialization failed\r\n");

        return 1;
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "AIN initialized\r\n");

    return 0;
}

/**
 * @brief Bring-up task
 *
 * This task performs:
 *  - RGB LED initialization
 *  - Acquisition LED initialization
 *  - Logging service initialization
 *  - Network communication checks
 *  - ADC communication checks
 *  - DAC communication checks
 *  - Protection checks
 *  - EEPROM communication checks
 *
 * @param args Task arguments
 *
 * @retval None
 */
static void prvBRINGUP_Task(void* args)
{
    (void)args;

    prvBRINGUP_STATUS = 0;
    prvBRINGUP_STATUS_ERROR = 0;

    prvBRINGUP_GPIO_Test();

    prvBRINGUP_RGB_Test();

    /** Logging service initialization */

    if(LOGGING_Init(2000) != LOGGING_STATUS_OK)
    {
    	prvBRINGUP_RGB_Set(255, 0, 0);
        vTaskDelete(NULL);
    }

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "Logging initialized \r\n");
	prvBRINGUP_RGB_Set(0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /** LAN9252 and network validation */

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "LAN9252 test started ...\r\n");
	prvBRINGUP_RGB_Set(0, 0, 0);

    if( prvBRINGUP_LAN9252_Test() != 0)
    {
    	prvBRINGUP_RGB_Set(255, 0, 0);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "LAN9252 test failed\r\n");
        vTaskDelete(NULL);
    }

	prvBRINGUP_RGB_Set(0, 255, 0);
    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "LAN9252 test successfully done\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /**  EEPROM communication validation */

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "EEPROM test started ...\r\n");
	prvBRINGUP_RGB_Set(0, 0, 0);

    if(prvBRINGUP_EEPROM_Test() != 0)
    {
    	prvBRINGUP_RGB_Set(255, 0, 0);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "EEPROM Test failed \r\n");
        vTaskDelete(NULL);
    }

	prvBRINGUP_RGB_Set(0, 255, 0);
    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "EEPROM test successfully done\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /**  ADC communication validation */

    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "ADC test started ...\r\n");
	prvBRINGUP_RGB_Set(0, 0, 0);

    if(prvBRINGUP_AIN_Test() != 0)
    {
    	prvBRINGUP_RGB_Set(255, 0, 0);

        LOGGING_Write("BringUp", LOGGING_MSG_TYPE_ERROR, "ADC Test failed \r\n");
        vTaskDelete(NULL);
    }

	prvBRINGUP_RGB_Set(0, 255, 0);
    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "ADC test successfully done\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000));

    /**
     * ---------------------------------------------------------
     * DAC communication validation
     * ---------------------------------------------------------
     */

    /* TODO: DAC validation */

    /**
     * ---------------------------------------------------------
     * Protection circuitry validation
     * ---------------------------------------------------------
     */

    /* TODO: Protection validation */



    LOGGING_Write("BringUp", LOGGING_MSG_TYPE_INFO, "Bring-up finished\r\n");

    vTaskDelete(NULL);
}

/**
 * @}
 */

/**
 * @defgroup BRINGUP_PUBLIC_FUNCTIONS Bring-up service public functions
 * @{
 */

bringup_status_t BRINGUP_Init(void)
{
    if(xTaskCreate(prvBRINGUP_Task,
                   BRINGUP_TASK_NAME,
                   BRINGUP_TASK_STACK_SIZE,
                   NULL,
                   BRINGUP_TASK_PRIO,
                   &prvBRINGUP_TASK_HANDLE) != pdTRUE)
    {
        return BRINGUP_STATUS_ERROR;
    }

    return BRINGUP_STATUS_OK;
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

#endif /* CONF_BRINGUP_ENABLE */

#endif /* CORE_MIDDLEWARES_SERVICES_SYSTEM_BRINGUP_BRINGUP_C_ */
