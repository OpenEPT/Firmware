/**
 ******************************************************************************
 * @file    network.c
 *
 * @brief   Network service responsible for initializing the LwIP stack,
 *          managing Ethernet link status, and configuring the physical interface
 *          via LAN8742. The service runs as a FreeRTOS task and provides
 *          network availability tracking for other system modules.
 *
 * @details The service configures the MAC and IP parameters, sets up callbacks
 *          for link status monitoring, and reacts to changes in Ethernet PHY
 *          state. The network interface is registered to the LwIP stack and
 *          becomes available through `netif`.
 *
 * @author  Haris Turkmanovic
 * @email   haris.turkmanovic@gmail.com
 * @date    November 2023
 ******************************************************************************
 */

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "ethernetif.h"
#include "lwip/apps/lwiperf.h"
#include "lan8742.h"

#include "system.h"
#include "network.h"
#include "logging.h"
#include "control.h"
#include "configuration.h"
/**
 * @defgroup SERVICES Services
 * @{
 */

/**
 * @brief Task notification bitmask for runtime network reconfiguration
 *
 * @details Used with xTaskNotify to signal network task to update
 *          IP configuration parameters. Multiple flags can be combined.
 */
#define NETWORK_MASK_SET_IP        0x00000001
#define NETWORK_MASK_SET_MASK      0x00000002
#define NETWORK_MASK_SET_GW        0x00000004
#define NETWORK_MASK_SET_ALL       0x00000008

/**
 * @defgroup NETWORK_SERVICE Network service
 * @{
 */

/**
 * @defgroup NETWORK_INTERNAL_TYPES Network private structures
 * @{
 */

/**
 * @brief Network service internal data structure
 *
 * @details
 *  - *_str fields hold configuration values (string form from CONFIGURATION)
 *  - ipaddr/netmask/gw are binary representations used by LwIP
 *  - Updates are performed by modifying *_str, then converting to binary
 *    using inet_pton before applying to netif
 */
typedef struct
{
	struct netif gnetif;                  /**< LwIP network interface */
	ip4_addr_t ipaddr;                    /**< Static IP address */
	ip4_addr_t netmask;                   /**< Subnet mask */
	ip4_addr_t gw;                        /**< Gateway IP */
	network_state_t state;                /**< Current network service state */
	SemaphoreHandle_t initSig;           /**< Semaphore for signaling init complete */
	system_link_status_t linkStatus;     /**< Last known physical link status */
	char ip_addr_str[CONFIGURATION_MAX_PARAM_VALUESIZE];
	char ip_mask_str[CONFIGURATION_MAX_PARAM_VALUESIZE];
	char ip_gw_str[CONFIGURATION_MAX_PARAM_VALUESIZE];
} network_data_t;
/**
 * @}
 */

/**
 * @defgroup NETWORK_PRIVATE_DATA Network private data
 * @{
 */

/**
 * @brief Static instance of network service data
 */
static network_data_t 			prvNETWORK_DATA;

/**
 * @brief Handle to the network task
 */
static 	TaskHandle_t 			prvNETWORK_TASK_HANDLE;
/**
 * @brief Temporary storage for MAC configuration
 */
static	ETH_MACConfigTypeDef 	prvNETWORK_MAC_CONFIG;
/**
 * @brief Global MAC address buffer
 *
 * @note Filled during INIT from CONFIGURATION service.
 *       Used by Ethernet driver during low-level initialization.
 */
uint8_t                         NETWORK_MAC_ADDR[6];
/**
 * @brief Handle
 */
extern ETH_HandleTypeDef 		HETH;
/**
 * @brief Temporary storage for lan8742 object
 */
extern lan8742_Object_t 		LAN8742;



lwiperf_report_fn a(void *arg, enum lwiperf_report_type report_type,
		  const ip_addr_t* local_addr, u16_t local_port, const ip_addr_t* remote_addr, u16_t remote_port,
		  u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec)
{

};
/**
 * @}
 */
/**
 * @brief Parse MAC address string into binary format
 *
 * @param str Input string in format "aa:bb:cc:dd:ee:ff"
 * @param mac Output buffer (6 bytes)
 *
 * @return 0 on success, -1 on failure
 *
 * @note This function does NOT modify hardware directly.
 *       It is used only during initialization.
 */
