/**
 ******************************************************************************
 * @file   	drv_gpio.c
 * @brief  	...
 * @author	Haris Turkmanovic
 * @email	haris.turkmanovic@gmail.com
 * @date	November 2022
 ******************************************************************************
 */
#include "drv_gpio.h"
#include "main.h"
#include <string.h>


static drv_gpio_port_handle_t 		prvDRV_GPIO_PORTS[DRV_GPIO_PORT_MAX_NUMBER];
static drv_gpio_pin_isr_callback	prvDRV_GPIO_PINS_INTERRUPTS[DRV_GPIO_INTERRUPTS_MAX_NUMBER];
static drv_gpio_init_status_t		prvDRV_GPIO_INIT_STATUS;

void 	HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	uint32_t 	i = 0;
	uint16_t	pinNo = GPIO_Pin;
	while(pinNo != 0x01)
	{
		pinNo = pinNo >> 1;
		i++;
	}
	if(i < DRV_GPIO_INTERRUPTS_MAX_NUMBER)
	{
		prvDRV_GPIO_PINS_INTERRUPTS[i](GPIO_Pin);
	}
}

static uint32_t	prvDRV_GPIO_Pin_GetModeCode(drv_gpio_pin_mode_t mode)
{
	switch(mode)
	{
	case	DRV_GPIO_PIN_MODE_INPUT:
		return GPIO_MODE_INPUT;
	case	DRV_GPIO_PIN_MODE_OUTPUT_PP:
		return GPIO_MODE_OUTPUT_PP;
	case	DRV_GPIO_PIN_MODE_OUTPUT_OD:
		return GPIO_MODE_OUTPUT_OD;
	case	DRV_GPIO_PIN_MODE_AF_PP:
		return GPIO_MODE_AF_PP;
	case	DRV_GPIO_PIN_MODE_AF_OD:
		return GPIO_MODE_AF_OD;
	case	DRV_GPIO_PIN_MODE_ANALOG:
		return GPIO_MODE_ANALOG;
	case	DRV_GPIO_PIN_MODE_IT_RISING:
		return GPIO_MODE_IT_RISING;
	case	DRV_GPIO_PIN_MODE_IT_FALLING:
		return GPIO_MODE_IT_FALLING;
	case	DRV_GPIO_PIN_MODE_IT_RISING_FALLING:
		return GPIO_MODE_IT_RISING_FALLING;
	case	DRV_GPIO_PIN_MODE_EVT_RISING:
		return GPIO_MODE_EVT_RISING;
	case	DRV_GPIO_PIN_MODE_EVT_FALLING:
		return GPIO_MODE_EVT_FALLING;
	case	DRV_GPIO_PIN_MODE_EVT_RISING_FALLING:
		return GPIO_MODE_EVT_RISING_FALLING;
	}
	return 0;
}

drv_gpio_status_t DRV_GPIO_Init()
{
	memset(prvDRV_GPIO_PORTS, 0, DRV_GPIO_PORT_MAX_NUMBER*sizeof(drv_gpio_port_handle_t));
	memset(prvDRV_GPIO_PINS_INTERRUPTS, 0, DRV_GPIO_INTERRUPTS_MAX_NUMBER*sizeof(drv_gpio_pin_isr_callback));
	prvDRV_GPIO_INIT_STATUS = DRV_GPIO_INIT_STATUS_INIT;
	return DRV_GPIO_STATUS_OK;
}

drv_gpio_status_t DRV_GPIO_Port_Init(drv_gpio_port_t port)
{
	if(prvDRV_GPIO_PORTS[port].lock != NULL) return DRV_GPIO_STATUS_ERROR;

	prvDRV_GPIO_PORTS[port].lock = xSemaphoreCreateMutex();

	if(prvDRV_GPIO_PORTS[port].lock == NULL)  return DRV_GPIO_STATUS_ERROR;

	prvDRV_GPIO_PORTS[port].initState = DRV_GPIO_PORT_INIT_STATUS_INITIALIZED;

	switch(port)
	{
	case DRV_GPIO_PORT_A:
		  __HAL_RCC_GPIOA_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOA;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_A;
		break;
	case DRV_GPIO_PORT_B:
		  __HAL_RCC_GPIOB_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOB;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_B;
		break;
	case DRV_GPIO_PORT_C:
		  __HAL_RCC_GPIOC_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOC;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_C;
		break;
	case DRV_GPIO_PORT_D:
		  __HAL_RCC_GPIOD_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOD;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_D;
		break;
	case DRV_GPIO_PORT_E:
		  __HAL_RCC_GPIOE_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOE;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_E;
		break;
	case DRV_GPIO_PORT_F:
		  __HAL_RCC_GPIOF_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOF;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_F;
		break;
	case DRV_GPIO_PORT_G:
		  __HAL_RCC_GPIOG_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOG;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_G;
		break;
	case DRV_GPIO_PORT_H:
		  __HAL_RCC_GPIOH_CLK_ENABLE();
		  prvDRV_GPIO_PORTS[port].portInstance = GPIOH;
		  prvDRV_GPIO_PORTS[port].port = DRV_GPIO_PORT_H;
		break;
	default:
		return DRV_GPIO_STATUS_ERROR;
		break;
	}

	return DRV_GPIO_STATUS_OK;
}


