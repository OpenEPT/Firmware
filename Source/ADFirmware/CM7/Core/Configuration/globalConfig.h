/**
 ******************************************************************************
 * @file    globalConfig.h
 *
 * @brief   Global system configuration file.
 *
 *          This file contains all compile-time configuration macros used
 *          across driver, middleware, and application layers.
 *
 *          Configuration groups are organized per module/service in order
 *          to simplify scalability, maintenance, and feature enabling/
 *          disabling.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2023
 ******************************************************************************
 */

#ifndef CORE_CONFIGURATION_GLOBALCONFIG_H_
#define CORE_CONFIGURATION_GLOBALCONFIG_H_


/**
 * @defgroup CONFIGURATION System Configuration
 * @{
 */

/**
 * @defgroup GLOBALCONFIG_CONFIG Global system configuration
 *
 * @brief Global compile-time system configuration
 *
 * This group contains all global configuration macros
 * used across driver, middleware, and application layers.
 *
 * Configuration parameters are grouped per module/service
 * in order to simplify:
 *  - Feature scalability
 *  - Service enabling/disabling
 *  - Platform customization
 *  - System maintenance
 *  - Compile-time optimization
 *
 * Every service configuration group starts with:
 *
 * @code
 * CONF_<SERVICE_NAME>_ENABLE
 * @endcode
 *
 * macro which is used to enable or disable
 * corresponding service during compilation.
 *
 * @{
 */

/**
 * @defgroup GLOBALCONFIG_DRIVER_CONFIG Driver layer configuration
 * @{
 */


/**
 * @defgroup GLOBALCONFIG_AIN_CONFIG Analog input driver configuration
 * @{
 */