static int prvParseMacAddress(const char* str, uint8_t* mac)
{
    if(str == NULL || mac == NULL)
        return -1;

    int values[6];

    /* sscanf je najjednostavniji i robustan ovde */
    if(sscanf(str, "%x:%x:%x:%x:%x:%x",
              &values[0], &values[1], &values[2],
              &values[3], &values[4], &values[5]) != 6)
    {
        return -1;
    }

    for(int i = 0; i < 6; i++)
    {
        if(values[i] < 0 || values[i] > 255)
            return -1;

        mac[i] = (uint8_t)values[i];
    }

    return 0;
}

/**
 * @defgroup NETWORK_PRIVATE_FUNCTIONS Network private functions
 * @{
 */

/**
 * @brief Print the current PHY link type and speed to the log.
 *
 * This function reads the link state from LAN8742 and prints the
 * duplex and speed mode (e.g. 100Mbps Full Duplex) via LOGGING_Write.
 */
static void prvNETWORK_LinkStatusPrintInfo()
{
	int32_t PHYLinkState = 0;
	PHYLinkState = LAN8742_GetLinkState(&LAN8742);
	switch (PHYLinkState)
	{
		case LAN8742_STATUS_100MBITS_FULLDUPLEX:
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Link Type: 100Mbps (Full Duplex)\r\n");
		  break;
		case LAN8742_STATUS_100MBITS_HALFDUPLEX:
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Link Type: 100Mbps (Half Duplex)\r\n");
		  break;
		case LAN8742_STATUS_10MBITS_FULLDUPLEX:
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Link Type: 10Mbps (Full Duplex)\r\n");
		  break;
		case LAN8742_STATUS_10MBITS_HALFDUPLEX:
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Link Type: 10Mbps (Half Duplex)\r\n");
		  break;
		default:
		  break;
	}
}
/**
 * @brief Callback called when the network link status changes.
 *
 * Called by LwIP when the netif goes up or down. It updates the
 * system link status and triggers CONTROL_LinkClosed() on disconnection.
 *
 * @param[in] netif Pointer to the affected network interface.
 */
static void prvNETWORK_LinkStatusUpdated(struct netif *netif)
{
	if (netif_is_up(netif))
	{
		SYSTEM_SetLinkStatus(SYSTEM_LINK_STATUS_UP);
		prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_UP;
		LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network interface up\r\n");
		prvNETWORK_LinkStatusPrintInfo();
	}
	else
	{
		SYSTEM_SetLinkStatus(SYSTEM_LINK_STATUS_DOWN);
		prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_DOWN;
		LOGGING_Write("Network", LOGGING_MSG_TYPE_WARNING, "Network interface down\r\n");
		CONTROL_LinkClosed();
	}
}
/**
 * @brief Network service task.
 *
 * This task performs the following:
 * - Initializes the LwIP stack
 * - Configures the IP, subnet, and gateway
 * - Sets up the MAC and registers the interface
 * - Monitors the Ethernet PHY link and reconfigures the MAC dynamically
 *
 * If an error occurs, the task reports it and blocks indefinitely.
 */