drv_gpio_status_t DRV_GPIO_Pin_Init(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_init_conf_t* conf)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_GPIO_PORTS[port].lock, portMAX_DELAY) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	GPIO_InitStruct.Pin  = 0x0001 << pin;
	GPIO_InitStruct.Mode = prvDRV_GPIO_Pin_GetModeCode(conf->mode);
	GPIO_InitStruct.Pull = conf->pullState;
	HAL_GPIO_Init((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, &GPIO_InitStruct);

	if(xSemaphoreGive(prvDRV_GPIO_PORTS[port].lock) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return DRV_GPIO_STATUS_OK;
}

drv_gpio_status_t 		DRV_GPIO_Pin_DeInit(drv_gpio_port_t port, drv_gpio_pin pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_GPIO_PORTS[port].lock, portMAX_DELAY) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	HAL_GPIO_DeInit((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, 0x0001 << pin);

	if(xSemaphoreGive(prvDRV_GPIO_PORTS[port].lock) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return DRV_GPIO_STATUS_OK;

}
drv_gpio_status_t DRV_GPIO_Pin_SetState(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state)
{
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_GPIO_PORTS[port].lock, portMAX_DELAY) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	HAL_GPIO_WritePin((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, 1 << pin, state);

	if(xSemaphoreGive(prvDRV_GPIO_PORTS[port].lock) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return DRV_GPIO_STATUS_OK;
}
drv_gpio_status_t 		DRV_GPIO_Pin_SetStateFromISR(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_state_t state)
{
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED ) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;


	HAL_GPIO_WritePin((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, 1 << pin, state);


	return DRV_GPIO_STATUS_OK;
}

drv_gpio_pin_state_t 	DRV_GPIO_Pin_ReadState(drv_gpio_port_t port, drv_gpio_pin pin)
{
	GPIO_PinState state;
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_GPIO_PORTS[port].lock, portMAX_DELAY) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	state = HAL_GPIO_ReadPin((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, 1 << pin);

	if(xSemaphoreGive(prvDRV_GPIO_PORTS[port].lock) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return state;
}

drv_gpio_status_t DRV_GPIO_Pin_ToogleFromISR(drv_gpio_port_t port, drv_gpio_pin pin)
{
	BaseType_t * pxHigherPriorityTaskWoken = pdFALSE;
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTakeFromISR(prvDRV_GPIO_PORTS[port].lock, &pxHigherPriorityTaskWoken) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	HAL_GPIO_TogglePin((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, 1 << pin);

	if(xSemaphoreGiveFromISR(prvDRV_GPIO_PORTS[port].lock, &pxHigherPriorityTaskWoken) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return DRV_GPIO_STATUS_OK;
}


drv_gpio_status_t DRV_GPIO_Pin_EnableInt(drv_gpio_port_t port, drv_gpio_pin pin, uint32_t pri, drv_gpio_pin_isr_callback callback)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	drv_gpio_status_t status = DRV_GPIO_STATUS_OK;
	if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL) return DRV_GPIO_STATUS_ERROR;
	if(pin > DRV_GPIO_PIN_MAX_NUMBER || pin > DRV_GPIO_INTERRUPTS_MAX_NUMBER) return DRV_GPIO_STATUS_ERROR;

	if(xSemaphoreTake(prvDRV_GPIO_PORTS[port].lock, portMAX_DELAY) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	/*Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin  = 1 << pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init((GPIO_TypeDef*)prvDRV_GPIO_PORTS[port].portInstance, &GPIO_InitStruct);

	prvDRV_GPIO_PINS_INTERRUPTS[pin] = callback;

	if(pin >= 10  && pin <= 15){
		HAL_NVIC_SetPriority(EXTI15_10_IRQn, pri, 0);
		HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
	}
	if(pin >= 5  && pin <= 9){
		HAL_NVIC_SetPriority(EXTI9_5_IRQn, pri, 0);
		HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	}
	if(pin < 5)
	{
		switch(pin)
		{
		case 0:
			HAL_NVIC_SetPriority(EXTI0_IRQn, pri, 0);
			HAL_NVIC_EnableIRQ(EXTI0_IRQn);
			break;
		case 1:
			HAL_NVIC_SetPriority(EXTI1_IRQn, pri, 0);
			HAL_NVIC_EnableIRQ(EXTI1_IRQn);
			break;
		case 2:
			HAL_NVIC_SetPriority(EXTI2_IRQn, pri, 0);
			HAL_NVIC_EnableIRQ(EXTI2_IRQn);
			break;
		case 3:
			HAL_NVIC_SetPriority(EXTI3_IRQn, pri, 0);
			HAL_NVIC_EnableIRQ(EXTI3_IRQn);
			break;
		case 4:
			HAL_NVIC_SetPriority(EXTI4_IRQn, pri, 0);
			HAL_NVIC_EnableIRQ(EXTI4_IRQn);
			break;
		default:
			/*Never ends here*/
			break;
		}
	}

	if(xSemaphoreGive(prvDRV_GPIO_PORTS[port].lock) == pdFALSE ) return DRV_GPIO_STATUS_ERROR;

	return DRV_GPIO_STATUS_OK;
}

drv_gpio_status_t DRV_GPIO_RegisterCallback(drv_gpio_port_t port, drv_gpio_pin pin, drv_gpio_pin_isr_callback callback, uint32_t priority)
{
    // Ensure the port is initialized
    if(prvDRV_GPIO_PORTS[port].initState != DRV_GPIO_PORT_INIT_STATUS_INITIALIZED || prvDRV_GPIO_PORTS[port].lock == NULL)
        return DRV_GPIO_STATUS_ERROR;

    // Ensure the pin number is valid
    if(pin > DRV_GPIO_PIN_MAX_NUMBER || pin > DRV_GPIO_INTERRUPTS_MAX_NUMBER)
        return DRV_GPIO_STATUS_ERROR;

    // Store the callback
    prvDRV_GPIO_PINS_INTERRUPTS[pin] = callback;

    // Enable the interrupt for the specified pin
    drv_gpio_status_t status = DRV_GPIO_Pin_EnableInt(port, pin, priority, callback);

    return status;
}

void DRV_GPIO_ClearInterruptFlag(uint16_t GPIO_Pin)
{
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);

}
