
#ifndef _CUST_PMIC_H_
#define _CUST_PMIC_H_

/*#define PMIC_VDVFS_CUST_ENABLE*/

#define LOW_POWER_LIMIT_LEVEL_1 15

#define PT_DLPT_BRINGUP 0

#if defined(CONFIG_FPGA_EARLY_PORTING) || PT_DLPT_BRINGUP
/*Define for disable low battery protect feature, default no define for enable low battery protect.*/
/*Begin modified by rongxiao.deng for task 5451380 on 2017/11/27 */
#ifdef DISABLE_TEMPERATURE_DETECTION_AND_THERMAL_POLICY
#define DISABLE_LOW_BATTERY_PROTECT
#endif
/*End modified by rongxiao.deng for task 5451380 on 2017/11/27 */

/*Define for disable battery OC protect*/
#define DISABLE_BATTERY_OC_PROTECT

/*Define for disable battery 15% protect*/
#define DISABLE_BATTERY_PERCENT_PROTECT

/*Define for DLPT*/
#define DISABLE_DLPT_FEATURE
#endif /* defined(CONFIG_FPGA_EARLY_PORTING) || PT_DLPT_BRINGUP */

#define POWER_UVLO_VOLT_LEVEL 2600
#define IMAX_MAX_VALUE 5500

#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100

#define POWER_BAT_OC_CURRENT_H    4670
#define POWER_BAT_OC_CURRENT_L    5500
#define POWER_BAT_OC_CURRENT_H_RE 4670
#define POWER_BAT_OC_CURRENT_L_RE 5500

#define DLPT_POWER_OFF_EN
#define POWEROFF_BAT_CURRENT 3000
#define DLPT_POWER_OFF_THD 100

#define DLPT_VOLT_MIN 3100

#define BATTERY_MODULE_INIT

/* ADC Channel Number, not uesed and removed */

#endif /* _CUST_PMIC_H_ */