static void prvNETWORK_Task()
{
	memset(&prvNETWORK_MAC_CONFIG, 0, sizeof(ETH_MACConfigTypeDef));
	int32_t PHYLinkState = 0;
	uint32_t linkchanged = 0U, speed = 0U, duplex = 0U;
	prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_DOWN;
	LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network service started\r\n");
	char mac_addr[CONFIGURATION_MAX_PARAM_VALUESIZE];
	memset(mac_addr, 0, CONFIGURATION_MAX_PARAM_VALUESIZE);

	uint8_t def;
	for(;;)
	{
		switch(prvNETWORK_DATA.state)
		{
		case NETWORK_STATE_INIT:
			/* Initilialize the LwIP stack with RTOS */
			tcpip_init( NULL, NULL );

			CONFIGURATION_GetParameter_String("MAC_ADDRESS", mac_addr, sizeof(mac_addr), &def);

			if(prvParseMacAddress(mac_addr, NETWORK_MAC_ADDR) != 0)
			{
			    LOGGING_Write("Network", LOGGING_MSG_TYPE_ERROR, "Invalid MAC format\r\n");
			}
			else
			{
			    LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "MAC loaded: %s\r\n", mac_addr);
			}

			CONFIGURATION_GetParameter_String("IP_ADDRESS", prvNETWORK_DATA.ip_addr_str, sizeof(prvNETWORK_DATA.ip_addr_str), &def);
			CONFIGURATION_GetParameter_String("IP_MASK", prvNETWORK_DATA.ip_mask_str, sizeof(prvNETWORK_DATA.ip_mask_str), &def);
			CONFIGURATION_GetParameter_String("IP_GATEWAY", prvNETWORK_DATA.ip_gw_str, sizeof(prvNETWORK_DATA.ip_gw_str), &def);

			if(inet_pton(AF_INET, prvNETWORK_DATA.ip_addr_str, &prvNETWORK_DATA.ipaddr) != 1)
			{
			    LOGGING_Write("Network", LOGGING_MSG_TYPE_ERROR, "Invalid IP format\r\n");
			}
			if(inet_pton(AF_INET, prvNETWORK_DATA.ip_mask_str, &prvNETWORK_DATA.netmask) != 1)
			{
			    LOGGING_Write("Network", LOGGING_MSG_TYPE_ERROR, "Invalid NETMASK format\r\n");
			}
			if(inet_pton(AF_INET, prvNETWORK_DATA.ip_gw_str, &prvNETWORK_DATA.gw) != 1)
			{
				LOGGING_Write("Network", LOGGING_MSG_TYPE_ERROR, "Invalid GW format\r\n");
			}

			/* add the network interface (IPv4/IPv6) with RTOS */
			netif_add(&prvNETWORK_DATA.gnetif, &prvNETWORK_DATA.ipaddr, &prvNETWORK_DATA.netmask, &prvNETWORK_DATA.gw, NULL, &ethernetif_init, &tcpip_input);

			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network interface added - Info:\r\n");
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - IP   : %s\r\n", prvNETWORK_DATA.ip_addr_str);
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - MASK : %s\r\n", prvNETWORK_DATA.ip_mask_str);
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - Gateway : %s\r\n", prvNETWORK_DATA.ip_gw_str);

			/* Registers the default network interface */
			netif_set_default(&prvNETWORK_DATA.gnetif);

			if (netif_is_link_up(&prvNETWORK_DATA.gnetif))
			{
				/* When the netif is fully configured this function must be called */
				netif_set_up(&prvNETWORK_DATA.gnetif);
				SYSTEM_SetLinkStatus(SYSTEM_LINK_STATUS_UP);
				prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_UP;
				LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network interface up\r\n");
			}
			else
			{
				/* When the netif link is down this function must be called */
				netif_set_down(&prvNETWORK_DATA.gnetif);
				SYSTEM_SetLinkStatus(SYSTEM_LINK_STATUS_DOWN);
				prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_DOWN;
				LOGGING_Write("Network", LOGGING_MSG_TYPE_WARNING, "Network interface down\r\n");
			}

			/* Set the link callback function, this function is called on change of link status*/
			netif_set_link_callback(&prvNETWORK_DATA.gnetif, prvNETWORK_LinkStatusUpdated);

			xSemaphoreGive(prvNETWORK_DATA.initSig);
			prvNETWORK_DATA.state = NETWORK_STATE_SERVICE;
			break;
		case NETWORK_STATE_SERVICE:

			uint32_t value;
			if(xTaskNotifyWait(0x0, 0xFFFFFFFF, &value, 0) == pdTRUE)
			{
			    if(value & NETWORK_MASK_SET_IP)
			    {
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_addr_str, &prvNETWORK_DATA.ipaddr);
			        netif_set_addr(&prvNETWORK_DATA.gnetif, &prvNETWORK_DATA.ipaddr, &prvNETWORK_DATA.netmask, &prvNETWORK_DATA.gw);

			        CONFIGURATION_SetParameter_String("IP_ADDRESS", prvNETWORK_DATA.ip_addr_str, 1000);

			        LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "IP updated: %s\r\n", prvNETWORK_DATA.ip_addr_str);
			    }

			    if(value & NETWORK_MASK_SET_MASK)
			    {
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_mask_str, &prvNETWORK_DATA.netmask);
			        netif_set_addr(&prvNETWORK_DATA.gnetif, &prvNETWORK_DATA.ipaddr, &prvNETWORK_DATA.netmask, &prvNETWORK_DATA.gw);

			        CONFIGURATION_SetParameter_String("IP_MASK", prvNETWORK_DATA.ip_mask_str, 1000);

			        LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Mask updated: %s\r\n", prvNETWORK_DATA.ip_mask_str);
			    }

			    if(value & NETWORK_MASK_SET_GW)
			    {
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_gw_str, &prvNETWORK_DATA.gw);
			        netif_set_addr(&prvNETWORK_DATA.gnetif, &prvNETWORK_DATA.ipaddr, &prvNETWORK_DATA.netmask, &prvNETWORK_DATA.gw);

			        CONFIGURATION_SetParameter_String("IP_GATEWAY", prvNETWORK_DATA.ip_gw_str, 1000);

			        LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Gateway updated: %s\r\n", prvNETWORK_DATA.ip_gw_str);
			    }
			    if(value & NETWORK_MASK_SET_ALL)
			    {
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_addr_str, &prvNETWORK_DATA.ipaddr);
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_mask_str, &prvNETWORK_DATA.netmask);
			        inet_pton(AF_INET, prvNETWORK_DATA.ip_gw_str, &prvNETWORK_DATA.gw);

			        netif_set_addr(&prvNETWORK_DATA.gnetif,
			                       &prvNETWORK_DATA.ipaddr,
			                       &prvNETWORK_DATA.netmask,
			                       &prvNETWORK_DATA.gw);

			        CONFIGURATION_SetParameter_String("IP_ADDRESS", prvNETWORK_DATA.ip_addr_str, 1000);
			        CONFIGURATION_SetParameter_String("IP_MASK", prvNETWORK_DATA.ip_mask_str, 1000);
			        CONFIGURATION_SetParameter_String("IP_GATEWAY", prvNETWORK_DATA.ip_gw_str, 1000);

			        LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO,
			                      "IP config updated: IP=%s MASK=%s GW=%s\r\n",
			                      prvNETWORK_DATA.ip_addr_str,
			                      prvNETWORK_DATA.ip_mask_str,
			                      prvNETWORK_DATA.ip_gw_str);
			    }

			}


			LOCK_TCPIP_CORE();
			PHYLinkState = LAN8742_GetLinkState(&LAN8742);

			if(netif_is_link_up(&prvNETWORK_DATA.gnetif) && (PHYLinkState <= LAN8742_STATUS_LINK_DOWN))
			{
				HAL_ETH_Stop_IT(&HETH);
				netif_set_down(&prvNETWORK_DATA.gnetif);
				netif_set_link_down(&prvNETWORK_DATA.gnetif);
			}
			else if(!netif_is_link_up(&prvNETWORK_DATA.gnetif) && (PHYLinkState > LAN8742_STATUS_LINK_DOWN))
			{
				switch (PHYLinkState)
				{
					case LAN8742_STATUS_100MBITS_FULLDUPLEX:
					  duplex = ETH_FULLDUPLEX_MODE;
					  speed = ETH_SPEED_100M;
					  linkchanged = 1;
					  break;
					case LAN8742_STATUS_100MBITS_HALFDUPLEX:
					  duplex = ETH_HALFDUPLEX_MODE;
					  speed = ETH_SPEED_100M;
					  linkchanged = 1;
					  break;
					case LAN8742_STATUS_10MBITS_FULLDUPLEX:
					  duplex = ETH_FULLDUPLEX_MODE;
					  speed = ETH_SPEED_10M;
					  linkchanged = 1;
					  break;
					case LAN8742_STATUS_10MBITS_HALFDUPLEX:
					  duplex = ETH_HALFDUPLEX_MODE;
					  speed = ETH_SPEED_10M;
					  linkchanged = 1;
					  break;
					default:
					  break;
				}

				if(linkchanged)
				{
					/* Get MAC Config MAC */
					HAL_ETH_GetMACConfig(&HETH, &prvNETWORK_MAC_CONFIG);
					prvNETWORK_MAC_CONFIG.DuplexMode = duplex;
					prvNETWORK_MAC_CONFIG.Speed = speed;
					HAL_ETH_SetMACConfig(&HETH, &prvNETWORK_MAC_CONFIG);
					HAL_ETH_Start_IT(&HETH);
					netif_set_up(&prvNETWORK_DATA.gnetif);
					netif_set_link_up(&prvNETWORK_DATA.gnetif);
				}

			  linkchanged = 0;
			}
			UNLOCK_TCPIP_CORE();
			vTaskDelay(pdMS_TO_TICKS(100));
			break;
		case NETWORK_STATE_ERROR:
			SYSTEM_ReportError(SYSTEM_ERROR_LEVEL_LOW);
			vTaskDelay(portMAX_DELAY);
			break;
		}

	}
}




