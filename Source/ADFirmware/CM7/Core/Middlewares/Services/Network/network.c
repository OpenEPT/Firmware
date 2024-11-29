/*
 * network.c
 *
 *  Created on: Nov 5, 2023
 *      Author: Haris
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

typedef  struct
{

	struct netif gnetif;
	ip4_addr_t ipaddr;
	ip4_addr_t netmask;
	ip4_addr_t gw;
	network_state_t state;
	SemaphoreHandle_t initSig;
	system_link_status_t linkStatus;
}network_data_t;

static network_data_t 			prvNETWORK_DATA;


static 	TaskHandle_t 			prvNETWORK_TASK_HANDLE;
static	ETH_MACConfigTypeDef 	prvNETWORK_MAC_CONFIG;
extern ETH_HandleTypeDef 		HETH;
extern lan8742_Object_t 		LAN8742;

lwiperf_report_fn a(void *arg, enum lwiperf_report_type report_type,
		  const ip_addr_t* local_addr, u16_t local_port, const ip_addr_t* remote_addr, u16_t remote_port,
		  u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec)
{

};

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
		LOGGING_Write("Network", LOGGING_MSG_TYPE_WARNNING, "Network interface down\r\n");
		CONTROL_LinkClosed();
	}
}

static void prvNETWORK_Task()
{
	memset(&prvNETWORK_MAC_CONFIG, 0, sizeof(ETH_MACConfigTypeDef));
	int32_t PHYLinkState = 0;
	uint32_t linkchanged = 0U, speed = 0U, duplex = 0U;
	prvNETWORK_DATA.linkStatus = SYSTEM_LINK_STATUS_DOWN;
	LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network service started\r\n");
	for(;;)
	{
		switch(prvNETWORK_DATA.state)
		{
		case NETWORK_STATE_INIT:
			/* Initilialize the LwIP stack with RTOS */
			tcpip_init( NULL, NULL );

			/* IP addresses initialization without DHCP (IPv4) */
			inet_pton(AF_INET, NETWORK_DEVICE_IP_ADDRESS, &prvNETWORK_DATA.ipaddr);
			inet_pton(AF_INET, NETWORK_DEVICE_IP_MASK, &prvNETWORK_DATA.netmask);
			inet_pton(AF_INET, NETWORK_DEVICE_IP_GW, &prvNETWORK_DATA.gw);

			/* add the network interface (IPv4/IPv6) with RTOS */
			netif_add(&prvNETWORK_DATA.gnetif, &prvNETWORK_DATA.ipaddr, &prvNETWORK_DATA.netmask, &prvNETWORK_DATA.gw, NULL, &ethernetif_init, &tcpip_input);

			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, "Network interface added - Info:\r\n");
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - IP  	: %s\r\n", NETWORK_DEVICE_IP_ADDRESS);
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - MASK	: %s\r\n", NETWORK_DEVICE_IP_MASK);
			LOGGING_Write("Network", LOGGING_MSG_TYPE_INFO, " - Gateway	: %s\r\n", NETWORK_DEVICE_IP_GW);

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
				LOGGING_Write("Network", LOGGING_MSG_TYPE_WARNNING, "Network interface down\r\n");
			}

			/* Set the link callback function, this function is called on change of link status*/
			netif_set_link_callback(&prvNETWORK_DATA.gnetif, prvNETWORK_LinkStatusUpdated);

			xSemaphoreGive(prvNETWORK_DATA.initSig);
			prvNETWORK_DATA.state = NETWORK_STATE_SERVICE;
			break;
		case NETWORK_STATE_SERVICE:
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
