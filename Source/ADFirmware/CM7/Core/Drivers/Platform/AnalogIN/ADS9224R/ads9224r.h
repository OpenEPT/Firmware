/*
 * ads9224r.h
 *
 *  Created on: Oct 7, 2024
 *      Author: Haris
 */

#ifndef CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_
#define CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_
#include "drv_gpio.h"

/*Platform specific options*/
#define				ADS9224R_TIMERS_INPUT_CLK					200		//Mhz


#define 			ADS9224R_READY_STROBE_PORT					DRV_GPIO_PORT_G
#define 			ADS9224R_READY_STROBE_PIN					10
#define 			ADS9224R_RESET_PD_PORT						DRV_GPIO_PORT_G
#define 			ADS9224R_RESET_PD_PIN						8
#define 			ADS9224R_CONVST_PORT						DRV_GPIO_PORT_A
#define 			ADS9224R_CONVST_PIN							3
#define				ADS9224R_CS_PORT							DRV_GPIO_PORT_D
#define				ADS9224R_CS_PIN								12

/*Device options*/
#define				ADS9224R_REG_ADDR_DEVICE_STATUS				0x00
#define				ADS9224R_REG_ADDR_POWER_DOWN_CNFG			0x01
#define				ADS9224R_REG_ADDR_PROTOCOL_CNFG				0x02
#define				ADS9224R_REG_ADDR_BUS_WIDTH					0x03
#define				ADS9224R_REG_ADDR_CRT_CFG					0x04
#define				ADS9224R_REG_ADDR_OUTPUT_DATA_WORD_CFG		0x05
#define				ADS9224R_REG_ADDR_DATA_AVG_CFG				0x06
#define				ADS9224R_REG_ADDR_REFBY2_OFFSET				0x07

#define				ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_DATA_RIGHT_ALIGNED			0x01
#define				ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_FIXED_PATTERN_DATA			0x02
#define				ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_PARALLEL_MODE_DATA_FORMAT	0x10
#define				ADS9224R_REG_MASK_OUTPUT_DATA_WORD_CFG_READY_MASK					0x20



/*Driver options*/
typedef enum
{
	ADS9224R_STATUS_OK = 0,
	ADS9224R_STATUS_ERROR
}ads9224r_status_t;

typedef enum
{
	ADS9224R_SDO_A,
	ADS9224R_SDO_B,
}ads9224r_sdo_t;

typedef enum
{
	ADS9224R_INIT_STATE_NOINIT = 0,
	ADS9224R_INIT_STATE_INIT
}ads9224r_init_state_t;

typedef enum
{
	ADS9224R_ACQ_STATE_INACTIVE = 0,
	ADS9224R_ACQ_STATE_ACTIVE
}ads9224r_acq_state_t;

typedef enum
{
	ADS9224R_OP_STATE_DOWN = 0,
	ADS9224R_OP_STATE_UP,
	ADS9224R_OP_STATE_CONFIG,
	ADS9224R_OP_STATE_ACQ
}ads9224r_op_state_t;

typedef enum
{
	ADS9224R_SDO_WIDTH_ONE_SDO = 0,
	ADS9224R_SDO_WIDTH_DUAL_SDO,
	ADS9224R_SDO_WIDTH_QUAD_SDO
}ads9224r_sdo_width_t;

typedef enum
{
	ADS9224R_POWER_DOWN_STATE_ACTIVE,
	ADS9224R_POWER_DOWN_STATE_INACTIVE
}ads9224r_power_down_state_t;

typedef enum
{
	ADS9224R_DATA_TRANSFER_FRAME_ZONE_1 = 0,
	ADS9224R_DATA_TRANSFER_FRAME_ZONE_2
}ads9224r_data_transfer_frame_t;

