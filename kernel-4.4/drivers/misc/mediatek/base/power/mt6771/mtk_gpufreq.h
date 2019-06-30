
#ifndef _MT_GPUFREQ_H_
#define _MT_GPUFREQ_H_

#include <linux/module.h>
#include <linux/clk.h>

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

typedef void (*gpufreq_power_limit_notify)(unsigned int); /* legacy */

#ifdef CONFIG_MTK_RAM_CONSOLE
#define MT_GPUFREQ_SRAM_DEBUG
#endif

extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int idx);
extern unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern void mt_gpufreq_restore_default_volt(void);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern unsigned int mt_gpufreq_get_max_power(void);
extern unsigned int mt_gpufreq_get_min_power(void);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_leakage_mw(void);
extern int mt_gpufreq_get_cur_ceiling_idx(void);
extern void mt_gpufreq_set_loading(unsigned int gpu_loading); /* legacy */
extern void mt_gpufreq_enable_CG(void);
extern void mt_gpufreq_disable_CG(void);
extern void mt_gpufreq_enable_MTCMOS(void);
extern void mt_gpufreq_disable_MTCMOS(void);
extern void mt_gpufreq_dump_reg(void);
extern void mt_gpufreq_dump_status(void);
extern void mt_gpufreq_set_MMU_AS_ACTIVE(int enable);
extern unsigned int mt_gpufreq_get_MMU_AS_ACTIVE(void);
extern int mt_gpufreq_check_GPU_non_idle_infra_idle(void);
extern void mt_gpufreq_reset_MFG_wrapper(void);
extern void mfg_latency_debug_enable(void);
extern void mfg_latency_debug_stopRead(void);

#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);
#endif /* CONFIG_MTK_RAM_CONSOLE */

#endif /* _MT_GPUFREQ_H_ */