
#include <linux/atomic.h>
#include "imgsensor_common.h"

#ifndef __IMGSENSOR_CFG_TABLE_H__
#define __IMGSENSOR_CFG_TABLE_H__

#define IMGSENSOR_DEV_NAME "kd_camera_hw"

#define IMGSENSOR_SENSOR_IDX_NAME_MAIN  "0"
#define IMGSENSOR_SENSOR_IDX_NAME_SUB   "1"
#define IMGSENSOR_SENSOR_IDX_NAME_MAIN2 "2"
#define IMGSENSOR_SENSOR_IDX_NAME_SUB2  "3"

#define IMGSENSOR_HW_POWER_INFO_MAX	12
#define IMGSENSOR_HW_SENSOR_MAX_NUM	8

enum IMGSENSOR_HW_PIN {
	IMGSENSOR_HW_PIN_NONE = 0,
	IMGSENSOR_HW_PIN_PDN,
	IMGSENSOR_HW_PIN_RST,
	IMGSENSOR_HW_PIN_AVDD,
	IMGSENSOR_HW_PIN_DVDD,
	IMGSENSOR_HW_PIN_DOVDD,
#ifdef MIPI_SWITCH
	IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
	IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
#endif
	IMGSENSOR_HW_PIN_MCLK,
	IMGSENSOR_HW_PIN_MAX_NUM,
	IMGSENSOR_HW_PIN_UNDEF = -1
};

enum IMGSENSOR_HW_PIN_STATE {
	IMGSENSOR_HW_PIN_STATE_LEVEL_0,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1000,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1100,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1200,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1210,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1220,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1500,
	IMGSENSOR_HW_PIN_STATE_LEVEL_1800,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2500,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2800,
	IMGSENSOR_HW_PIN_STATE_LEVEL_2900,
	IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,

	IMGSENSOR_HW_PIN_STATE_NONE = -1
};

/*Legacy design*/
enum PowerType {
	SensorMCLK = IMGSENSOR_HW_PIN_MCLK,
	PDN        = IMGSENSOR_HW_PIN_PDN,
	RST        = IMGSENSOR_HW_PIN_RST,
	AVDD       = IMGSENSOR_HW_PIN_AVDD,
	DVDD       = IMGSENSOR_HW_PIN_DVDD,
	DOVDD      = IMGSENSOR_HW_PIN_DOVDD,
	VDD_None   = IMGSENSOR_HW_PIN_NONE,

	/* For backward compatible */
	AFVDD      = IMGSENSOR_HW_PIN_UNDEF,
};

enum Voltage {
	Vol_Low  = IMGSENSOR_HW_PIN_STATE_LEVEL_0,
	Vol_High = IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
	Vol_1000 = IMGSENSOR_HW_PIN_STATE_LEVEL_1000,
	Vol_1100 = IMGSENSOR_HW_PIN_STATE_LEVEL_1100,
	Vol_1200 = IMGSENSOR_HW_PIN_STATE_LEVEL_1200,
	Vol_1210 = IMGSENSOR_HW_PIN_STATE_LEVEL_1210,
	Vol_1220 = IMGSENSOR_HW_PIN_STATE_LEVEL_1220,
	Vol_1500 = IMGSENSOR_HW_PIN_STATE_LEVEL_1500,
	Vol_1800 = IMGSENSOR_HW_PIN_STATE_LEVEL_1800,
	Vol_2500 = IMGSENSOR_HW_PIN_STATE_LEVEL_2500,
	Vol_2800 = IMGSENSOR_HW_PIN_STATE_LEVEL_2800,
	Vol_2900 = IMGSENSOR_HW_PIN_STATE_LEVEL_2900,
};

enum IMGSENSOR_HW_ID {
	IMGSENSOR_HW_ID_MCLK,
	IMGSENSOR_HW_ID_REGULATOR,
	IMGSENSOR_HW_ID_GPIO,

	IMGSENSOR_HW_ID_MAX_NUM,
	IMGSENSOR_HW_ID_NONE = -1
};

#define IMGSENSOR_I2C_DRV_NAME_0  "kd_camera_hw"
#define IMGSENSOR_I2C_DRV_NAME_1  "kd_camera_hw_bus2"
#define IMGSENSOR_I2C_DRV_NAME_2  "kd_camera_hw_trigger"

#define IMGSENSOR_I2C_OF_DRV_NAME_0 "mediatek,camera_main"
#define IMGSENSOR_I2C_OF_DRV_NAME_1 "mediatek,camera_sub"
#define IMGSENSOR_I2C_OF_DRV_NAME_2 "mediatek,camera_main_hw"

enum IMGSENSOR_I2C_DEV {
	IMGSENSOR_I2C_DEV_0,
	IMGSENSOR_I2C_DEV_1,
	IMGSENSOR_I2C_DEV_2,
	IMGSENSOR_I2C_DEV_MAX_NUM,
};

#endif