typedef enum
{
	ADS9224R_SDO_PROTOCOL_SPI_SDR = 0,
	ADS9224R_SDO_PROTOCOL_SPI_DDR,
	ADS9224R_SDO_PROTOCOL_SPI_SDR_CRT,
	ADS9224R_SDO_PROTOCOL_SPI_DDR_CRT,
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_1,
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_2,
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_3,
	ADS9224R_SDO_PROTOCOL_SPI_PARALLEL_4
}ads9224r_sdo_protocol_t;

typedef enum
{
	ADS9224R_CPOL_LOW =0,
	ADS9224R_CPOL_HIGH
}ads9224r_cpol_t;

typedef enum
{
	ADS9224_CPHA_LOW =0,
	ADS9224_CPHA_HIGH
}ads9224r_cpha_t;

typedef enum
{
	ADS9224R_CRT_CLK_SERIAL = 0,
	ADS9224R_CRT_CLK_INTCLK,
	ADS9224R_CRT_CLK_INTCLK_1_2,
	ADS9224R_CRT_CLK_INTCLK_1_4
}ads9224r_crt_clk_t;

typedef enum
{
	ADS9224R_PARALLEL_MODE_AA = 0,
	ADS9224R_PARALLEL_MODE_AB
}ads9224r_parallel_mode_t;

typedef enum
{
	ADS9224R_DATA_ALIGNED_LEFT = 0,
	ADS9224R_DATA_ALIGNED_RIGHT
}ads9224r_data_aligned_t;

typedef enum
{
	ADS9224R_FPATTERN_STATE_DISABLED = 0,
	ADS9224R_FPATTERN_STATE_ENABLED
}ads9224r_fpattern_state_t;

typedef enum
{
	ADS9224R_AVG_DISABLE = 0,
	ADS9224R_AVG_2,
	ADS9224R_AVG_4
}ads9224r_avg_t;

typedef struct
{
	ads9224r_power_down_state_t pdRefBy2;
	ads9224r_power_down_state_t pdAdcB;
	ads9224r_power_down_state_t pdAdcA;
	ads9224r_power_down_state_t pdRef;
}ads9224r_power_down_config_t;

typedef struct
{
	ads9224r_sdo_protocol_t sdoProtocol;
	ads9224r_cpol_t			cPol;
	ads9224r_cpha_t			cpha;
	ads9224r_sdo_width_t	busWidth;
	ads9224r_crt_clk_t		crt;
}ads9224r_protocol_config_t;



typedef struct
{
	ads9224r_data_transfer_frame_t 	frame;
	ads9224r_power_down_config_t	pd;
	ads9224r_protocol_config_t		protocol;
	uint32_t						samplingRate;
}ads9224r_config_t;

typedef void (*bufferReceiveCallback)(uint32_t buffAddr);


ads9224r_status_t	ADS9224R_Init(ads9224r_config_t *ads9224r_config_t, uint32_t timeout);
ads9224r_status_t	ADS9224R_SetPatternState(ads9224r_fpattern_state_t state, uint32_t timeout);
ads9224r_status_t   ADS9224R_StartAcquisiton(uint8_t* sdoaBuffer0, uint8_t* sdoaBuffer1, uint8_t* sdobBuffer0, uint8_t* sdobBuffer1, uint32_t size);
ads9224r_status_t   ADS9224R_StopAcquisiton();
ads9224r_status_t   ADS9224R_SetConfig(ads9224r_config_t *config);
ads9224r_status_t   ADS9224R_GetConfig(ads9224r_config_t *config);
ads9224r_status_t   ADS9224R_SetSamplingRate(uint32_t timPeriod, uint32_t timPrescaler);
ads9224r_status_t   ADS9224R_RegisterCallback(bufferReceiveCallback callback, ads9224r_sdo_t trigger);
ads9224r_status_t   ADS9224R_SubmitBuffer(uint32_t buffAddr);


#endif /* CORE_DRIVERS_PLATFORM_ANALOGIN_ADS9224R_ADS9224R_H_ */
