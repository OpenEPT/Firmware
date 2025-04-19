/*
 * bq25150.h
 *
 *  Created on: Apr 12, 2025
 *      Author: elektronika
 */

#ifndef CORE_HAL_BQ25150_BQ25150_H_
#define CORE_HAL_BQ25150_BQ25150_H_

typedef enum
{
	BQ25150_STATUS_OK,
	BQ25150_STATUS_ERROR
}bq25150_status_t;

typedef enum
{
	BQ25150_WDG_STATUS_DISABLE	=	0,
	BQ25150_WDG_STATUS_ENABLE
}bq25150_wdg_status;

typedef enum
{
	BQ25150_CHARGE_STATUS_DISABLE = 0,
	BQ25150_CHARGE_STATUS_ENABLE
}bq25150_charge_status;

typedef enum
{
	BQ25150_ILIM_VALUE_50 	= 0,
	BQ25150_ILIM_VALUE_100	= 1,
	BQ25150_ILIM_VALUE_150	= 2,
	BQ25150_ILIM_VALUE_200	= 3,
	BQ25150_ILIM_VALUE_300	= 4,
	BQ25150_ILIM_VALUE_400	= 5,
	BQ25150_ILIM_VALUE_500	= 6,
	BQ25150_ILIM_VALUE_600	= 7
}bq25250_ilim_value_t;

bq25150_status_t BQ25150_Init();
bq25150_status_t BQ25150_Ping(uint32_t timeout);
bq25150_status_t BQ25150_WDG_SetStatus(bq25150_wdg_status status, uint32_t timeout);
bq25150_status_t BQ25150_ILim_Set(bq25250_ilim_value_t value, uint32_t timeout);
bq25150_status_t BQ25150_Charge_SetStatus(bq25150_charge_status status, uint32_t timeout);
bq25150_status_t BQ25150_Charge_Current_Set(uint32_t current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_Current_Get(uint32_t* current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_PreCurrent_Set(uint32_t current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_PreCurrent_Get(uint32_t* current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_TermCurrent_Set(uint32_t current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_TermCurrent_Get(uint32_t* current, uint32_t timeout);
bq25150_status_t BQ25150_Charge_RegVoltage_Set(float voltage, uint32_t timeout);
bq25150_status_t BQ25150_Charge_RegVoltage_Get(float* voltage, uint32_t timeout);

bq25150_status_t BQ25150_ReadReg(uint8_t regAddr, uint8_t* data, uint32_t timeout);


#endif /* CORE_HAL_BQ25150_BQ25150_H_ */
