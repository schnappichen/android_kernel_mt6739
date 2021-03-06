
#ifndef _PMIC_THROTTLING_DLPT_H_
#define _PMIC_THROTTLING_DLPT_H_

#include <linux/dcache.h>
#include <linux/platform_device.h>

/* just use in suspend flow for important log due to console suspend */
#if defined PMIC_DEBUG_PR_DBG
#define pmic_spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	pr_notice("[SPM-PMIC] " fmt, ##args);		\
} while (0)
#else
#define pmic_spm_crit2(fmt, args...)		\
do {					\
	aee_sram_printk(fmt, ##args);	\
	pr_info("[SPM-PMIC] " fmt, ##args);		\
} while (0)
#endif

extern int pmic_throttling_dlpt_init(void);
extern void low_battery_protect_init(void);
extern void battery_oc_protect_init(void);
extern void bat_percent_notify_init(void);
extern void dlpt_notify_init(void);
extern void pmic_throttling_dlpt_suspend(void);
extern void pmic_throttling_dlpt_resume(void);
extern void pmic_throttling_dlpt_debug_init(struct platform_device *dev, struct dentry *debug_dir);
extern void bat_h_int_handler(void);
extern void bat_l_int_handler(void);
extern void fg_cur_h_int_handler(void);
extern void fg_cur_l_int_handler(void);
extern int get_rac(void);
extern int get_imix(void);
extern int do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur);

#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
extern void mt6355_auxadc_dump_setting_regs(void);
extern void mt6355_auxadc_dump_clk_regs(void);
extern void mt6355_auxadc_dump_channel_regs(void);
#endif


#endif				/* _PMIC_THROTTLING_DLPT_H_ */