network_status_t NETWORK_Init(uint32_t timeout)
{
	if(xTaskCreate(prvNETWORK_Task,
			NETWORK_TASK_NAME,
			NETWORK_TASK_STACK_SIZE,
			NULL,
			NETWORK_TASK_PRIO,
			&prvNETWORK_TASK_HANDLE) != pdTRUE) return NETWORK_STATUS_ERROR;

	prvNETWORK_DATA.initSig = xSemaphoreCreateBinary();

	if(prvNETWORK_DATA.initSig == NULL) return NETWORK_STATUS_ERROR;

	prvNETWORK_DATA.state = NETWORK_STATE_INIT;

	if(xSemaphoreTake(prvNETWORK_DATA.initSig, pdMS_TO_TICKS(timeout)) != pdFALSE) return NETWORK_STATUS_ERROR;

	return NETWORK_STATUS_OK;
}

network_status_t NETWORK_SetIPAddress(const char* ip)
{
    strncpy(prvNETWORK_DATA.ip_addr_str, ip, sizeof(prvNETWORK_DATA.ip_addr_str)-1);
    prvNETWORK_DATA.ip_addr_str[sizeof(prvNETWORK_DATA.ip_addr_str)-1] = '\0';

    if(xTaskNotify(prvNETWORK_TASK_HANDLE, NETWORK_MASK_SET_IP, eSetBits) != pdTRUE)
        return NETWORK_STATUS_ERROR;

    return NETWORK_STATUS_OK;
}