#define CONF_AIN_ENABLE                         1           /*!< Enable/disable analog input driver */
#define CONF_AIN_MAX_BUFFER_SIZE                500         /*!< Maximum ADC buffer size */
#define CONF_AIN_ADC_BUFFER_OFFSET              4           /*!< ADC buffer offset */
#define CONF_AIN_ADC_BUFFER_MARKER              0xA5A5      /*!< ADC synchronization marker */
#define CONF_AIN_MAX_BUFFER_NO                  2           /*!< Maximum number of ADC buffers */
#define CONF_DRV_AIN_ADC_TIM_INPUT_CLK          200000000   /*!< ADC timer input clock frequency */
/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_TIMER_CONFIG Timer driver configuration
 * @{
 */

#define CONF_DRV_TIMER_ENABLE                   1           /*!< Enable/disable timer driver */
#define CONF_DRV_TIMER_MAX_NUMBER_OF_TIMERS     3           /*!< Maximum number of timer instances */
#define CONF_DRV_TIMER_MAX_NUMBER_OF_CHANNELS   3           /*!< Maximum number of timer channels */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_GPIO_CONFIG GPIO driver configuration
 * @{
 */

#define CONF_GPIO_ENABLE                        1           /*!< Enable/disable GPIO driver */
#define CONF_GPIO_PORT_MAX_NUMBER               10          /*!< Maximum number of GPIO ports */
#define CONF_GPIO_PIN_MAX_NUMBER                16          /*!< Maximum number of GPIO pins per port */
#define CONF_GPIO_INTERRUPTS_MAX_NUMBER         16          /*!< Maximum number of GPIO interrupts */

/**
 * @}
 */



/**
 * @defgroup GLOBALCONFIG_UART_CONFIG UART driver configuration
 * @{
 */

#define CONF_UART_ENABLE                        1           /*!< Enable/disable UART driver */
#define CONF_UART_INSTANCES_MAX_NUMBER          5           /*!< Maximum number of UART instances */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_SPI_CONFIG SPI driver configuration
 * @{
 */

#define CONF_SPI_ENABLE                         1           /*!< Enable/disable SPI driver */
#define CONF_SPI_INSTANCES_MAX_NUMBER           2           /*!< Maximum number of SPI instances */

/**
 * @}
 */

/**
 * @}
 */


/**
 * @defgroup GLOBALCONFIG_MIDDLEWARE_CONFIG Middleware layer configuration
 * @{
 */

/**
 * @defgroup GLOBALCONFIG_BRINGUP_CONFIG Bringup code configuration
 * @{
 */
#define CONF_BRINGUP_ENABLE                       0
#define CONF_BRINGUP_CHECK_ENABLE                 1       /*!< Enable/disable hardware bring-up task */
#define CONF_BRINGUP_TASK_NAME                    "BringUp"
#define CONF_BRINGUP_TASK_PRIO                    6
#define CONF_BRINGUP_TASK_STACK_SIZE              1024

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_SSTREAM_CONFIG Sample stream service configuration
 * @{
 */


#define CONF_SSTREAM_CONTROL_TASK_NAME          "Stream control" /*!< Stream control task name */
#define CONF_SSTREAM_CONTROL_TASK_PRIO          4           /*!< Stream control task priority */
#define CONF_SSTREAM_CONTROL_TASK_STACK_SIZE    1024        /*!< Stream control task stack size */

#define CONF_SSTREAM_STREAM_TASK_NAME           "Stream"    /*!< Stream task name */
#define CONF_SSTREAM_STREAM_TASK_PRIO           4           /*!< Stream task priority */
#define CONF_SSTREAM_STREAM_TASK_STACK_SIZE     1024        /*!< Stream task stack size */

#define CONF_SSTREAM_CONNECTIONS_MAX_NO         2           /*!< Maximum number of stream connections */

#define CONF_SSTREAM_AIN_DEFAULT_RESOLUTION     16          /*!< Default ADC resolution */
#define CONF_SSTREAM_AIN_DEFAULT_SAMPLES_NO     250         /*!< Default ADC samples count */
#define CONF_SSTREAM_AIN_DEFAULT_CLOCK_DIV      1           /*!< Default ADC clock divider */
#define CONF_SSTREAM_AIN_DEFAULT_CH_SAMPLE_TIME 2           /*!< Default ADC channel sample time */
#define CONF_SSTREAM_AIN_DEFAULT_CH_AVG_RATIO   1           /*!< Default ADC averaging ratio */
#define CONF_SSTREAM_AIN_DEFAULT_SAMPLE_TIME    1000        /*!< Default sample time */
#define CONF_SSTREAM_AIN_DEFAULT_PRESCALER      49999       /*!< Default ADC timer prescaler */
#define CONF_SSTREAM_AIN_DEFAULT_PERIOD         3           /*!< Default ADC timer period */

#define CONF_SSTREAM_AIN_VOLTAGE_CHANNEL        1           /*!< ADC voltage channel */
#define CONF_SSTREAM_AIN_CURRENT_CHANNEL        2           /*!< ADC current channel */

#define CONF_SSTREAM_LED_PORT                   3           /*!< Stream status LED port */
#define CONF_SSTREAM_LED_PIN                    11          /*!< Stream status LED pin */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_SYSTEM_CONFIG System service configuration
 * @{
 */


#define CONF_SYSTEM_TASK_NAME                   "System task" /*!< System task name */
#define CONF_SYSTEM_TASK_PRIO                   5           /*!< System task priority */
#define CONF_SYSTEM_TASK_STACK_SIZE             1024        /*!< System task stack size */

#define CONF_SYSTEM_DEFAULT_DEVICE_NAME         "ACQ Device" /*!< Default device name */
#define CONF_SYSTEM_DEFAULT_DEVICE_NAME_MAX     50          /*!< Maximum device name length */

#define CONF_SYSTEM_RGB_DEFAULT_BRIGHTNESS      50          /*!< Default RGB LED brightness */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_NETWORK_CONFIG Network service configuration
 * @{
 */

#define CONF_NETWORK_ENABLE                     1           /*!< Enable/disable network service */

#define CONF_NETWORK_TASK_NAME                  "Network Task" /*!< Network task name */
#define CONF_NETWORK_TASK_PRIO                  3           /*!< Network task priority */
#define CONF_NETWORK_TASK_STACK_SIZE            1024        /*!< Network task stack size */

#define CONF_NETWORK_DEVICE_MAC_ADDRESS         "00:11:22:33:44:55" /*!< Default MAC address */
#define CONF_NETWORK_DEVICE_IP_ADDRESS          "192.168.1.111" /*!< Default IP address */
#define CONF_NETWORK_DEVICE_IP_MASK             "255.255.255.0" /*!< Default IP mask */
#define CONF_NETWORK_DEVICE_IP_GW               "192.168.1.1" /*!< Default gateway address */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_CHARGER_CONFIG Charger service configuration
 * @{
 */

#define CONF_CHARGER_ENABLE                     0           /*!< Enable/disable charger service */

#define CONF_CHARGER_TASK_NAME                  "Charger Task" /*!< Charger task name */
#define CONF_CHARGER_STACK_SIZE                 1024        /*!< Charger task stack size */
#define CONF_CHARGER_PRIO                       3           /*!< Charger task priority */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_LOGGING_CONFIG Logging service configuration
 * @{
 */

#define CONF_LOGGING_ENABLE                     1           /*!< Enable/disable logging service */

#define CONF_LOGGING_TASK_NAME                  "LOG Task"  /*!< Logging task name */
#define CONF_LOGGING_STACK_SIZE                 1024        /*!< Logging task stack size */
#define CONF_LOGGING_PRIO                       3           /*!< Logging task priority */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_CONTROL_CONFIG Control service configuration
 * @{
 */

#define CONF_CONTROL_ENABLE                     1           /*!< Enable/disable control service */

#define CONF_CONTROL_TASK_NAME                  "Control Task" /*!< Control task name */
#define CONF_CONTROL_PRIO                       4           /*!< Control task priority */
#define CONF_CONTROL_STACK_SIZE                 4096        /*!< Control task stack size */

#define CONF_CONTROL_BUFFER_SIZE                1024        /*!< Control command buffer size */
#define CONF_CONTROL_SERVER_PORT                5000        /*!< Control TCP server port */

#define CONF_CONTROL_RESPONSE_OK_STATUS_MSG     "OK"        /*!< Control OK response string */
#define CONF_CONTROL_RESPONSE_ERROR_STATUS_MSG  "ERROR"     /*!< Control ERROR response string */

#define CONF_CONTROL_STATUS_LINK_MAX_NO         3           /*!< Maximum number of status links */

#define CONF_CONTROL_STATUS_LINK_TASK_NAME      "Status Link Task" /*!< Status link task name */
#define CONF_CONTROL_STATUS_LINK_PRIO           5           /*!< Status link task priority */
#define CONF_CONTROL_STATUS_LINK_STACK_SIZE     1024        /*!< Status link task stack size */

#define CONF_CONTROL_STATUS_MESSAGES_MAX_NO     20          /*!< Maximum number of status messages */

/**
 * @}
 */


/**
 * @defgroup GLOBALCONFIG_ENERGY_DEBUGGER_CONFIG Energy debugger configuration
 * @{
 */

#define CONF_ENERGY_DEBUGGER_ENABLE                     1       /*!< Enable/disable energy debugger service */

#define CONF_ENERGY_DEBUGGER_TASK_NAME                  "Energy Debugger Task" /*!< Energy debugger task name */
#define CONF_ENERGY_DEBUGGER_TASK_PRIO                  5       /*!< Energy debugger task priority */
#define CONF_ENERGY_DEBUGGER_STACK_SIZE                 1024    /*!< Energy debugger task stack size */

#define CONF_ENERGY_DEBUGGER_BUTTON_PORT                3       /*!< User button GPIO port */
#define CONF_ENERGY_DEBUGGER_BUTTON_PIN                 14      /*!< User button GPIO pin */
#define CONF_ENERGY_DEBUGGER_BUTTON_ISR_PRIO            5       /*!< User button interrupt priority */

#define CONF_ENERGY_DEBUGGER_ID_QUEUE_LENGTH            100     /*!< Maximum ID queue length */
#define CONF_ENERGY_DEBUGGER_MESSAGE_BUFFER_LENGTH      100     /*!< Internal message buffer length */
#define CONF_ENERGY_DEBUGGER_TCP_MESSAGE_BUFFER_LENGTH  200     /*!< TCP message buffer length */

#define CONF_ENERGY_DEBUGGER_EBP_NAMES_QUEUE_LENGTH     10      /*!< EBP names queue length */
#define CONF_ENERGY_DEBUGGER_EBP_QUEUE_LENGTH           10      /*!< EBP queue length */

#define CONF_ENERGY_DEBUGGER_MAX_CONNECTIONS            3       /*!< Maximum number of TCP connections */

/**
 * @}
 */

/**
 * @defgroup GLOBALCONFIG_EEZ_DIB_CONFIG EEZ DIB service configuration
 * @{
 */

#define CONF_EEZ_DIB_ENABLE                 0

#define CONF_EEZ_DIB_TASK_NAME              "Eez Dib Task"
#define CONF_EEZ_DIB_TASK_PRIO              6
#define CONF_EEZ_DIB_STACK_SIZE             1024
#define CONF_EEZ_DIB_ID_QUEUE_LENGTH        10

/**
 * @}
 */


/**
 * @defgroup GLOBALCONFIG_DPCONTROL_CONFIG DPControl service configuration
 * @{
 */

#define CONF_DPCONTROL_ENABLE                   1       /*!< Enable/disable DPControl service */

#define CONF_DPCONTROL_TASK_NAME                "DPControl" /*!< DPControl task name */
#define CONF_DPCONTROL_TASK_STACK_SIZE          1024    /*!< DPControl task stack size */
#define CONF_DPCONTROL_TASK_PRIO                3       /*!< DPControl task priority */

#define CONF_DPCONTROL_LOAD_DISABLE_PORT        0       /*!< Load disable GPIO port */
#define CONF_DPCONTROL_LOAD_DISABLE_PIN         6       /*!< Load disable GPIO pin */

#define CONF_DPCONTROL_GPIO_DISABLE_PORT        6       /*!< GPIO disable control port */
#define CONF_DPCONTROL_GPIO_DISABLE_PIN         14      /*!< GPIO disable control pin */

#define CONF_DPCONTROL_BAT_DISABLE_PORT         1       /*!< Battery disable GPIO port */
#define CONF_DPCONTROL_BAT_DISABLE_PIN          5       /*!< Battery disable GPIO pin */

#define CONF_DPCONTROL_LATCH_PORT               3       /*!< Protection latch GPIO port */
#define CONF_DPCONTROL_LATCH_PIN                0       /*!< Protection latch GPIO pin */

#define CONF_DPCONTROL_UV_PORT                  4       /*!< Under-voltage detection GPIO port */
#define CONF_DPCONTROL_UV_PIN                   12      /*!< Under-voltage detection GPIO pin */
#define CONF_DPCONTROL_UV_ISR_PRIO              5       /*!< Under-voltage interrupt priority */

#define CONF_DPCONTROL_OV_PORT                  4       /*!< Over-voltage detection GPIO port */
#define CONF_DPCONTROL_OV_PIN                   10      /*!< Over-voltage detection GPIO pin */
#define CONF_DPCONTROL_OV_ISR_PRIO              5       /*!< Over-voltage interrupt priority */

#define CONF_DPCONTROL_OC_PORT                  4       /*!< Over-current detection GPIO port */
#define CONF_DPCONTROL_OC_PIN                   15      /*!< Over-current detection GPIO pin */
#define CONF_DPCONTROL_OC_ISR_PRIO              5       /*!< Over-current interrupt priority */

#define CONF_DPCONTROL_SHUNT_VALUE              0.045   /*!< Current sensing shunt resistor value */
#define CONF_DPCONTROL_INA_GAIN                 9.37    /*!< Current sense amplifier gain */

#define CONF_DPCONTROL_CAL_V_REF                8.179   /*!< Voltage reference calibration value */
#define CONF_DPCONTROL_CAL_V_OFF                0.0     /*!< Voltage offset calibration value */
#define CONF_DPCONTROL_CAL_V_COR                1.327   /*!< Voltage correction calibration value */

#define CONF_DPCONTROL_CAL_C_OFF                1.63265 /*!< Current offset calibration value */
#define CONF_DPCONTROL_CAL_C_COR                1.0     /*!< Current correction calibration value */

#define CONF_DPCONTROL_OV_VALUE                 4.3     /*!< Over-voltage protection threshold */
#define CONF_DPCONTROL_UV_VALUE                 3.0     /*!< Under-voltage protection threshold */
#define CONF_DPCONTROL_OC_VALUE                 1000    /*!< Over-current protection threshold in mA */

/**
 * @}
 */



/**
 * @defgroup GLOBALCONFIG_FSYSTEM_CONFIG File system service configuration
 * @{
 */

#define CONF_FSYSTEM_ENABLE                     1

#define CONF_FSYSTEM_TASK_NAME                  "File System"
#define CONF_FSYSTEM_PRIO                       6
#define CONF_FSYSTEM_STACK_SIZE                 2048

#define CONF_FSYSTEM_OFFSET                     4096
#define CONF_FSYSTEM_BLOCK_SIZE                 256
#define CONF_FSYSTEM_BLOCK_COUNT                1008

#define CONF_FSYSTEM_BD_CHUNK_SIZE              2048
#define CONF_FSYSTEM_BD_CHUNK_MIN_SIZE          CONF_FSYSTEM_BLOCK_SIZE

#define CONF_FSYSTEM_BD_SIZE                    (256 * 1024)

/**
 * @}
 */


/**
 * @defgroup GLOBALCONFIG_CONFIGURATION_CONFIG Configuration service configuration
 * @{
 */

#define CONF_CONFIGURATION_ENABLE                   1       /*!< Enable/disable configuration service */

#define CONF_CONFIGURATION_TASK_NAME                "Configuration service" /*!< Configuration task name */
#define CONF_CONFIGURATION_TASK_PRIO                3       /*!< Configuration task priority */
#define CONF_CONFIGURATION_TASK_STACK_SIZE          1024    /*!< Configuration task stack size */

#define CONF_CONFIGURATION_FILE_PATH                "config/device.cfg" /*!< Configuration file path */
#define CONF_CONFIGURATION_FILE_MAX_SIZE            2048    /*!< Maximum configuration file size */

#define CONF_CONFIGURATION_MAX_PARAMS               30      /*!< Maximum number of configuration parameters */
#define CONF_CONFIGURATION_MAX_PARAM_VALUESIZE      32      /*!< Maximum configuration parameter value length */

#define CONF_CONFIGURATION_HEADER_SIZE              8       /*!< Configuration file header size */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#endif /* CORE_CONFIGURATION_GLOBALCONFIG_H_ */