network_status_t NETWORK_SetIPMask(const char* mask)
{
    strncpy(prvNETWORK_DATA.ip_mask_str, mask, sizeof(prvNETWORK_DATA.ip_mask_str)-1);
    prvNETWORK_DATA.ip_mask_str[sizeof(prvNETWORK_DATA.ip_mask_str)-1] = '\0';

    if(xTaskNotify(prvNETWORK_TASK_HANDLE, NETWORK_MASK_SET_MASK, eSetBits) != pdTRUE)
        return NETWORK_STATUS_ERROR;

    return NETWORK_STATUS_OK;
}

network_status_t NETWORK_SetGateway(const char* gw)
{
    strncpy(prvNETWORK_DATA.ip_gw_str, gw, sizeof(prvNETWORK_DATA.ip_gw_str)-1);
    prvNETWORK_DATA.ip_gw_str[sizeof(prvNETWORK_DATA.ip_gw_str)-1] = '\0';

    if(xTaskNotify(prvNETWORK_TASK_HANDLE, NETWORK_MASK_SET_GW, eSetBits) != pdTRUE)
        return NETWORK_STATUS_ERROR;

    return NETWORK_STATUS_OK;
}

network_status_t NETWORK_SetIPAll(const char* ip, const char* mask, const char* gw)
{
    if(ip == NULL || mask == NULL || gw == NULL)
        return NETWORK_STATUS_ERROR;

    strncpy(prvNETWORK_DATA.ip_addr_str, ip, sizeof(prvNETWORK_DATA.ip_addr_str) - 1);
    strncpy(prvNETWORK_DATA.ip_mask_str, mask, sizeof(prvNETWORK_DATA.ip_mask_str) - 1);
    strncpy(prvNETWORK_DATA.ip_gw_str, gw, sizeof(prvNETWORK_DATA.ip_gw_str) - 1);

    prvNETWORK_DATA.ip_addr_str[sizeof(prvNETWORK_DATA.ip_addr_str) - 1] = '\0';
    prvNETWORK_DATA.ip_mask_str[sizeof(prvNETWORK_DATA.ip_mask_str) - 1] = '\0';
    prvNETWORK_DATA.ip_gw_str[sizeof(prvNETWORK_DATA.ip_gw_str) - 1] = '\0';

    if(xTaskNotify(prvNETWORK_TASK_HANDLE, NETWORK_MASK_SET_ALL, eSetBits) != pdTRUE)
        return NETWORK_STATUS_ERROR;

    return NETWORK_STATUS_OK;
}

network_status_t NETWORK_GetMACAddr(uint8_t* mac)
{
    if(mac == NULL)
        return NETWORK_STATUS_ERROR;

    memcpy(mac, NETWORK_MAC_ADDR, 6);

    return NETWORK_STATUS_OK;
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
