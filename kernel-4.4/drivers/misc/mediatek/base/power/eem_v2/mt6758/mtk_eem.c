

#define __MTK_EEM_C__

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/topology.h>

/* project includes */


#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
	#if defined(CONFIG_MTK_CLKMGR)
		#include <mach/mt_clkmgr.h>
	#else
		#include <linux/clk.h>
	#endif
#endif

/* local includes (kernel-4.4)*/
#include <mt-plat/mtk_chip.h>
#include <mt-plat/mtk_gpio.h>
#include "mt-plat/upmu_common.h"
#include "mtk_thermal.h"
#include "mtk_ppm_api.h"
#include "mtk_cpufreq_api.h"
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_defeem.h"
#if !(EEM_ENABLE_TINYSYS_SSPM)
	#include "mtk_eem_internal_ap.h"
#else
	#include "mtk_eem_internal_sspm.h"
#endif
#include "mtk_eem_internal.h"
#include "mtk_spm_vcore_dvfs.h"
#include "mtk_gpufreq.h"
/* #include "emi_bwl.h" todo */
#include <mt-plat/mtk_devinfo.h>
#include <regulator/consumer.h>
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	#include "pmic_regulator.h"
	#include "mtk_pmic_regulator.h"
	#include "mtk_pmic_api_buck.h"
#endif

#if UPDATE_TO_UPOWER
#include "mtk_upower.h"
#endif

#if 0
#include  "mt-plat/elbrus/include/mach/mt_cpufreq_api.h"
#include "mach/mt_ppm_api.h"
#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
#include "../../../pmic/include/mt6313/mt6313.h"
#endif
#endif

#if EEM_ENABLE_TINYSYS_SSPM
	#include "sspm_ipi.h"
	#include <sspm_reservedmem_define.h>
#endif /* if EEM_ENABLE_TINYSYS_SSPM */

#if !(EEM_ENABLE_TINYSYS_SSPM)

	#if EEM_ENABLE
	static unsigned int ctrl_EEM_Enable = 1;
	#else
	static unsigned int ctrl_EEM_Enable;
	#endif /* EEM_ENABLE */

	/* Get time stmp to known the time period */
	static unsigned long long eem_pTime_us, eem_cTime_us, eem_diff_us;
	/* 0 : signed off volt
	* 1 : voltage bin volt
	*/
	/* static unsigned int ctrl_VCORE_Volt_Enable;*/

	#if ITurbo
	unsigned int ctrl_ITurbo = 0, ITurboRun;
	static unsigned int ITurbo_offset[16];
	#endif /* ITurbo */

	#ifndef EARLY_PORTING_PMIC
		/* for setting pmic pwm mode and auto mode */
		struct regulator *eem_regulator_proc1;
		struct regulator *eem_regulator_gpu;

		#if EEM_BANK_SOC
		struct regulator *eem_regulator_vcore;
		static unsigned int eem_vcore_is_enabled_by_eem;
		#endif

		#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
		static unsigned int eem_vproc1_is_enabled_by_eem;
		static unsigned int eem_vgpu_is_enabled_by_eem;
		#endif
	#endif


	#if 0 /* no record table */
	u32 *recordRef;
	#endif

	static void eem_set_eem_volt(struct eem_det *det);
	static void eem_restore_eem_volt(struct eem_det *det);
	static unsigned int mt_eem_update_vcore_volt(unsigned int index, unsigned int newVolt);

	#if DVT
	unsigned int freq[NR_FREQ] = {0x64, 0x59, 0x4D, 0x3D, 0x35, 0x2C, 0x1E, 0x0E};
	#endif /* DVT */

	#if EEM_BANK_SOC /* khz */
		unsigned char vcore_freq[NR_FREQ] = {0x64, 0x53, 0x32, 0x19};
	#endif

	unsigned int gpuOutput[NR_FREQ]; /* (8)final volt table to gpu */
	unsigned int record_tbl_locked[NR_FREQ]; /* table used to apply to dvfs at final */

	/*static unsigned int *recordTbl;*//* no record table */
	/*static unsigned int *gpuTbl;*/ /* used to record gpu volt table */
	#ifdef BINLEVEL_BYEFUSE
	static unsigned int cpu_speed;
	#endif

	DEFINE_MUTEX(record_mutex);

	#if ITurbo/* irene */
	/* CPU callback */
	static int __cpuinit _mt_eem_cpu_CB(struct notifier_block *nfb,
					unsigned long action, void *hcpu);
	static struct notifier_block __refdata _mt_eem_cpu_notifier = {
		.notifier_call = _mt_eem_cpu_CB,
	};
	#endif /* if ITurbo */

	static struct eem_devinfo eem_devinfo;

	/**
	* timer for log
	*/
	static struct hrtimer eem_log_timer;

	/* Global variable for I DVFS use */
	unsigned int infoIdvfs;
	static DEFINE_SPINLOCK(eem_spinlock);

#else /* if EEM_ENABLE_TINYSYS_SSPM */
	/* #define NR_FREQ 8 */ /* for sspm struct eem_det */
	phys_addr_t eem_log_phy_addr, eem_log_virt_addr;
	uint32_t eem_log_size;
	static unsigned int eem_disable = 1;
	struct eem_log *eem_logs;

	/* functions declaration */
	static unsigned int eem_to_sspm(unsigned int cmd, struct eem_ipi_data *eem_data);

#endif /* if EEM_ENABLE_TINYSYS_SSPM */

#if (!defined(CONFIG_MTK_CLKMGR)) && !(EARLY_PORTING)
struct clk *clk_mfg0, *clk_mfg1, *clk_mfg2, *clk_mfg3;
#endif

/* SOC v1 Voltage (10uv)*/
static unsigned int vcore_opp_L4[VCORE_NR_FREQ][VCORE_NR_FREQ_EFUSE] = {
	{80000, 0, 0, 0},
	{70000, 0, 0, 0},
	{70000, 0, 0, 0},
};

static unsigned int vcore_opp_L3[VCORE_NR_FREQ][VCORE_NR_FREQ_EFUSE] = {
	{80000, 0, 0, 0},
	{80000, 0, 0, 0},
	{70000, 0, 0, 0},
};

/* ptr that points to L4 or L3 opp table */
unsigned int (*vcore_opp)[VCORE_NR_FREQ_EFUSE];
/* final vcore opp table */
unsigned int eem_vcore[VCORE_NR_FREQ];
/* record index for vcore opp table from efuse */
unsigned int eem_vcore_index[VCORE_NR_FREQ] = {0};
/* static unsigned int eem_chip_ver;*/
static int eem_log_en;
static unsigned int eem_checkEfuse = 1;
static unsigned int informEEMisReady;

/* The EMM controller list managed by Picachu. */
static unsigned int pi_eem_ctrl_id[] = {
	EEM_CTRL_2L, EEM_CTRL_L, EEM_CTRL_CCI
};

#ifdef CONFIG_OF
void __iomem *eem_base;
static u32 eem_irq_number;
#endif

/* Definition for Picachu */
#define PI_PTP1_MTDES_START_BIT		(0)
#define PI_PTP1_BDES_START_BIT		(8)
#define PI_PTP1_MDES_START_BIT		(16)

unsigned int mt_eem_is_enabled(void)
{
	return informEEMisReady;
}

static struct eem_det *id_to_eem_det(enum eem_det_id id)
{
	if (likely(id < NR_EEM_DET))
		return &eem_detectors[id];
	else
		return NULL;
}

static void inherit_base_det_transfer_fops(struct eem_det *det)
{
	/*
	 * Inherit ops from eem_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

	#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = eem_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, volt_2_pmic);
	INIT_OP(det->ops, volt_2_eem);
	INIT_OP(det->ops, pmic_2_volt);
	INIT_OP(det->ops, eem_2_pmic);

	FUNC_EXIT(FUNC_LV_HELP);
}

int __attribute__((weak))
get_dram_type(void)
{
	eem_debug("cannot find %s!!\n", __func__);
	return 0;
}

static void get_vcore_opp(void)
{
	/* default setting */
	int ddr_type = get_dram_type();

	if (0) /* (ddr_type == TYPE_LPDDR3) todo */
		vcore_opp = &vcore_opp_L3[0];
	else
		vcore_opp = &vcore_opp_L4[0];

	eem_debug("@%s():%d, ddr_type = %d\n",
		__func__, __LINE__, ddr_type);
}

#if !EEM_BANK_SOC
static void get_soc_efuse(void)
{
	unsigned int soc_efuse;

	/* soc_efuse = get_devinfo_with_index(DEVINFO_IDX_10); */
	/* no voltage bin, use default opp 0 */
	soc_efuse = 0;

	eem_debug("[VCORE] - Kernel Got efuse 0x%0X\n", soc_efuse);
	eem_vcore_index[0] = GET_BITS_VAL(5:4, soc_efuse);
	eem_vcore_index[1] = GET_BITS_VAL(3:2, soc_efuse);
	eem_vcore_index[2] = GET_BITS_VAL(1:0, soc_efuse);
	eem_debug("eem_vcore_index:%d, %d, %d\n", eem_vcore_index[0], eem_vcore_index[1],
				eem_vcore_index[2]);
}
#endif

/* done at subsys_init */
static int __init vcore_ptp_init(void)
{
	int i = 0;
	struct eem_det *det;

	/* if vcore opp passed from preloader, use this */
	/* of_scan_flat_dt(dt_get_ptp_devinfo, NULL); */

	get_vcore_opp();

#if (EEM_ENABLE_TINYSYS_SSPM)
	eem_log_phy_addr = sspm_reserve_mem_get_phys(PTPOD_MEM_ID);
	eem_log_virt_addr = sspm_reserve_mem_get_virt(PTPOD_MEM_ID);
	eem_log_size = sspm_reserve_mem_get_size(PTPOD_MEM_ID);
	eem_logs = (struct eem_log *)eem_log_virt_addr;
#endif


#if !EEM_BANK_SOC
	get_soc_efuse();
#endif

	for_each_det(det)
		inherit_base_det_transfer_fops(det);

	det = id_to_eem_det(EEM_DET_SOC);

	if (det->ops->volt_2_pmic) {
		for (i = 0; i < VCORE_NR_FREQ; i++)
			eem_vcore[i] = det->ops->volt_2_pmic(det, *(vcore_opp[i]+eem_vcore_index[i]));
	}

	/*vcore1 = (vcore1 < vcore2) ? vcore2 : vcore1;*/
	/*vcore0 = (vcore0 < vcore1) ? vcore1 : vcore0;*/
	/* bottom up compare each volt to ensure each opp is in descending order */
	for (i = VCORE_NR_FREQ - 2; i >= 0; i--)
		eem_vcore[i] = (eem_vcore[i] < eem_vcore[i+1]) ? eem_vcore[i+1] : eem_vcore[i];

#if (EEM_ENABLE_TINYSYS_SSPM)
	for (i = 0; i < VCORE_NR_FREQ; i++)
		eem_logs->det_log[EEM_DET_SOC].volt_tbl_pmic[i] = eem_vcore[i];

	eem_debug("Got vcore volt(pmic): 0x%x 0x%x 0x%x 0x%x\n",
				eem_logs->det_log[EEM_DET_SOC].volt_tbl_pmic[0],
				eem_logs->det_log[EEM_DET_SOC].volt_tbl_pmic[1],
				eem_logs->det_log[EEM_DET_SOC].volt_tbl_pmic[2],
#else
	eem_debug("Got vcore volt(pmic): 0x%x 0x%x 0x%x\n",
				eem_vcore[0], eem_vcore[1], eem_vcore[2]);
#endif
	return 0;
}

/* return ack from sspm spm_vcorefs_pwarp_cmd */
int __attribute__((weak))
spm_vcorefs_pwarp_cmd(void)
{
	eem_debug("cannot find %s!!\n", __func__);
	return 0;
}

unsigned int mt_eem_vcorefs_set_volt(void)
{
	int ret = 0;
#if (EEM_ENABLE_TINYSYS_SSPM)
	struct eem_ipi_data eem_data;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ret = eem_to_sspm(IPI_EEM_VCORE_INIT, &eem_data);
#else
#ifndef EARLY_PORTING_VCORE
	ret = spm_vcorefs_pwarp_cmd();
#endif
#endif
	return ret;
}

/* return vcore pmic value to vcore dvfs(pmic) */
unsigned int get_vcore_ptp_volt(unsigned int seg)
{
#if (EEM_ENABLE_TINYSYS_SSPM)
	return eem_logs->det_log[EEM_DET_SOC].volt_tbl_pmic[seg];
#else
	unsigned int ret = 0;

	if (seg < VCORE_NR_FREQ)
		ret = eem_vcore[seg];
	else
		eem_debug("VCORE: wrong segment\n");

	return ret;
#endif
}

#if EEM_BANK_BIG
static void mt_eem_enable_big_cluster(unsigned int enable)
{
	if (enable == 1) {
		set_cpu_present(8, true);
		if (!cpu_online(8))
			cpu_up(8);

	} else if (enable == 0) {
		if (cpu_online(8))
			cpu_down(8);
	}
	eem_debug("eem_enable_big_cluster %d\n", enable);
}
#endif

/* enable mtcmos for GPU ptp detector */
static void mt_eem_get_clk(struct platform_device *pdev)
{
	#if !defined(CONFIG_MTK_CLKMGR) && !(EARLY_PORTING)
	clk_mfg0 = devm_clk_get(&pdev->dev, "mtcmos-mfg0");
	if (IS_ERR(clk_mfg0))
		eem_debug("cannot get mtcmos-mfg-sync\n");

	clk_mfg1 = devm_clk_get(&pdev->dev, "mtcmos-mfg1");
	if (IS_ERR(clk_mfg1))
		eem_debug("cannot get mtcmos-mfg-top\n");

	clk_mfg2 = devm_clk_get(&pdev->dev, "mtcmos-mfg2");
	if (IS_ERR(clk_mfg2))
		eem_debug("cannot get mtcmos-shader0\n");

	clk_mfg3 = devm_clk_get(&pdev->dev, "mtcmos-mfg3");
	if (IS_ERR(clk_mfg3))
		eem_debug("cannot get mtcmos-shader1\n");
	#endif
}
static void mt_eem_enable_mtcmos(void)
{
	/* spm_mtcmos_ctrl_mfg1(enable); */
	/* spm_mtcmos_ctrl_mfg2(enable); */
	int ret;
	#if !defined(CONFIG_MTK_CLKMGR) && !(EARLY_PORTING)
	ret = clk_prepare_enable(clk_mfg0); /* gpu mtcmos enable*/
	if (ret)
		eem_debug("clk_prepare_enable failed when enabling clk_mfg_sync\n");

	ret = clk_prepare_enable(clk_mfg1); /* gpu mtcmos enable*/
	if (ret)
		eem_debug("clk_prepare_enable failed when enabling clk_mfg_top\n");

	ret = clk_prepare_enable(clk_mfg2); /* gpu mtcmos enable*/
	if (ret)
		eem_debug("clk_prepare_enable failed when enabling clk_shader0\n");

	ret = clk_prepare_enable(clk_mfg3); /* gpu mtcmos enable*/
	if (ret)
		eem_debug("clk_prepare_enable failed when enabling clk_shader1\n");
	#endif /* if !defined CONFIG_MTK_CLKMGR */

	eem_debug("mt_eem_enable_mtcmos done\n");
}

static void mt_eem_disable_mtcmos(void)
{
	#if !defined(CONFIG_MTK_CLKMGR) && !(EARLY_PORTING)
	clk_disable_unprepare(clk_mfg3);
	clk_disable_unprepare(clk_mfg2);
	clk_disable_unprepare(clk_mfg1);
	clk_disable_unprepare(clk_mfg0);
	#endif /* EARLY_PORTING */
	eem_debug("mt_eem_disable_mtcmos done\n");
}

static int get_devinfo(void)
{
	unsigned int mtdes_idx, bdes_mdes_idx;
	struct eem_det *det;
	int ret = 0;
	int *val;
	int i = 0;
	int ftpgm_ver = 0;


	FUNC_ENTER(FUNC_LV_HELP);
	/* if legacy eem, save efuse data into eem_devinfo */
	/* if sspm eem, save efuse data into logs */
	#if !(EEM_ENABLE_TINYSYS_SSPM)
	val = (int *)&eem_devinfo;
	#else
	val = (int *)eem_logs->hw_res;
	#endif

	#if !DVT
		val[0] = get_devinfo_with_index(DEVINFO_IDX_0); /* FTPGM 0x10450580 */
		val[1] = get_devinfo_with_index(DEVINFO_IDX_1); /* LL 0x10450584 */
		val[2] = get_devinfo_with_index(DEVINFO_IDX_2); /* LL 0x10450588 */
		val[3] = get_devinfo_with_index(DEVINFO_IDX_3); /* L 0x1045058c */
		val[4] = get_devinfo_with_index(DEVINFO_IDX_4); /* L 0x10450590 */
		val[5] = get_devinfo_with_index(DEVINFO_IDX_5); /* CCI 0x10450594 */
		val[6] = get_devinfo_with_index(DEVINFO_IDX_6); /* CCI 0x10450598 */
		val[7] = get_devinfo_with_index(DEVINFO_IDX_7); /* GPU 0x1045059c */
		val[8] = get_devinfo_with_index(DEVINFO_IDX_8); /* GPU 0x104505a0 */
		val[9] = get_devinfo_with_index(DEVINFO_IDX_9); /* SOC 0x104505a4 */
		val[10] = get_devinfo_with_index(DEVINFO_IDX_10); /* SOC 0x104505a8 */

		for (i = 1; i <= NR_HW_RES_FOR_BANK; i++) {
			if (val[i] == 0) {
				ret = 1;
				eem_checkEfuse = 0;
				eem_debug("No EEM EFUSE available, will apply safe EFUSE for EEM (val[%d] !!\n", i);
				break;
			}
		}

		if (eem_checkEfuse == 0) {
			/* for verification */
			val[0] = DEVINFO_0;
			val[1] = DEVINFO_1;
			val[2] = DEVINFO_2;
			val[3] = DEVINFO_3;
			val[4] = DEVINFO_4;
			val[5] = DEVINFO_5;
			val[6] = DEVINFO_6;
			val[7] = DEVINFO_7;
			val[8] = DEVINFO_8;
			val[9] = DEVINFO_9;
			val[10] = DEVINFO_10;
		}

		/* get ftpgm */
		#if !(EEM_ENABLE_TINYSYS_SSPM)
		ftpgm_ver = eem_devinfo.FT_PGM;
		#else
		ftpgm_ver = GET_BITS_VAL(7:4, eem_logs->hw_res[0]);
		#endif

		#if defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING)
			aee_rr_rec_ptp_e0((unsigned int)val[0]);
			aee_rr_rec_ptp_e1((unsigned int)val[1]);
			aee_rr_rec_ptp_e2((unsigned int)val[2]);
			aee_rr_rec_ptp_e3((unsigned int)val[3]);
			aee_rr_rec_ptp_e4((unsigned int)val[4]);
			aee_rr_rec_ptp_e5((unsigned int)val[5]);
			aee_rr_rec_ptp_e6((unsigned int)val[6]);
			aee_rr_rec_ptp_e7((unsigned int)val[7]);
			aee_rr_rec_ptp_e8((unsigned int)val[8]);
			aee_rr_rec_ptp_e9((unsigned int)val[9]);
			aee_rr_rec_ptp_e10((unsigned int)val[10]);
		#endif
	#else
	/* for DVT */
	val[0] = DEVINFO_DVT_0;
	val[1] = DEVINFO_DVT_1;
	val[2] = DEVINFO_DVT_2;
	val[3] = DEVINFO_DVT_3;
	val[4] = DEVINFO_DVT_4;
	val[5] = DEVINFO_DVT_5;
	val[6] = DEVINFO_DVT_6;
	val[7] = DEVINFO_DVT_7;
	val[8] = DEVINFO_DVT_8;
	val[9] = DEVINFO_DVT_9;
	val[10] = DEVINFO_DVT_10;
	#endif

	/* Update MTDES/BDES/MDES if they are modified by PICACHU. */
	for (i = 0; i < sizeof(pi_eem_ctrl_id) / sizeof(unsigned int); i++) {
		det = id_to_eem_det(pi_eem_ctrl_id[i]);

		if (!det->pi_efuse)
			continue;

		eem_debug("ctrl_id: %d, pi_efuse: 0x%x\n",
						det->ctrl_id, det->pi_efuse);

		if (det->ctrl_id != EEM_CTRL_CCI) {
			bdes_mdes_idx = (det->ctrl_id << 1) + 1;
		} else {
			/* CCI index */
			bdes_mdes_idx = 5;
		}

		mtdes_idx = bdes_mdes_idx + 1;

		/* Update BDES */
		val[bdes_mdes_idx] = (val[bdes_mdes_idx] & 0xFFFFFF00) |
		((det->pi_efuse >> PI_PTP1_BDES_START_BIT) & 0xff);

		/* Update MDES */
		val[bdes_mdes_idx] = (val[bdes_mdes_idx] & 0xFFFF00FF) |
		(((det->pi_efuse >> PI_PTP1_BDES_START_BIT) & 0x0000FF00));

		/* Update MTDES */
		val[mtdes_idx] = (val[mtdes_idx] & 0xFF00FFFF) |
		(((det->pi_efuse >> PI_PTP1_MTDES_START_BIT) & 0xff) << 16);
	}

	eem_debug("M_HW_RES0 = 0x%08X\n", val[0]);
	eem_debug("M_HW_RES1 = 0x%08X\n", val[1]);
	eem_debug("M_HW_RES2 = 0x%08X\n", val[2]);
	eem_debug("M_HW_RES3 = 0x%08X\n", val[3]);
	eem_debug("M_HW_RES4 = 0x%08X\n", val[4]);
	eem_debug("M_HW_RES5 = 0x%08X\n", val[5]);
	eem_debug("M_HW_RES6 = 0x%08X\n", val[6]);
	eem_debug("M_HW_RES7 = 0x%08X\n", val[7]);
	eem_debug("M_HW_RES8 = 0x%08X\n", val[8]);
	eem_debug("M_HW_RES9 = 0x%08X\n", val[9]);
	eem_debug("M_HW_RES10 = 0x%08X\n", val[10]);
	eem_debug("eem_checkEfuse = %d\n", eem_checkEfuse);

	eem_debug("ftpgm %d\n", ftpgm_ver);

	FUNC_EXIT(FUNC_LV_HELP);
	return ret;
}

unsigned char mt_eem_get_turbo(void)
{
	unsigned char ret = 0x0;

	#if !(EEM_ENABLE_TINYSYS_SSPM)
	/* ret = eem_devinfo.TURBO; */
	#else
	/* eem_debug("big efuse: 0x%0x\n", eem_logs->hw_res[HW_RES_IDX_TURBO]); todo */
	/* ret = GET_BITS_VAL(27:27, eem_logs->hw_res[HW_RES_IDX_TURBO]); */
	#endif

	return ret;
}
#if !(EEM_ENABLE_TINYSYS_SSPM)
static void mt_ptp_lock(unsigned long *flags);
static void mt_ptp_unlock(unsigned long *flags);
#endif /* if !EEM_ENABLE_TINYSYS_SSPM */

#if !(EEM_ENABLE_TINYSYS_SSPM)

#if defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING)
static void _mt_eem_aee_init(void)
{
	aee_rr_rec_ptp_vboot(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_big_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_gpu_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_little_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_2_little_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_1(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_2(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_cpu_cci_volt_3(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_temp(0xFFFFFFFFFFFFFFFF);
	aee_rr_rec_ptp_status(0xFF);
}
#endif /* if defined(CONFIG_EEM_AEE_RR_REC) */

/* common part in thermal */
int __attribute__((weak))
tscpu_get_temp_by_bank(enum thermal_bank_name ts_bank)
{
	eem_debug("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}

int __attribute__((weak))
tscpu_is_temp_valid(void)
{
	eem_debug("cannot find %s (thermal has not ready yet!)\n", __func__);
	return 0;
}

static struct eem_ctrl *id_to_eem_ctrl(enum eem_ctrl_id id)
{
	if (likely(id < NR_EEM_CTRL))
		return &eem_ctrls[id];
	else
		return NULL;
}

void base_ops_enable(struct eem_det *det, int reason)
{
	/* FIXME: UNDER CONSTRUCTION */
	FUNC_ENTER(FUNC_LV_HELP);
	det->disabled &= ~reason;
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_switch_bank(struct eem_det *det, enum eem_phase phase)
{
	unsigned int coresel;

	FUNC_ENTER(FUNC_LV_HELP);
	coresel = (eem_read(EEMCORESEL) & ~BITMASK(2:0)) | BITS(2:0, det->ctrl_id);

	/* 803f0000 + det->ctrl_id = enable ctrl's swcg clock */
	/* 003f0000 + det->ctrl_id = disable ctrl's swcg clock */
	/* bug: when system resume, need to restore coresel value */
	if (phase == EEM_PHASE_INIT01) {
		coresel |= CORESEL_VAL;
	} else {
		coresel |= CORESEL_INIT2_VAL;
		coresel &= 0x0fffffff;
	}

	eem_write(EEMCORESEL, coresel);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable_locked(struct eem_det *det, int reason)
{
	FUNC_ENTER(FUNC_LV_HELP);

	switch (reason) {
	case BY_MON_ERROR:
		/* disable EEM */
		eem_write(EEMEN, 0x0);

		/* Clear EEM interrupt EEMINTSTS */
		eem_write(EEMINTSTS, 0x00ffffff);
		/* fall through */

	case BY_PROCFS_INIT2: /* 2 */
		/* set init2 value to DVFS table (PMIC) */
		memcpy(det->volt_tbl, det->volt_tbl_init2, sizeof(det->volt_tbl_init2));
		#if UPDATE_TO_UPOWER
		det->set_volt_to_upower = 0;
		#endif
		eem_set_eem_volt(det);
		det->disabled |= reason;
		eem_debug("det->disabled=%x", det->disabled);
		break;

	case BY_INIT_ERROR:
		/* disable EEM */
		eem_write(EEMEN, 0x0);

		/* Clear EEM interrupt EEMINTSTS */
		eem_write(EEMINTSTS, 0x00ffffff);
		/* fall through */

	case BY_PROCFS: /* 1 */
		det->disabled |= reason;
		eem_debug("det->disabled=%x", det->disabled);
		/* restore default DVFS table (PMIC) */
		eem_restore_eem_volt(det);
		break;

	default:
		eem_debug("det->disabled=%x\n", det->disabled);
		det->disabled &= ~BY_PROCFS;
		det->disabled &= ~BY_PROCFS_INIT2;
		eem_debug("det->disabled=%x\n", det->disabled);
		eem_set_eem_volt(det);
		break;
	}

	eem_debug("Disable EEM[%s] done. reason=[%d]\n", det->name, det->disabled);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_disable(struct eem_det *det, int reason)
{
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det, NR_EEM_PHASE);
	det->ops->disable_locked(det, reason);
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_init01(struct eem_det *det)
{
	/* struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id); */

	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT01))) {
		eem_debug("det %s has no INIT01\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	#if 0
	if (det->disabled & BY_PROCFS) {
		eem_debug("[%s] Disabled by PROCFS\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
	#endif

	/* atomic_inc(&ctrl->in_init); */
	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, EEM_PHASE_INIT01);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_init02(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	if (unlikely(!HAS_FEATURE(det, FEA_INIT02))) {
		eem_debug("det %s has no INIT02\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eem_debug("[%s] Disabled by INIT_ERROR\n", ((char *)(det->name) + 8));
		det->ops->dump_status(det);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}
	eem_debug("DCV = 0x%08X, AGEV = 0x%08X\n", det->DCVOFFSETIN, det->AGEVOFFSETIN);

	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, EEM_PHASE_INIT02);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_mon_mode(struct eem_det *det)
{
	#if defined(CONFIG_THERMAL) && !defined(EARLY_PORTING_THERMAL)
	struct TS_PTPOD ts_info;
	enum thermal_bank_name ts_bank;
	#endif

	FUNC_ENTER(FUNC_LV_HELP);

	if (!HAS_FEATURE(det, FEA_MON)) {
		eem_debug("det %s has no MON mode\n", det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	if (det->disabled & BY_INIT_ERROR) {
		eem_debug("[%s] Disabled BY_INIT_ERROR\n", ((char *)(det->name) + 8));
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	#ifndef EARLY_PORTING_THERMAL
		#if !defined(CONFIG_THERMAL)
		det->MTS = MTS_VAL;
		det->BTS = BTS_VAL;
		#else
		ts_bank = det->ctrl_id;
		get_thermal_slope_intercept(&ts_info, ts_bank);
		det->MTS = ts_info.ts_MTS;
		det->BTS = ts_info.ts_BTS;
		#endif
	#else
		det->MTS =  MTS_VAL; /* orig: 0x2cf, (2048 * TS_SLOPE) + 2048; */
		det->BTS =  BTS_VAL; /* orig: 0x80E, 4 * TS_INTERCEPT; */
	#endif

	/* eem_debug("[base_ops_mon_mode] Bk = %d, MTS = 0x%08X, BTS = 0x%08X\n",
	*			det->ctrl_id, det->MTS, det->BTS);
	*/
	#if 0
	if ((det->EEMINITEN == 0x0) || (det->EEMMONEN == 0x0)) {
		eem_debug("EEMINITEN = 0x%08X, EEMMONEN = 0x%08X\n", det->EEMINITEN, det->EEMMONEN);
		FUNC_EXIT(FUNC_LV_HELP);
		return 1;
	}
	#endif
	/* det->ops->dump_status(det); */
	det->ops->set_phase(det, EEM_PHASE_MON);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_get_status(struct eem_det *det)
{
	int status;
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	mt_ptp_lock(&flags);
	det->ops->switch_bank(det, NR_EEM_PHASE);
	status = (eem_read(EEMEN) != 0) ? 1 : 0;
	mt_ptp_unlock(&flags);

	FUNC_EXIT(FUNC_LV_HELP);

	return status;
}

void base_ops_dump_status(struct eem_det *det)
{
	unsigned int i;

	FUNC_ENTER(FUNC_LV_HELP);

	eem_isr_info("[%s]\n",			det->name);

	eem_isr_info("EEMINITEN = 0x%08X\n",	det->EEMINITEN);
	eem_isr_info("EEMMONEN = 0x%08X\n",	det->EEMMONEN);
	eem_isr_info("MDES = 0x%08X\n",		det->MDES);
	eem_isr_info("BDES = 0x%08X\n",		det->BDES);
	eem_isr_info("DCMDET = 0x%08X\n",	det->DCMDET);

	eem_isr_info("DCCONFIG = 0x%08X\n",	det->DCCONFIG);
	eem_isr_info("DCBDET = 0x%08X\n",	det->DCBDET);

	eem_isr_info("AGECONFIG = 0x%08X\n",	det->AGECONFIG);
	eem_isr_info("AGEM = 0x%08X\n",		det->AGEM);

	/* eem_isr_info("AGEDELTA = 0x%08X\n",	det->AGEDELTA); */
	eem_isr_info("DVTFIXED = 0x%08X\n",	det->DVTFIXED);
	eem_isr_info("MTDES = 0x%08X\n",	det->MTDES);
	eem_isr_info("VCO = 0x%08X\n",		det->VCO);

	eem_isr_info("DETWINDOW = 0x%08X\n",	det->DETWINDOW);
	eem_isr_info("VMAX = 0x%08X\n",		det->VMAX);
	eem_isr_info("VMIN = 0x%08X\n",		det->VMIN);
	eem_isr_info("DTHI = 0x%08X\n",		det->DTHI);
	eem_isr_info("DTLO = 0x%08X\n",		det->DTLO);
	eem_isr_info("VBOOT = 0x%08X\n",	det->VBOOT);
	eem_isr_info("DETMAX = 0x%08X\n",	det->DETMAX);

	eem_isr_info("DCVOFFSETIN = 0x%08X\n",	det->DCVOFFSETIN);
	eem_isr_info("AGEVOFFSETIN = 0x%08X\n",	det->AGEVOFFSETIN);

	eem_isr_info("MTS = 0x%08X\n",		det->MTS);
	eem_isr_info("BTS = 0x%08X\n",		det->BTS);

	eem_isr_info("num_freq_tbl = %d\n", det->num_freq_tbl);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("freq_tbl[%d] = %d\n", i, det->freq_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl[%d] = %d\n", i, det->volt_tbl[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_init2[%d] = %d\n", i, det->volt_tbl_init2[i]);

	for (i = 0; i < det->num_freq_tbl; i++)
		eem_isr_info("volt_tbl_pmic[%d] = %d\n", i, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_set_phase(struct eem_det *det, enum eem_phase phase)
{
	unsigned int i, filter, val;
	/* unsigned long flags; */

	FUNC_ENTER(FUNC_LV_HELP);

	/* mt_ptp_lock(&flags); */

	det->ops->switch_bank(det, phase);

	/* config EEM register */
	eem_write(EEM_DESCHAR,
		  ((det->BDES << 8) & 0xff00) | (det->MDES & 0xff));
	eem_write(EEM_TEMPCHAR,
		  (((det->VCO << 16) & 0xff0000) |
		   ((det->MTDES << 8) & 0xff00) | (det->DVTFIXED & 0xff)));
	eem_write(EEM_DETCHAR,
		  ((det->DCBDET << 8) & 0xff00) | (det->DCMDET & 0xff));
/* MT6758 do not have AGEDELTA */
#if 0
	eem_write(EEM_AGECHAR,
		  ((det->AGEDELTA << 8) & 0xff00) | (det->AGEM & 0xff));
#endif
	eem_write(EEM_DCCONFIG, det->DCCONFIG);
	eem_write(EEM_AGECONFIG, det->AGECONFIG);

	if (phase == EEM_PHASE_MON)
		eem_write(EEM_TSCALCS,
			  ((det->BTS << 12) & 0xfff000) | (det->MTS & 0xfff));

	if (det->AGEM == 0x0)
		eem_write(EEM_RUNCONFIG, 0x80000000);
	else {
		val = 0x0;

		for (i = 0; i < 24; i += 2) {
			filter = 0x3 << i;

			if (((det->AGECONFIG) & filter) == 0x0)
				val |= (0x1 << i);
			else
				val |= ((det->AGECONFIG) & filter);
		}

		eem_write(EEM_RUNCONFIG, val);
	}

	eem_write(EEM_FREQPCT30,
		  ((det->freq_tbl[3 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000)	|
		  ((det->freq_tbl[2 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000)	|
		  ((det->freq_tbl[1 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00)	|
		  (det->freq_tbl[0] & 0xff));
	eem_write(EEM_FREQPCT74,
		  ((det->freq_tbl[7 * ((det->num_freq_tbl + 7) / 8)] << 24) & 0xff000000)	|
		  ((det->freq_tbl[6 * ((det->num_freq_tbl + 7) / 8)] << 16) & 0xff0000)	|
		  ((det->freq_tbl[5 * ((det->num_freq_tbl + 7) / 8)] << 8) & 0xff00)	|
		  ((det->freq_tbl[4 * ((det->num_freq_tbl + 7) / 8)]) & 0xff));

	eem_write(EEM_LIMITVALS,
		  ((det->VMAX << 24) & 0xff000000)	|
		  ((det->VMIN << 16) & 0xff0000)	|
		  ((det->DTHI << 8) & 0xff00)		|
		  (det->DTLO & 0xff));
	/* eem_write(EEM_LIMITVALS, 0xFF0001FE); */
	eem_write(EEM_VBOOT, (((det->VBOOT) & 0xff)));
	eem_write(EEM_DETWINDOW, (((det->DETWINDOW) & 0xffff)));
	eem_write(EEMCONFIG, (((det->DETMAX) & 0xffff)));

	/* eem ctrl choose thermal sensors */
	eem_write(EEM_CTL0, det->EEMCTL0);
	#if 0
	eem_debug("[SET_PHASE]big: EEM_FREQPCT30 = 0x%08X, EEM_FREQPCT74 = 0X%08X\n",
				eem_read(EEM_FREQPCT30), eem_read(EEM_FREQPCT74));
	eem_debug("[SET_PHASE]big: EEM_VBOOT = 0x%08X, EEMCORESEL = 0x%08X\n",
				eem_read(EEM_VBOOT), eem_read(EEMCORESEL));
	#endif

	/* clear all pending EEM interrupt & config EEMINTEN */
	eem_write(EEMINTSTS, 0xffffffff);
	/* eem_debug("%s set phase = %d\n", ((char *)(det->name) + 8), phase); */
	switch (phase) {
	case EEM_PHASE_INIT01:
		eem_write(EEMINTEN, 0x00005f01);
		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000001);
		udelay(250); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_INIT02:
		eem_write(EEMINTEN, 0x00005f01);
		eem_write(EEM_INIT2VALS,
			  ((det->AGEVOFFSETIN << 16) & 0xffff0000) |
			  ((eem_checkEfuse == 0) ? 0 : det->DCVOFFSETIN & 0xffff));
		/* enable EEM INIT measurement */
		eem_write(EEMEN, 0x00000005);
		udelay(200); /* all banks' phase cannot be set without delay */
		break;

	case EEM_PHASE_MON:
		eem_write(EEMINTEN, 0x00FF0000);
		/* enable EEM monitor mode */
		eem_write(EEMEN, 0x00000002);
		break;

	default:
		WARN_ON(1); /*BUG()*/
		break;
	}
	/* mt_ptp_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_HELP);
}

int base_ops_get_temp(struct eem_det *det)
{
#if defined(CONFIG_THERMAL) && !defined(EARLY_PORTING_THERMAL)
	enum thermal_bank_name ts_bank;

	ts_bank = det_to_id(det);
	return tscpu_get_temp_by_bank(ts_bank);
#else
	return 0;
#endif
}

int base_ops_get_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

int base_ops_set_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

void base_ops_restore_default_volt(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("[%s] default func\n", __func__);
	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_freq_table(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);

	det->freq_tbl[0] = 100;
	det->num_freq_tbl = 1;

	FUNC_EXIT(FUNC_LV_HELP);
}

void base_ops_get_orig_volt_table(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

static long long eem_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

static void mt_ptp_lock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	/* FIXME: lock with MD32 */
	/* get_md32_semaphore(SEMAPHORE_PTP); */
	spin_lock_irqsave(&eem_spinlock, *flags);
	eem_pTime_us = eem_get_current_time_us();
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_lock);

static void mt_ptp_unlock(unsigned long *flags)
{
	/* FUNC_ENTER(FUNC_LV_HELP); */
	eem_cTime_us = eem_get_current_time_us();
	EEM_IS_TOO_LONG();
	spin_unlock_irqrestore(&eem_spinlock, *flags);
	/* FIXME: lock with MD32 */
	/* release_md32_semaphore(SEMAPHORE_PTP); */
	/* FUNC_EXIT(FUNC_LV_HELP); */
}
EXPORT_SYMBOL(mt_ptp_unlock);

#if 0
int mt_ptp_idle_can_enter(void)
{
	struct eem_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_ctrl(ctrl) {
		if (atomic_read(&ctrl->in_init)) {
			FUNC_EXIT(FUNC_LV_HELP);
			return 0;
		}
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 1;
}
EXPORT_SYMBOL(mt_ptp_idle_can_enter);
#endif

static enum hrtimer_restart eem_log_timer_func(struct hrtimer *timer)
{
	struct eem_det *det;

	FUNC_ENTER(FUNC_LV_HELP);

	for_each_det(det) {
		if (det->ctrl_id == EEM_CTRL_SOC)
			continue;

		/* get rid of redundent banks */
		if (det->features == 0)
			continue;

		eem_debug("Timer Bk=%d (%d)(%d, %d, %d, %d, %d, %d, %d, %d)(0x%x)\n",
			det->ctrl_id,
			det->ops->get_temp(det),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[0]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[1]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[2]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[3]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[4]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[5]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[6]),
			det->ops->pmic_2_volt(det, det->volt_tbl_pmic[7]),
			det->t250);
		#if ITurbo
		eem_debug("sts(%d), It(%d)\n",
			det->ops->get_status(det),
			ITurboRun
			);
		#endif

		#if 0
		det->freq_tbl[0],
		det->freq_tbl[1],
		det->freq_tbl[2],
		det->freq_tbl[3],
		det->freq_tbl[4],
		det->freq_tbl[5],
		det->freq_tbl[6],
		det->freq_tbl[7],
		det->dcvalues[3],
		det->freqpct30[3],
		det->eem_26c[3],
		det->vop30[3]
		#endif
	}

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	FUNC_EXIT(FUNC_LV_HELP);

	return HRTIMER_RESTART;
}

static int eem_volt_thread_handler(void *data)
{
	struct eem_ctrl *ctrl = (struct eem_ctrl *)data;
	struct eem_det *det = id_to_eem_det(ctrl->det_id);

	FUNC_ENTER(FUNC_LV_HELP);
	do {
		wait_event_interruptible(ctrl->wq, ctrl->volt_update);

		if ((ctrl->volt_update & EEM_VOLT_UPDATE) && det->ops->set_volt) {

		#if (defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING))
			/* update set volt status for this bank */
			int temp = -1;

			switch (det->ctrl_id) {
			case EEM_CTRL_2L:
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_2_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_2_LITTLE_IS_SET_VOLT;
				break;

			case EEM_CTRL_L:
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_LITTLE_IS_SET_VOLT));
				temp = EEM_CPU_LITTLE_IS_SET_VOLT;
				break;

			case EEM_CTRL_GPU:
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
					(1 << EEM_GPU_IS_SET_VOLT));
				temp = EEM_GPU_IS_SET_VOLT;
				break;

			case EEM_CTRL_CCI:
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_CCI_IS_SET_VOLT));
				temp = EEM_CPU_CCI_IS_SET_VOLT;
				break;

			#if EEM_BANK_BIG
			case EEM_CTRL_BIG:
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() |
					(1 << EEM_CPU_BIG_IS_SET_VOLT));
				temp = EEM_CPU_BIG_IS_SET_VOLT;
				break;
			#endif

			default:
				break;
			}
		#endif

		det->ops->set_volt(det);
		#if 0
		eem_debug("B=%d,T=%d,DC=%x,V30=%x,F30=%x,sts=%x,250=%x\n",
		det->ctrl_id,
		det->ops->get_temp(det),
		det->dcvalues[EEM_PHASE_INIT01],
		det->vop30[EEM_PHASE_MON],
		det->freqpct30[EEM_PHASE_MON],
		det->ops->get_status(det),
		det->t250);
		#endif

		/* clear out set volt status for this bank */
		#if (defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING))
			if (temp >= 0)
				aee_rr_rec_ptp_status(aee_rr_curr_ptp_status() & ~(1 << temp));
		#endif
		}
		if ((ctrl->volt_update & EEM_VOLT_RESTORE) && det->ops->restore_default_volt)
			det->ops->restore_default_volt(det);

		ctrl->volt_update = EEM_VOLT_NONE;

	} while (!kthread_should_stop());
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static void inherit_base_det(struct eem_det *det)
{
	/*
	 * Inherit ops from eem_det_base_ops if ops in det is NULL
	 */
	FUNC_ENTER(FUNC_LV_HELP);

	#define INIT_OP(ops, func)					\
		do {							\
			if (ops->func == NULL)				\
				ops->func = eem_det_base_ops.func;	\
		} while (0)

	INIT_OP(det->ops, disable);
	INIT_OP(det->ops, disable_locked);
	INIT_OP(det->ops, switch_bank);
	INIT_OP(det->ops, init01);
	INIT_OP(det->ops, init02);
	INIT_OP(det->ops, mon_mode);
	INIT_OP(det->ops, get_status);
	INIT_OP(det->ops, dump_status);
	INIT_OP(det->ops, set_phase);
	INIT_OP(det->ops, get_temp);
	INIT_OP(det->ops, get_volt);
	INIT_OP(det->ops, set_volt);
	INIT_OP(det->ops, restore_default_volt);
	INIT_OP(det->ops, get_freq_table);
	INIT_OP(det->ops, volt_2_pmic);
	INIT_OP(det->ops, volt_2_eem);
	INIT_OP(det->ops, pmic_2_volt);
	INIT_OP(det->ops, eem_2_pmic);

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_init_ctrl(struct eem_ctrl *ctrl)
{
	FUNC_ENTER(FUNC_LV_HELP);
	/* init_completion(&ctrl->init_done); */
	/* atomic_set(&ctrl->in_init, 0); */
	/* if(HAS_FEATURE(id_to_eem_det(ctrl->det_id), FEA_MON)) { */
	/* TODO: FIXME, why doesn't work <-XXX */
	if (1) {
		init_waitqueue_head(&ctrl->wq);
		ctrl->thread = kthread_run(eem_volt_thread_handler, ctrl, ctrl->name);

		if (IS_ERR(ctrl->thread))
			eem_debug("Create %s thread failed: %ld\n", ctrl->name, PTR_ERR(ctrl->thread));
	}
	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_init_det(struct eem_det *det, struct eem_devinfo *devinfo)
{
	enum eem_det_id det_id = det_to_id(det);
	/* unsigned int binLevel; */

	FUNC_ENTER(FUNC_LV_HELP);
	eem_debug("eem_init_det: det=%s, id=%d\n", ((char *)(det->name) + 8), det_id);

	inherit_base_det(det);

	#if 0
	if (det->ops->get_volt != NULL) {
		det->VBOOT = det->ops->get_volt(det);
		eem_debug("@%s, %s-VBOOT = %d\n",
			__func__, ((char *)(det->name) + 8), det->VBOOT);
	}
	#endif

	switch (det_id) {
	case EEM_DET_2L:
		det->MDES	= devinfo->CPU_2L_MDES;
		det->BDES	= devinfo->CPU_2L_BDES;
		det->DCMDET	= devinfo->CPU_2L_DCMDET;
		det->DCBDET	= devinfo->CPU_2L_DCBDET;
		det->EEMINITEN	= devinfo->CPU_2L_INITEN;
		det->EEMMONEN	= devinfo->CPU_2L_MONEN;
		/* det->AGEDELTA	= devinfo->CPU_2L_AGEDELTA; */
		det->MTDES	= devinfo->CPU_2L_MTDES;
		#if 0
		det->recordRef	= recordRef;
		int i;

		for (int i = 0; i < NR_FREQ; i++)
			eem_debug("@(Record)%s----->(%s), = 0x%08x\n",
						__func__,
						det->name,
						*(det->recordRef + (i * 2)));
		#endif
		break;

	case EEM_DET_L:
		det->MDES	= devinfo->CPU_L_MDES;
		det->BDES	= devinfo->CPU_L_BDES;
		det->DCMDET	= devinfo->CPU_L_DCMDET;
		det->DCBDET	= devinfo->CPU_L_DCBDET;
		det->EEMINITEN	= devinfo->CPU_L_INITEN;
		det->EEMMONEN	= devinfo->CPU_L_MONEN;
		/* det->AGEDELTA	= devinfo->CPU_L_AGEDELTA; */
		det->MTDES	= devinfo->CPU_L_MTDES;
		#if 0
		det->recordRef	= recordRef + 36;
		int i;

		for (int i = 0; i < NR_FREQ; i++)
			eem_debug("@(Record)%s----->(%s), = 0x%08x\n",
						__func__,
						det->name,
						*(det->recordRef + (i * 2)));
		#endif
		break;

	case EEM_DET_GPU:
		det->MDES	= devinfo->GPU_MDES;
		det->BDES	= devinfo->GPU_BDES;
		det->DCMDET	= devinfo->GPU_DCMDET;
		det->DCBDET	= devinfo->GPU_DCBDET;
		det->EEMINITEN	= devinfo->GPU_INITEN;
		det->EEMMONEN	= devinfo->GPU_MONEN;
		/* det->AGEDELTA	= devinfo->GPU_AGEDELTA; */
		det->MTDES	= devinfo->GPU_MTDES;
		break;

	case EEM_DET_CCI:
		det->MDES	= devinfo->CCI_MDES;
		det->BDES	= devinfo->CCI_BDES;
		det->DCMDET	= devinfo->CCI_DCMDET;
		det->DCBDET	= devinfo->CCI_DCBDET;
		det->EEMINITEN	= devinfo->CCI_INITEN;
		det->EEMMONEN	= devinfo->CCI_MONEN;
		/* det->AGEDELTA	= devinfo->CCI_AGEDELTA; */
		det->MTDES	= devinfo->CCI_MTDES;
		#if 0
		det->recordRef	= recordRef + 72;
		int i;

		for (int i = 0; i < NR_FREQ; i++)
			eem_debug("@(Record)%s----->(%s), = 0x%08x\n",
						__func__,
						det->name,
						*(det->recordRef + (i * 2)));
		#endif
		break;

	 /* for DVT SOC input values are the same as CCI*/
#if EEM_BANK_SOC
	case EEM_DET_SOC:
		det->MDES	= devinfo->SOC_MDES;
		det->BDES	= devinfo->SOC_BDES;
		det->DCMDET	= devinfo->SOC_DCMDET;
		det->DCBDET	= devinfo->SOC_DCBDET;
		det->EEMINITEN	= devinfo->SOC_INITEN;
		det->EEMMONEN	= devinfo->SOC_MONEN;
		/* det->AGEDELTA	= devinfo->SOC_AGEDELTA; */
		det->MTDES	= devinfo->SOC_MTDES;
		break;
#endif

#if EEM_BANK_BIG
	case EEM_DET_BIG:
		det->MDES	= devinfo->BIG_MDES;
		det->BDES	= devinfo->BIG_BDES;
		det->DCMDET	= devinfo->BIG_DCMDET;
		det->DCBDET	= devinfo->BIG_DCBDET;
		det->EEMINITEN	= devinfo->BIG_INITEN;
		det->EEMMONEN	= devinfo->BIG_MONEN;
		/* det->AGEDELTA	= devinfo->BIG_AGEDELTA; */
		det->MTDES	= devinfo->BIG_MTDES;
		#if 0
		if (ateVer > 5)
			det->VCO = VCO_VAL_AFTER_5_BIG;
		else
			det->VCO = VCO_VAL_BIG;

		det->recordRef	= recordRef + 108;
		/*
		*int i;
		*	for (int i = 0; i < NR_FREQ; i++)
		*		eem_debug("@(SRAM)%s----->(%s), = 0x%08x\n",
		*				__func__,
		*				det->name,
		*				*(det->recordRef + (i * 2)));
		*/
		#endif
		break;
#endif

	default:
		eem_debug("[%s]: Unknown det_id %d\n", __func__, det_id);
		break;
	}

	#if DVT
		det->VBOOT = 0x48;
		det->VMAX = 0xFE;
		det->VMIN = 0x10;
		det->VCO = 0x10;
		det->DVTFIXED = 0x06;
	#endif

	memset(det->volt_tbl, 0, sizeof(det->volt_tbl));
	memset(det->volt_tbl_pmic, 0, sizeof(det->volt_tbl_pmic));
	memset(det->freq_tbl, 0, sizeof(det->freq_tbl));
	memset(record_tbl_locked, 0, sizeof(record_tbl_locked));

	/* get DVFS frequency table */
	if (det->ops->get_freq_table)
		det->ops->get_freq_table(det);

	FUNC_EXIT(FUNC_LV_HELP);
}

#if UPDATE_TO_UPOWER
static enum upower_bank transfer_ptp_to_upower_bank(unsigned int det_id)
{
	enum upower_bank bank;

	switch (det_id) {
	case EEM_DET_2L:
		bank = UPOWER_BANK_LL;
		break;
	case EEM_DET_L:
		bank = UPOWER_BANK_L;
		break;
	case EEM_DET_CCI:
		bank = UPOWER_BANK_CCI;
		break;
	#if EEM_BANK_BIG
	case EEM_DET_BIG:
		bank = UPOWER_BANK_B;
		break;
	#endif
	default:
		bank = NR_UPOWER_BANK;
		break;
	}
	return bank;
}

static void eem_update_init2_volt_to_upower(struct eem_det *det, unsigned int *pmic_volt)
{
	unsigned int volt_tbl[NR_FREQ_CPU];
	enum upower_bank bank;
	int i;

	for (i = 0; i < det->num_freq_tbl; i++)
		volt_tbl[i] = det->ops->pmic_2_volt(det, pmic_volt[i]);

	bank = transfer_ptp_to_upower_bank(det_to_id(det));
	if (bank < NR_UPOWER_BANK) {
		upower_update_volt_by_eem(bank, volt_tbl, det->num_freq_tbl);
		/* eem_debug("update init2 volt to upower (eem bank %ld upower bank %d)\n", det_to_id(det), bank); */
	}
}
#endif

static void eem_set_eem_volt(struct eem_det *det)
{
#if SET_PMIC_VOLT
	unsigned i;
	int low_temp_offset = 0;
	struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);
	#if ITurbo
	ITurboRunSet = 0;
	#endif
	FUNC_ENTER(FUNC_LV_HELP);

	det->temp = det->ops->get_temp(det);

	#if UPDATE_TO_UPOWER
	upower_update_degree_by_eem(transfer_ptp_to_upower_bank(det_to_id(det)), det->temp/1000);
	#endif

	/* eem_debug("T = %d, valid = %d\n", det->temp, tscpu_is_temp_valid()); */
	/* 6250 * 10uV = 62.5mv */
	if (det->temp <= INVERT_TEMP_VAL || !tscpu_is_temp_valid())
		det->isTempInv = 1;
	else if ((det->isTempInv) && (det->temp > OVER_INV_TEM_VAL))
		det->isTempInv = 0;

	if (det->isTempInv) {
		memcpy(det->volt_tbl, det->volt_tbl_init2, sizeof(det->volt_tbl));
		low_temp_offset = det->low_temp_off;
	}

	ctrl->volt_update |= EEM_VOLT_UPDATE;

	#if ITurbo
	if (det->temp <= INVERT_TEMP_VAL) {
		if ((det->ctrl_id == EEM_CTRL_L) && (ITurboRun == 1))
			ITurboRunSet = 0;
	} else {
		if ((det->ctrl_id == EEM_CTRL_L) && (ITurboRun == 1))
			ITurboRunSet = 0;
	}
	#endif
	/* for debugging */
	/* eem_debug("volt_offset, low_temp_offset= %d, %d\n", det->volt_offset, low_temp_offset); */
	/* eem_debug("pi_offset = %d\n", det->pi_offset);*/
	/* eem_debug("det->vmin = %d\n", det->VMIN); */
	/* eem_debug("det->vmax = %d\n", det->VMAX); */

	/*
	*eem_debug("Temp=(%d) ITR=(%d), ITRS=(%d)\n", det->temp, ITurboRun, ITurboRunSet);
	*eem_debug("ctrl->volt_update |= EEM_VOLT_UPDATE\n");
	*/

	/* scale of det->volt_offset must equal 10uV */
	/* if has record table, min with record table of each cpu */
	for (i = 0; i < det->num_freq_tbl; i++) {
		switch (det->ctrl_id) {
		case EEM_CTRL_2L:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det,
					(det->volt_tbl[i] + det->volt_offset + low_temp_offset)) + det->pi_offset,
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;

		case EEM_CTRL_L:
			#if ITurbo
			if (ITurboRunSet == 1) {
				ITurbo_offset[i] = det->ops->volt_2_eem(det, MAX_ITURBO_OFFSET + det->eem_v_base) *
						(det->volt_tbl[i] - det->volt_tbl[NR_FREQ-1]) /
						(det->volt_tbl[0] - det->volt_tbl[NR_FREQ-1]);
			} else
				ITurbo_offset[i] = 0;
			#endif

			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det,
					(det->volt_tbl[i] + det->volt_offset + low_temp_offset
					#if ITurbo
					- ITurbo_offset[i]
					#endif
				))+det->pi_offset,
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);

			#if 0
			if (eem_log_en)
				eem_debug("L->hw_v[%d]=0x%X, V(%d)L(%d)I(%d)P(%d) volt_tbl_pmic[%d]=0x%X (%d)\n",
					i, det->volt_tbl[i],
					det->volt_offset, low_temp_offset, ITurbo_offset[i], det->pi_offset,
					i, det->volt_tbl_pmic[i], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
			#endif
			break;

		case EEM_CTRL_GPU:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det, (det->volt_tbl[i] + det->volt_offset + low_temp_offset)),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;

		case EEM_CTRL_CCI:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det, (det->volt_tbl[i] + det->volt_offset + low_temp_offset)),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;

		#if EEM_BANK_SOC
		case EEM_CTRL_SOC:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det, (det->volt_tbl[i] + det->volt_offset + low_temp_offset)),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;
		#endif

		#if EEM_BANK_BIG
		case EEM_CTRL_BIG:
			det->volt_tbl_pmic[i] = min(
			(unsigned int)(clamp(
				det->ops->eem_2_pmic(det, (det->volt_tbl[i] + det->volt_offset +
									low_temp_offset + det->pi_offset)),
				det->ops->eem_2_pmic(det, det->VMIN),
				det->ops->eem_2_pmic(det, det->VMAX))),
				det->volt_tbl_orig[i]);
			break;
		#endif

		default:
			break;
		}
		#if 0
		eem_debug("[%s].volt_tbl[%d] = 0x%X ----- Ori[0x%x] volt_tbl_pmic[%d] = 0x%X (%d)\n",
			det->name,
			i, det->volt_tbl[i], det->volt_tbl_orig[i],
			i, det->volt_tbl_pmic[i], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
		#endif
	}

	#if UPDATE_TO_UPOWER
	/* only when set_volt_to_upower == 0, the volt will be apply to upower */
	if (det->set_volt_to_upower == 0) {
		eem_update_init2_volt_to_upower(det, det->volt_tbl_pmic);
		det->set_volt_to_upower = 1;
	}
	#endif

	if ((0 == (det->disabled % 2)) && (0 == (det->disabled & BY_PROCFS_INIT2)))
		wake_up_interruptible(&ctrl->wq);
	else
		eem_debug("Disabled by [%d]\n", det->disabled);
#endif

	FUNC_EXIT(FUNC_LV_HELP);
}

static void eem_restore_eem_volt(struct eem_det *det)
{
	#if SET_PMIC_VOLT
		struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);

		ctrl->volt_update |= EEM_VOLT_RESTORE;
			wake_up_interruptible(&ctrl->wq);
	#endif

	FUNC_ENTER(FUNC_LV_HELP);
	FUNC_EXIT(FUNC_LV_HELP);
}

#if 0
static void mt_eem_reg_dump_locked(void)
{
#ifndef CONFIG_ARM64
	unsigned int addr;

	for (addr = (unsigned int)EEM_DESCHAR; addr <= (unsigned int)EEM_SMSTATE1; addr += 4)
		eem_isr_info("0x%08X = 0x%08X\n", addr, *(unsigned int *)addr);

	addr = (unsigned int)EEMCORESEL;
	eem_isr_info("0x%08X = 0x%08X\n", addr, *(unsigned int *)addr);
#else
	unsigned long addr;

	for (addr = (unsigned long)EEM_DESCHAR; addr <= (unsigned long)EEM_SMSTATE1; addr += 4)
		eem_isr_info("0x %lu = 0x %lu\n", addr, *(unsigned long *)addr);

	addr = (unsigned long)EEMCORESEL;
	eem_isr_info("0x %lu = 0x %lu\n", addr, *(unsigned long *)addr);
#endif
}
#endif

static inline void handle_init01_isr(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_isr_info("******************** mode = init1 %s-isr\n", ((char *)(det->name) + 8)); */

	det->dcvalues[EEM_PHASE_INIT01]		= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_INIT01]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_INIT01]		= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_INIT01]		= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_INIT01]	= eem_read(EEMEN);

	#if DUMP_DATA_TO_DE
	{
		unsigned int i;

		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
			det->reg_dump_data[i][EEM_PHASE_INIT01] = eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
			#if 0
			eem_isr_info("0x%lx = 0x%08x\n",
				(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
				det->reg_dump_data[i][EEM_PHASE_INIT01]
				);
			#endif
		}
	}
	#endif
	/*
	 * Read & store 16 bit values EEM_DCVALUES.DCVOFFSET and
	 * EEM_AGEVALUES.AGEVOFFSET for later use in INIT2 procedure
	 */
	det->DCVOFFSETIN = ~(eem_read(EEM_DCVALUES) & 0xffff) + 1; /* hw bug, workaround */
	/* check if DCVALUES is minus and set DCVOFFSETIN to zero */

	if (det->DCVOFFSETIN & 0x8000)
		det->DCVOFFSETIN = 0;

	det->AGEVOFFSETIN = eem_read(EEM_AGEVALUES) & 0xffff;

	/*
	 * Set EEMEN.EEMINITEN/EEMEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMINTSTS = 0x00000001
	 */
	eem_write(EEMEN, 0x0);
	eem_write(EEMINTSTS, 0x1);
	/* det->ops->init02(det); */

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static unsigned int interpolate(unsigned int y1, unsigned int y0,
	unsigned int x1, unsigned int x0, unsigned int ym)
{
	unsigned int ratio, result;

	if (x1 == x0) {
		result =  x1;
	} else {
		ratio = (((y1 - y0) * 100) + (x1 - x0 - 1)) / (x1 - x0);
		result =  (x1 - ((((y1 - ym) * 10000) + ratio - 1) / ratio) / 100);
		/*
		*eem_debug("y1(%d), y0(%d), x1(%d), x0(%d), ym(%d), ratio(%d), rtn(%d)\n",
		*	y1, y0, x1, x0, ym, ratio, result);
		*/
	}

	return result;
}

static void read_volt_from_VOP(struct eem_det *det)
{
	int temp, i, j;
	unsigned int step = NR_FREQ / 8;
	int ref_idx = ((det->num_freq_tbl + (step - 1)) / step) - 1;

	temp = eem_read(EEM_VOP30);
	/* eem_debug("read(EEM_VOP30) = 0x%08X\n", temp); */
	/* EEM_VOP30=>pmic value */
	det->volt_tbl[0] = (temp & 0xff);
	det->volt_tbl[1 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 8)  & 0xff;
	det->volt_tbl[2 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 16) & 0xff;
	det->volt_tbl[3 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 24) & 0xff;

	temp = eem_read(EEM_VOP74);
	/* eem_debug("read(EEM_VOP74) = 0x%08X\n", temp); */
	/* EEM_VOP74=>pmic value */
	det->volt_tbl[4 * ((det->num_freq_tbl + 7) / 8)] = (temp & 0xff);
	det->volt_tbl[5 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 8)  & 0xff;
	det->volt_tbl[6 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 16) & 0xff;
	det->volt_tbl[7 * ((det->num_freq_tbl + 7) / 8)] = (temp >> 24) & 0xff;

	if ((det->num_freq_tbl > 8) && (ref_idx > 0)) {
		for (i = 0; i <= ref_idx; i++) { /* i < 8 */
			for (j = 1; j < step; j++) {
				if (i < ref_idx) {
					det->volt_tbl[(i * step) + j] =
						interpolate(
							det->freq_tbl[(i * step)],
							det->freq_tbl[((1 + i) * step)],
							det->volt_tbl[(i * step)],
							det->volt_tbl[((1 + i) * step)],
							det->freq_tbl[(i * step) + j]
						);
				} else {
					det->volt_tbl[(i * step) + j] =
					clamp(
						interpolate(
							det->freq_tbl[((i - 1) * step)],
							det->freq_tbl[((i) * step)],
							det->volt_tbl[((i - 1) * step)],
							det->volt_tbl[((i) * step)],
							det->freq_tbl[(i * step) + j]
						),
						det->VMIN,
						det->VMAX
					);
				}
			}
		}
	} /* if (NR_FREQ > 8)*/
}

static inline void handle_init02_isr(struct eem_det *det)
{
	unsigned int i;
	/* struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id); */

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_debug("******************** mode = init2 %s-isr\n", ((char *)(det->name) + 8)); */

	det->dcvalues[EEM_PHASE_INIT02]		= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_INIT02]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_INIT02]		= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_INIT02]	= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_INIT02]	= eem_read(EEMEN);

	#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
		det->reg_dump_data[i][EEM_PHASE_INIT02] = eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
		#if 0
		eem_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
			det->reg_dump_data[i][EEM_PHASE_INIT02]
			);
		#endif
	}
	#endif

	read_volt_from_VOP(det);

	/* backup to volt_tbl_init2 */
	memcpy(det->volt_tbl_init2, det->volt_tbl, sizeof(det->volt_tbl_init2));

	for (i = 0; i < NR_FREQ; i++) {
		#if defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING) /* I-Chang */
		switch (det->ctrl_id) {
		case EEM_CTRL_2L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_2_little_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_2_little_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_little_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_little_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_GPU:
			if (i < 8) {
				aee_rr_rec_ptp_gpu_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_gpu_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_gpu_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_gpu_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_cci_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_cci_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		#if EEM_BANK_BIG
		case EEM_CTRL_BIG:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_big_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_big_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_big_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_big_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
		#endif

		default:
			break;
		}
		#endif
		#if 0
		eem_debug("init02_[%s].volt_tbl[%d] = 0x%X (%d)\n",
			det->name, i, det->volt_tbl[i], det->ops->pmic_2_volt(det, det->volt_tbl[i]));

		if (NR_FREQ > 8) {
			eem_debug("init02_[%s].volt_tbl[%d] = 0x%X (%d)\n",
			det->name, i+1, det->volt_tbl[i+1], det->ops->pmic_2_volt(det, det->volt_tbl[i+1]));
		}
		#endif
	}
	eem_set_eem_volt(det);

	/*
	 * Set EEMEN.EEMINITEN/EEMEN.EEMINIT2EN = 0x0 &
	 * Clear EEM INIT interrupt EEMINTSTS = 0x00000001
	 */
	eem_write(EEMEN, 0x0);
	eem_write(EEMINTSTS, 0x1);

	/* atomic_dec(&ctrl->in_init); */
	/* complete(&ctrl->init_done); */
	det->ops->mon_mode(det);
	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_init_err_isr(struct eem_det *det)
{
	FUNC_ENTER(FUNC_LV_LOCAL);
	eem_debug("====================================================\n");
	eem_debug("EEM init err: EEMEN(%p) = 0x%X, EEMINTSTS(%p) = 0x%X\n",
			 EEMEN, eem_read(EEMEN),
			 EEMINTSTS, eem_read(EEMINTSTS));
	eem_debug("EEM_SMSTATE0 (%p) = 0x%X\n",
			 EEM_SMSTATE0, eem_read(EEM_SMSTATE0));
	eem_debug("EEM_SMSTATE1 (%p) = 0x%X\n",
			 EEM_SMSTATE1, eem_read(EEM_SMSTATE1));
	eem_debug("====================================================\n");

#if 0
TODO: FIXME
	{
		struct eem_ctrl *ctrl = id_to_eem_ctrl(det->ctrl_id);

		atomic_dec(&ctrl->in_init);
		complete(&ctrl->init_done);
	}
TODO: FIXME
#endif

	aee_kernel_warning("mt_eem", "@%s():%d, get_volt(%s) = 0x%08X\n",
		__func__,
		__LINE__,
		det->name,
		det->VBOOT);
	det->ops->disable_locked(det, BY_INIT_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_mode_isr(struct eem_det *det)
{
	unsigned int i;
	#if defined(CONFIG_THERMAL) && !defined(EARLY_PORTING_THERMAL)
	unsigned long long temp_long;
	unsigned long long temp_cur = (unsigned long long)aee_rr_curr_ptp_temp();
	#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* eem_debug("******************** mode = mon %s-isr\n", ((char *)(det->name) + 8)); */

	#if 0 /* defined(CONFIG_THERMAL) && !defined(EARLY_PORTING_THERMAL) */
	eem_isr_info("B_temp=%d, CCI_temp=%d, G_temp=%d, LL_temp=%d, L_temp=%d\n",
		tscpu_get_temp_by_bank(THERMAL_BANK0),
		tscpu_get_temp_by_bank(THERMAL_BANK1),
		tscpu_get_temp_by_bank(THERMAL_BANK2),
		tscpu_get_temp_by_bank(THERMAL_BANK3),
		tscpu_get_temp_by_bank(THERMAL_BANK4)
		);
	#endif

	#if defined(CONFIG_EEM_AEE_RR_REC) && !defined(EARLY_PORTING_THERMAL)
	switch (det->ctrl_id) {
	case EEM_CTRL_2L:
		#ifdef CONFIG_THERMAL
		temp_long = (unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK3)/1000;
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long << (8 * EEM_CPU_2_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~((unsigned long long)0xFF << (8 * EEM_CPU_2_LITTLE_IS_SET_VOLT))));
		}
		#endif
		break;

	case EEM_CTRL_L:
		#ifdef CONFIG_THERMAL
		temp_long = (unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK4)/1000;
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long << (8 * EEM_CPU_LITTLE_IS_SET_VOLT) |
			(temp_cur & ~((unsigned long long)0xFF << (8 * EEM_CPU_LITTLE_IS_SET_VOLT))));
		}
		#endif
		break;

	case EEM_CTRL_GPU:
		#ifdef CONFIG_THERMAL
		temp_long = (unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK2)/1000;
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long << (8 * EEM_GPU_IS_SET_VOLT) |
			(temp_cur & ~((unsigned long long)0xFF << (8 * EEM_GPU_IS_SET_VOLT))));
		}
		#endif
		break;

	case EEM_CTRL_CCI:
		#ifdef CONFIG_THERMAL
		temp_long = (unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK1)/1000;
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long << (8 * EEM_CPU_CCI_IS_SET_VOLT)|
			(temp_cur & ~((unsigned long long)0xFF << (8 * EEM_CPU_CCI_IS_SET_VOLT))));
		}
		#endif
		break;

	#if EEM_BANK_BIG
	case EEM_CTRL_BIG:
		#ifdef CONFIG_THERMAL
		temp_long = (unsigned long long)tscpu_get_temp_by_bank(THERMAL_BANK0)/1000;
		if (temp_long != 0) {
			aee_rr_rec_ptp_temp(temp_long << (8 * EEM_CPU_BIG_IS_SET_VOLT) |
			(temp_cur & ~((unsigned long long)0xFF << (8 * EEM_CPU_BIG_IS_SET_VOLT))));
		}
		#endif
		break;
	#endif

	default:
		break;
	}
	#endif

	det->dcvalues[EEM_PHASE_MON]	= eem_read(EEM_DCVALUES);
	det->freqpct30[EEM_PHASE_MON]	= eem_read(EEM_FREQPCT30);
	det->eem_26c[EEM_PHASE_MON]	= eem_read(EEMINTEN + 0x10);
	det->vop30[EEM_PHASE_MON]	= eem_read(EEM_VOP30);
	det->eem_eemEn[EEM_PHASE_MON]	= eem_read(EEMEN);

	#if DUMP_DATA_TO_DE
	for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
		det->reg_dump_data[i][EEM_PHASE_MON] = eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
		#if 0
		eem_isr_info("0x%lx = 0x%08x\n",
			(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
			det->reg_dump_data[i][EEM_PHASE_MON]
			);
		#endif
	}
	#endif

	/* check if thermal sensor init completed? */
	det->t250 = eem_read(TEMP);
	/* eem_debug("ptp TEMP ----- (0x%08X) degree !!\n", det->t250); */

	/* 0x64 mappint to 100 + 25 = 125C,
	*   0xB2 mapping to 178 - 128 = 50, -50 + 25 = -25C
	*/
	if (((det->t250 & 0xff)  > 0x64) && ((det->t250  & 0xff) < 0xB2)) {
		eem_debug("Temperature > 125C or < -25C !!\n");
		goto out;
	}

	read_volt_from_VOP(det);

	for (i = 0; i < NR_FREQ; i++) {
		#if defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING) /* I-Chang */
		switch (det->ctrl_id) {

		case EEM_CTRL_2L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_2_little_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_2_little_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_2_little_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_2_little_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_L:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_little_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_little_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_little_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_little_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_GPU:
			if (i < 8) {
				aee_rr_rec_ptp_gpu_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_gpu_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_gpu_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_gpu_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		case EEM_CTRL_CCI:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_cci_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_cci_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_cci_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_cci_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;

		#if EEM_BANK_BIG
		case EEM_CTRL_BIG:
			if (i < 8) {
				aee_rr_rec_ptp_cpu_big_volt(
					((unsigned long long)(det->volt_tbl[i]) << (8 * i)) |
					(aee_rr_curr_ptp_cpu_big_volt() & ~
						((unsigned long long)(0xFF) << (8 * i))
					)
				);
			} else {
				aee_rr_rec_ptp_cpu_big_volt_1(
					((unsigned long long)(det->volt_tbl[i]) << (8 * (i - 8))) |
					(aee_rr_curr_ptp_cpu_big_volt_1() & ~
						((unsigned long long)(0xFF) << (8 * (i - 8)))
					)
				);
			}
			break;
		#endif

		default:
			break;
		}
		#endif
		/*
		*eem_debug("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		*	det->name, i, det->volt_tbl[i], det->ops->pmic_2_volt(det, det->volt_tbl[i]));

		*if (NR_FREQ > 8) {
		*	eem_isr_info("mon_[%s].volt_tbl[%d] = 0x%X (%d)\n",
		*	det->name, i+1, det->volt_tbl[i+1], det->ops->pmic_2_volt(det, det->volt_tbl[i+1]));
		*}
		*/
	}
	/* eem_isr_info("ISR : EEM_TEMPSPARE1 = 0x%08X\n", eem_read(EEM_TEMPSPARE1)); */

	eem_set_eem_volt(det);

	#if ITurbo
	if (det->ctrl_id == EEM_CTRL_L)
		ctrl_ITurbo = (ctrl_ITurbo == 0) ? 0 : 2;
	#endif
out:
	/* Clear EEM INIT interrupt EEMINTSTS = 0x00ff0000 */
	eem_write(EEMINTSTS, 0x00ff0000);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void handle_mon_err_isr(struct eem_det *det)
{
	#if DUMP_DATA_TO_DE
		unsigned int i;
	#endif

	FUNC_ENTER(FUNC_LV_LOCAL);

	/* EEM Monitor mode error handler */
	eem_debug("====================================================\n");
	eem_debug("EEM mon err: EEMCORESEL(%p) = 0x%08X, EEM_THERMINTST(%p) = 0x%08X, EEMODINTST(%p) = 0x%08X",
			 EEMCORESEL, eem_read(EEMCORESEL),
			 EEM_THERMINTST, eem_read(EEM_THERMINTST),
			 EEMODINTST, eem_read(EEMODINTST));
	eem_debug(" EEMINTSTSRAW(%p) = 0x%08X, EEMINTEN(%p) = 0x%08X\n",
			 EEMINTSTSRAW, eem_read(EEMINTSTSRAW),
			 EEMINTEN, eem_read(EEMINTEN));
	eem_debug("====================================================\n");
	eem_debug("EEM mon err: EEMEN(%p) = 0x%08X, EEMINTSTS(%p) = 0x%08X\n",
			 EEMEN, eem_read(EEMEN),
			 EEMINTSTS, eem_read(EEMINTSTS));
	eem_debug("EEM_SMSTATE0 (%p) = 0x%08X\n",
			 EEM_SMSTATE0, eem_read(EEM_SMSTATE0));
	eem_debug("EEM_SMSTATE1 (%p) = 0x%08X\n",
			 EEM_SMSTATE1, eem_read(EEM_SMSTATE1));
	eem_debug("TEMP (%p) = 0x%08X\n",
			 TEMP, eem_read(TEMP));
	eem_debug("EEM_TEMPMSR0 (%p) = 0x%08X\n",
			 EEM_TEMPMSR0, eem_read(EEM_TEMPMSR0));
	eem_debug("EEM_TEMPMSR1 (%p) = 0x%08X\n",
			 EEM_TEMPMSR1, eem_read(EEM_TEMPMSR1));
	eem_debug("EEM_TEMPMSR2 (%p) = 0x%08X\n",
			 EEM_TEMPMSR2, eem_read(EEM_TEMPMSR2));
	eem_debug("EEM_TEMPMONCTL0 (%p) = 0x%08X\n",
			 EEM_TEMPMONCTL0, eem_read(EEM_TEMPMONCTL0));
	eem_debug("EEM_TEMPMSRCTL1 (%p) = 0x%08X\n",
			 EEM_TEMPMSRCTL1, eem_read(EEM_TEMPMSRCTL1));
	eem_debug("====================================================\n");

	#if DUMP_DATA_TO_DE
		for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++) {
			det->reg_dump_data[i][EEM_PHASE_MON] = eem_read(EEM_BASEADDR + reg_dump_addr_off[i]);
			eem_debug("0x%lx = 0x%08x\n",
				(unsigned long)EEM_BASEADDR + reg_dump_addr_off[i],
				det->reg_dump_data[i][EEM_PHASE_MON]
				);
		}
	#endif

	eem_debug("====================================================\n");
	eem_debug("EEM mon err: EEMCORESEL(%p) = 0x%08X, EEM_THERMINTST(%p) = 0x%08X, EEMODINTST(%p) = 0x%08X",
			 EEMCORESEL, eem_read(EEMCORESEL),
			 EEM_THERMINTST, eem_read(EEM_THERMINTST),
			 EEMODINTST, eem_read(EEMODINTST));
	eem_debug(" EEMINTSTSRAW(%p) = 0x%08X, EEMINTEN(%p) = 0x%08X\n",
			 EEMINTSTSRAW, eem_read(EEMINTSTSRAW),
			 EEMINTEN, eem_read(EEMINTEN));
	eem_debug("====================================================\n");

	aee_kernel_warning("mt_eem", "@%s():%d, get_volt(%s) = 0x%08X\n", __func__, __LINE__, det->name, det->VBOOT);
	det->ops->disable_locked(det, BY_MON_ERROR);

	FUNC_EXIT(FUNC_LV_LOCAL);
}

static inline void eem_isr_handler(struct eem_det *det)
{
	unsigned int eemintsts, eemen;

	FUNC_ENTER(FUNC_LV_LOCAL);

	eemintsts = eem_read(EEMINTSTS);
	eemen = eem_read(EEMEN);

	/* eem_debug("[eem_isr_handler]Bk_No = %d %s-isr, 0x%X, 0x%X\n",
	*	det->ctrl_id, ((char *)(det->name) + 8), eemintsts, eemen);
	*/

	if (eemintsts == 0x1) { /* EEM init1 or init2 */
		if ((eemen & 0x7) == 0x1) /* EEM init1 */
			handle_init01_isr(det);
		else if ((eemen & 0x7) == 0x5) /* EEM init2 */
			handle_init02_isr(det);
		else {
			/* error : init1 or init2 */
			handle_init_err_isr(det);
		}
	} else if ((eemintsts & 0x00ff0000) != 0x0)
		handle_mon_mode_isr(det);
	else { /* EEM error handler */
		/* init 1  || init 2 error handler */
		if (((eemen & 0x7) == 0x1) || ((eemen & 0x7) == 0x5))
			handle_init_err_isr(det);
		else /* EEM Monitor mode error handler */
			handle_mon_err_isr(det);
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}
static irqreturn_t eem_isr(int irq, void *dev_id)
{
	unsigned long flags;
	struct eem_det *det = NULL;
	int i;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* mt_ptp_lock(&flags); */

	#if 0
	if (!(BIT(EEM_CTRL_VCORE) & eem_read(EEMODINTST))) {
		switch (eem_read_field(PERI_VCORE_PTPOD_CON0, VCORE_PTPODSEL)) {
		case SEL_VCORE_AO:
			det = &eem_detectors[PTP_DET_VCORE_AO];
			break;

		case SEL_VCORE_PDN:
			det = &eem_detectors[PTP_DET_VCORE_PDN];
			break;
		}

		if (likely(det)) {
			det->ops->switch_bank(det, NR_EEM_PHASE);
			eem_isr_handler(det);
		}
	}
	#endif

	for (i = 0; i < NR_EEM_CTRL; i++) {

		#if !(EEM_BANK_SOC) /* DVT will test EEM_CTRL_SOC */
		if (i == EEM_CTRL_SOC)
			continue;
		#endif

		mt_ptp_lock(&flags);
		/* TODO: FIXME, it is better to link i @ struct eem_det */
		if ((BIT(i) & eem_read(EEMODINTST))) {
			mt_ptp_unlock(&flags);
			continue;
		}

		det = &eem_detectors[i];

		det->ops->switch_bank(det, NR_EEM_PHASE);

		/*mt_eem_reg_dump_locked(); */

		eem_isr_handler(det);
		mt_ptp_unlock(&flags);
	}

	/* mt_ptp_unlock(&flags); */

	FUNC_EXIT(FUNC_LV_MODULE);
	return IRQ_HANDLED;
}
#if ITurbo
#define ITURBO_CPU_NUM 1
static int __cpuinit _mt_eem_cpu_CB(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned long flags;
	unsigned int cpu = (unsigned long)hcpu;
	unsigned int online_cpus = num_online_cpus();
	struct device *dev;
	struct eem_det *det;
	enum mt_eem_cpu_id cluster_id;

	/* CPU mask - Get on-line cpus per-cluster */
	struct cpumask eem_cpumask;
	struct cpumask cpu_online_cpumask;
	unsigned int cpus, big_cpus;

	if (ctrl_ITurbo < 2) {
		eem_debug("Default I-Turbo off (%d) !!", ctrl_ITurbo);
		return NOTIFY_OK;
	}
	eem_debug("I-Turbo start to run (%d) !!", ctrl_ITurbo);

	/* Current active CPU is belong which cluster */
	cluster_id = arch_get_cluster_id(cpu);

	/* How many active CPU in this cluster, present by bit mask
	*	ex:	B	L	LL
	*		00	1111	0000
	*/
	arch_get_cluster_cpus(&eem_cpumask, cluster_id);

	/* How many active CPU online in this cluster, present by number */
	cpumask_and(&cpu_online_cpumask, &eem_cpumask, cpu_online_mask);
	cpus = cpumask_weight(&cpu_online_cpumask);

	if (eem_log_en)
		eem_debug("@%s():%d, cpu = %d, act = %lu, on_cpus = %d, clst = %d, clst_cpu = %d\n"
		, __func__, __LINE__, cpu, action, online_cpus, cluster_id, cpus);

	dev = get_cpu_device(cpu);

	if (dev) {
		det = id_to_eem_det(EEM_DET_BIG);
		arch_get_cluster_cpus(&eem_cpumask, MT_EEM_CPU_B);
		cpumask_and(&cpu_online_cpumask, &eem_cpumask, cpu_online_mask);
		big_cpus = cpumask_weight(&cpu_online_cpumask);

		switch (action) {
		case CPU_POST_DEAD:
			if ((ITurboRun == 0) && (big_cpus == ITURBO_CPU_NUM) && (cluster_id == MT_EEM_CPU_B)) {
				if (eem_log_en)
					eem_debug("Turbo(1) DEAD (%d) BIG_cc(%d)\n", online_cpus, big_cpus);
				mt_ptp_lock(&flags);
				ITurboRun = 1;
				/* Revise BIG private table */
				/* [25:21]=dcmdiv, [20:12]=DDS, [11:7]=clkdiv, [6:4]=postdiv, [3:0]=CFindex */
#if 0 /* no record table */
				det->recordRef[0] =
					((*(recordTbl + (48 * 8) + 0) & 0x1F) << 21) |
					((*(recordTbl + (48 * 8) + 1) & 0x1FF) << 12) |
					((*(recordTbl + (48 * 8) + 2) & 0x1F) << 7) |
					((*(recordTbl + (48 * 8) + 3) & 0x7) << 4) |
					(*(recordTbl + (48 * 8) + 4) & 0xF);
#endif
				eem_set_eem_volt(det);
				mt_ptp_unlock(&flags);
			} else {
				if (eem_log_en)
					eem_debug("Turbo(%d)ed !! DEAD (%d), BIG_cc(%d)\n",
						ITurboRun, online_cpus, big_cpus);
			}
		break;

		case CPU_DOWN_PREPARE:
			if ((ITurboRun == 1) && (big_cpus == ITURBO_CPU_NUM) && (cluster_id == MT_EEM_CPU_B)) {
				if (eem_log_en)
					eem_debug("Turbo(0) DP (%d) BIG_cc(%d)\n", online_cpus, big_cpus);
				mt_ptp_lock(&flags);
				ITurboRun = 0;
				/* Restore BIG private table */
#if 0 /* no record table */
				det->recordRef[0] =
					((*(recordTbl + (48 * 8) + 0) & 0x1F) << 21) |
					((*(recordTbl + (48 * 8) + 1) & 0x1FF) << 12) |
					((*(recordTbl + (48 * 8) + 2) & 0x1F) << 7) |
					((*(recordTbl + (48 * 8) + 3) & 0x7) << 4) |
					(*(recordTbl + (48 * 8) + 4) & 0xF);
#endif
				eem_set_eem_volt(det);
				mt_ptp_unlock(&flags);
			} else {
				if (eem_log_en)
					eem_debug("Turbo(%d)ed !! DP (%d), BIG_cc(%d)\n",
						ITurboRun, online_cpus, big_cpus);
			}
		break;

		case CPU_UP_PREPARE:
			if ((ITurboRun == 0) && (cpu == 8) && (big_cpus == 0)) {
				eem_debug("Turbo(1) UP (%d), BIG_cc(%d)\n", online_cpus, big_cpus);
				mt_ptp_lock(&flags);
				ITurboRun = 1;
				/* Revise BIG private table */
#if 0 /* no record table*/
				det->recordRef[0] =
					((*(recordTbl + (48 * 8) + 0) & 0x1F) << 21) |
					((*(recordTbl + (48 * 8) + 1) & 0x1FF) << 12) |
					((*(recordTbl + (48 * 8) + 2) & 0x1F) << 7) |
					((*(recordTbl + (48 * 8) + 3) & 0x7) << 4) |
					(*(recordTbl + (48 * 8) + 4) & 0xF);
#endif
				eem_set_eem_volt(det);
				mt_ptp_unlock(&flags);
			} else if ((ITurboRun == 1) && (big_cpus == ITURBO_CPU_NUM) && (cluster_id == MT_EEM_CPU_B)) {
				if (eem_log_en)
					eem_debug("Turbo(0) UP (%d), BIG_cc(%d)\n", online_cpus, big_cpus);
				mt_ptp_lock(&flags);
				ITurboRun = 0;
				/* Restore BIG private table */
#if 0 /* no record table */
				det->recordRef[0] =
					((*(recordTbl + (48 * 8) + 0) & 0x1F) << 21) |
					((*(recordTbl + (48 * 8) + 1) & 0x1FF) << 12) |
					((*(recordTbl + (48 * 8) + 2) & 0x1F) << 7) |
					((*(recordTbl + (48 * 8) + 3) & 0x7) << 4) |
					(*(recordTbl + (48 * 8) + 4) & 0xF);
#endif
				eem_set_eem_volt(det);
				mt_ptp_unlock(&flags);
			} else {
				if (eem_log_en)
					eem_debug("Turbo(%d)ed !! UP (%d), BIG_cc(%d)\n",
						ITurboRun, online_cpus, big_cpus);
			}
		break;
		}
	}
	return NOTIFY_OK;
}
#endif /* if ITurbo */

void eem_init02(void)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		if (HAS_FEATURE(det, FEA_INIT02)) {
			unsigned long flag;

			mt_ptp_lock(&flag);
			det->ops->init02(det);
			mt_ptp_unlock(&flag);
		}
	}

	FUNC_EXIT(FUNC_LV_LOCAL);
}

/* get regulator reference */
#ifndef EARLY_PORTING_PMIC
static int eem_buck_get(struct platform_device *pdev)
{
	int ret = 0;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	/* proc11: big, cci, ll, l */
	eem_regulator_proc1 = regulator_get(&pdev->dev, "vproc11");
	if (!eem_regulator_proc1) {
		eem_debug("regulotor_proc11 error\n");
		return -EINVAL;
	}
	eem_regulator_gpu = regulator_get(&pdev->dev, "vgpu");
	if (!eem_regulator_gpu) {
		eem_debug("regulotor_gpu error\n");
		return -EINVAL;
	}

#if EEM_BANK_SOC
	eem_regulator_vcore = regulator_get(&pdev->dev, "vcore");
	if (!eem_regulator_vcore) {
		eem_debug("regulotor_vcore error\n");
		return -EINVAL;
	}
#endif
#endif

	return ret;
}
static void eem_buck_enable(void)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	int ret = 0;

	ret = regulator_is_enabled(eem_regulator_proc1);
	if (ret == 0) {
		ret = regulator_enable(eem_regulator_proc1);
		if (ret != 0)
			eem_debug("enable vproc1 failed\n");
		else {
			eem_vproc1_is_enabled_by_eem = 1;
			eem_debug("eem vproc1 enable success\n");
		}
	}

	ret = regulator_is_enabled(eem_regulator_gpu);
	if (ret == 0) {
		ret = regulator_enable(eem_regulator_gpu);
		if (ret != 0)
			eem_debug("enable vgpu failed\n");
		else {
			eem_vgpu_is_enabled_by_eem = 1;
			eem_debug("eem vgpu enable success\n");
		}
	}
#if EEM_BANK_SOC /* if DVT, set pwm mode for soc to run ptp soc det */
	ret = regulator_is_enabled(eem_regulator_vcore);
	if (ret == 0) {
		ret = regulator_enable(eem_regulator_vcore);
		if (ret != 0)
			eem_debug("enable vcore failed\n");
		else {
			eem_vcore_is_enabled_by_eem = 1;
			eem_debug("eem vcore enable success\n");
		}
	}

#endif /* if EEM_BANK_SOC */
#endif
}

static void eem_buck_disable(void)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	int ret = 0;

	if (eem_vproc1_is_enabled_by_eem) {
		ret = regulator_disable(eem_regulator_proc1);
		if (ret != 0)
			eem_debug("vproc1 disable failed\n");
		else
			eem_debug("eem vproc1 disabled success\n");
	}
	if (eem_vgpu_is_enabled_by_eem) {
		ret = regulator_disable(eem_regulator_gpu);
		if (ret != 0)
			eem_debug("vgpu disable failed\n");
		else
			eem_debug("eem vgpu disabled success\n");
	}

#if EEM_BANK_SOC /* if DVT, set non pwm mode for soc*/
	if (eem_vcore_is_enabled_by_eem) {
		ret = regulator_disable(eem_regulator_vcore);
		if (ret != 0)
			eem_debug("vcore disable failed\n");
		else
			eem_debug("eem vcore disabled success\n");
	}
#endif /* if EEM_BANK_SOC */
#endif
}

static void eem_buck_set_mode(unsigned int mode)
{
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
	int ret = 0;

	/* set pwm mode for each buck, 0:auto mode, 1:pwm mode */
	eem_debug("pmic set mode (%d)\n", mode);
	#if 0
		ret = regulator_set_mode(eem_regulator_proc1, mode);
		ret = regulator_set_mode(eem_regulator_gpu, mode);
	#else
		ret = vproc_pmic_set_mode(mode);
		ret = vgpu_pmic_set_mode(mode);
	#endif

	#if EEM_BANK_SOC /* if DVT, set pwm mode for soc to run ptp soc det */
		#if 0
		ret = regulator_set_mode(eem_regulator_vcore, mode);
		#else
		ret = vcore_pmic_set_mode(mode);
		#endif
	#endif
#endif
}
#endif /* ifndef EARLY_PORTING_PMIC */

void eem_init01(void)
{
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	unsigned int out = 0, timeout = 0;

	FUNC_ENTER(FUNC_LV_LOCAL);

	for_each_det_ctrl(det, ctrl) {
		unsigned long flag;

		if (HAS_FEATURE(det, FEA_INIT01)) {
			if (det->ops->get_volt != NULL) {
				det->real_vboot = det->ops->volt_2_eem(det, det->ops->get_volt(det));
				#if 1
				eem_debug("[%s] real_vboot=%d\n", ((char *)(det->name) + 8),
							det->ops->get_volt(det));
				#endif

				#if defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING) /* irene */
					aee_rr_rec_ptp_vboot(
						((unsigned long long)(det->real_vboot) << (8 * det->ctrl_id)) |
						(aee_rr_curr_ptp_vboot() & ~
							((unsigned long long)(0xFF) << (8 * det->ctrl_id))
						)
					);
				#endif
			}

			#if !EARLY_PORTING
				if (det->real_vboot != det->VBOOT) {
					eem_debug("@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
							__func__, __LINE__, det->name, det->real_vboot, det->VBOOT);
					/*
					*aee_kernel_warning("mt_eem",
					*	"@%s():%d, get_volt(%s) = 0x%08X, VBOOT = 0x%08X\n",
					*	__func__, __LINE__, det->name, det->real_vboot, det->VBOOT);
					*/
				}
				WARN_ON(det->real_vboot != det->VBOOT); /* BUG_ON(vboot != det->VBOOT);*/
			#endif

			mt_ptp_lock(&flag); /* <-XXX */
			det->ops->init01(det);
			mt_ptp_unlock(&flag); /* <-XXX */

			/*
			 * VCORE_AO and VCORE_PDN use the same controller.
			 * Wait until VCORE_AO init01 and init02 done
			 */

			/*
			*if (atomic_read(&ctrl->in_init)) {
			*	TODO: Use workqueue to avoid blocking
			*	wait_for_completion(&ctrl->init_done);
			*}
			*/
		}
	}

	#if EEM_BANK_BIG
	mt_eem_enable_big_cluster(0);
	#endif

	/* set non PWM mode*/
#ifndef EARLY_PORTING_PMIC
		eem_buck_set_mode(0);
		eem_buck_disable();
#endif /* ifndef EARLY_PORTING_PMIC */

	mt_eem_disable_mtcmos();
		#ifndef EARLY_PORTING_GPU
		mt_gpufreq_enable_by_ptpod(); /* enable gpu DVFS */
		#endif

		#ifndef EARLY_PORTING_PPM
		mt_ppm_ptpod_policy_deactivate();
		#endif

	/* call hotplug api to disable L bulk */

	/* This patch is waiting for whole bank finish the init01 then go
	 * next. Due to LL/L use same bulk PMIC, LL voltage table change
	 * will impact L to process init01 stage, because L require a
	 * stable 1V for init01.
	*/
	while (1) {
		for_each_det_ctrl(det, ctrl) {
			if ((det->ctrl_id == EEM_CTRL_2L) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_2L);
			else if ((det->ctrl_id == EEM_CTRL_L) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_L);
			else if ((det->ctrl_id == EEM_CTRL_GPU) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_GPU);
			else if ((det->ctrl_id == EEM_CTRL_CCI) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_CCI);
			#if EEM_BANK_SOC
			else if ((det->ctrl_id == EEM_CTRL_SOC) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_SOC);
			#endif
			#if EEM_BANK_BIG
			else if ((det->ctrl_id == EEM_CTRL_BIG) && (det->eem_eemEn[EEM_PHASE_INIT01] == 1))
				out |= BIT(EEM_CTRL_BIG);
			#endif
		}
		/* current banks: 00 0101 1111 */
		if (out == EEM_INIT01_FLAG) { /* 0x3B==out */
			eem_debug("init01 finish time is %d, out=0x%x\n", timeout, out);
			break;
		}
		udelay(100);
		timeout++;
	}
	eem_init02();
	FUNC_EXIT(FUNC_LV_LOCAL);
}

#if EN_EEM
#if 0
static char *readline(struct file *fp)
{
#define BUFSIZE 1024
	static char buf[BUFSIZE];
	static int buf_end;
	static int line_start;
	static int line_end;
	char *ret;

	FUNC_ENTER(FUNC_LV_HELP);
empty:

	if (line_start >= buf_end) {
		line_start = 0;
		buf_end = fp->f_op->read(fp, &buf[line_end], sizeof(buf) - line_end, &fp->f_pos);

		if (buf_end == 0) {
			line_end = 0;
			return NULL;
		}

		buf_end += line_end;
	}

	while (buf[line_end] != '\n') {
		line_end++;

		if (line_end >= buf_end) {
			memcpy(&buf[0], &buf[line_start], buf_end - line_start);
			line_end = buf_end - line_start;
			line_start = buf_end;
			goto empty;
		}
	}

	buf[line_end] = '\0';
	ret = &buf[line_start];
	line_start = line_end + 1;

	FUNC_EXIT(FUNC_LV_HELP);

	return ret;
}
#endif
/* leakage */
unsigned int leakage_core;
unsigned int leakage_gpu;
unsigned int leakage_sram2;
unsigned int leakage_sram1;

static int eem_probe(struct platform_device *pdev)
{
	int ret;
	struct eem_det *det;
	struct eem_ctrl *ctrl;
	/* unsigned int code = mt_get_chip_hw_code(); */
	#ifdef CONFIG_OF
	struct device_node *node = NULL;
	#endif

	FUNC_ENTER(FUNC_LV_MODULE);

	#ifdef CONFIG_OF
	node = pdev->dev.of_node;
	if (!node) {
		eem_debug("get eem device node err\n");
		return -ENODEV;
	}
	/* Setup IO addresses */
	eem_base = of_iomap(node, 0);
	eem_debug("[EEM] eem_base = 0x%p\n", eem_base);
	eem_irq_number = irq_of_parse_and_map(node, 0);
	eem_debug("[THERM_CTRL] eem_irq_number=%d\n", eem_irq_number);
	if (!eem_irq_number) {
		eem_debug("[EEM] get irqnr failed=0x%x\n", eem_irq_number);
		return 0;
	}
	#endif

	/* thermal and gpu may enable their clock by themselves */
	/* set EEM IRQ */
	ret = request_irq(eem_irq_number, eem_isr, IRQF_TRIGGER_LOW, "eem", NULL);
	if (ret) {
		eem_debug("EEM IRQ register failed (%d)\n", ret);
		WARN_ON(1);
	}
	eem_debug("Set EEM IRQ OK.\n");

	#if (defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING))
		_mt_eem_aee_init();
	#endif

	for_each_ctrl(ctrl)
		eem_init_ctrl(ctrl);

	/* call hotplug api to enable B cluster, no need CONFIG_BIG_OFF */
	#if EEM_BANK_BIG
	mt_eem_enable_big_cluster(1);
	#endif
	mt_eem_get_clk(pdev);
	mt_eem_enable_mtcmos();
	/* disable DVFS and set vproc = 1v */
	#ifndef EARLY_PORTING_PPM
	mt_ppm_ptpod_policy_activate();
	#endif

	#ifndef EARLY_PORTING_GPU
	mt_gpufreq_disable_by_ptpod();
	#endif

	/* set PWM mode*/
#ifndef EARLY_PORTING_PMIC
		/* get regulator reference */
		ret = eem_buck_get(pdev);
		if (ret != 0)
			eem_debug("eem_buck_get failed\n");

		eem_buck_enable();
		eem_buck_set_mode(1);
#endif /* ifndef EARLY_PORTING_PMIC */

	for_each_det(det)
		eem_init_det(det, &eem_devinfo);

	/* get original volt from cpu dvfs before init01*/
	for_each_det(det) {
		if (det->ops->get_orig_volt_table)
			det->ops->get_orig_volt_table(det);
	}

	/* mt_cpufreq_set_ptbl_registerCB(mt_cpufreq_set_ptbl_funcEEM);*/ /* no record table */
	eem_init01();

	#if ITurbo
	register_hotcpu_notifier(&_mt_eem_cpu_notifier);
	#endif

	eem_debug("eem_probe ok\n");
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int eem_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	*kthread_stop(eem_volt_thread);
	*/
	FUNC_ENTER(FUNC_LV_MODULE);
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

static int eem_resume(struct platform_device *pdev)
{
	#if 0
	eem_volt_thread = kthread_run(eem_volt_thread_handler, 0, "eem volt");
	if (IS_ERR(eem_volt_thread))
		eem_debug("[%s]: failed to create eem volt thread\n", __func__);
	#endif

	FUNC_ENTER(FUNC_LV_MODULE);

#if 0
	#ifdef __KERNEL__
	mt_cpufreq_eem_resume();
	#endif
#endif

	eem_init02();
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_eem_of_match[] = {
	{ .compatible = "mediatek,eem_fsm", },
	{},
};
#endif

static struct platform_driver eem_driver = {
	.remove	 = NULL,
	.shutdown   = NULL,
	.probe	  = eem_probe,
	.suspend	= eem_suspend,
	.resume	 = eem_resume,
	.driver	 = {
		.name   = "mt-eem",
#ifdef CONFIG_OF
		.of_match_table = mt_eem_of_match,
#endif
	},
};

#ifdef CONFIG_PROC_FS
int mt_eem_opp_num(enum eem_det_id id)
{
	struct eem_det *det = id_to_eem_det(id);

	FUNC_ENTER(FUNC_LV_API);
	FUNC_EXIT(FUNC_LV_API);

	return det->num_freq_tbl;
}
EXPORT_SYMBOL(mt_eem_opp_num);

void mt_eem_opp_freq(enum eem_det_id id, unsigned int *freq)
{
	struct eem_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

	for (i = 0; i < det->num_freq_tbl; i++)
		freq[i] = det->freq_tbl[i];

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_freq);

void mt_eem_opp_status(enum eem_det_id id, unsigned int *temp, unsigned int *volt)
{
	struct eem_det *det = id_to_eem_det(id);
	int i = 0;

	FUNC_ENTER(FUNC_LV_API);

#if defined(CONFIG_THERMAL) && !(EARLY_PORTING)
	*temp = tscpu_get_temp_by_bank(id);
#else
	*temp = 0;
#endif
	for (i = 0; i < det->num_freq_tbl; i++)
		volt[i] = det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]);

	FUNC_EXIT(FUNC_LV_API);
}
EXPORT_SYMBOL(mt_eem_opp_status);

int mt_eem_status(enum eem_det_id id)
{
	struct eem_det *det = id_to_eem_det(id);

	FUNC_ENTER(FUNC_LV_API);

	WARN_ON(!det); /*BUG_ON(!det);*/
	WARN_ON(!det->ops); /*BUG_ON(!det->ops);*/
	WARN_ON(!det->ops->get_status); /* BUG_ON(!det->ops->get_status);*/

	FUNC_EXIT(FUNC_LV_API);

	return det->ops->get_status(det);
}


static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	/* FIXME: EEMEN sometimes is disabled temp */
	seq_printf(m, "[%s] %s (%d)\n",
		   ((char *)(det->name) + 8),
		   det->disabled ? "disabled" : "enable",
		   det->ops->get_status(det)
		   );

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_debug_proc_write(struct file *file,
					const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;

		if (enabled == 0)
			/* det->ops->enable(det, BY_PROCFS); */
			det->ops->disable(det, 0);
		else if (enabled == 1)
			det->ops->disable(det, BY_PROCFS);
		else if (enabled == 2)
			det->ops->disable(det, BY_PROCFS_INIT2);

	} else
		ret = -EINVAL;

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

static int eem_dump_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det;
	int *val = (int *)&eem_devinfo;
	int i, k;
	#if DUMP_DATA_TO_DE
		int j;
	#endif

	FUNC_ENTER(FUNC_LV_HELP);
	#if 0
	eem_detectors[EEM_DET_BIG].ops->dump_status(&eem_detectors[EEM_DET_BIG]);
	eem_detectors[EEM_DET_L].ops->dump_status(&eem_detectors[EEM_DET_L]);
	seq_printf(m, "det->EEMMONEN= 0x%08X,det->EEMINITEN= 0x%08X\n", det->EEMMONEN, det->EEMINITEN);
	seq_printf(m, "leakage_core\t= %d\n"
			"leakage_gpu\t= %d\n"
			"leakage_little\t= %d\n"
			"leakage_big\t= %d\n",
			leakage_core,
			leakage_gpu,
			leakage_sram2,
			leakage_sram1
			);
	#endif

	for (i = 0; i < sizeof(struct eem_devinfo) / sizeof(unsigned int); i++)
		seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, val[i]);

	/*
	*for (i = 0; i < NR_HW_RES_FOR_BANK; i++)
	*	seq_printf(m, "M_HW_RES%d\t= 0x%08X\n", i, get_devinfo_with_index(50 + i));
	*/

	for_each_det(det) {
		#if !EEM_BANK_SOC
		if (det->ctrl_id == EEM_CTRL_SOC)
			continue;
		#endif
		for (i = EEM_PHASE_INIT01; i < NR_EEM_PHASE; i++) {
			seq_printf(m, "Bank_number = %d\n", det->ctrl_id);
			if (i < EEM_PHASE_MON)
				seq_printf(m, "mode = init%d\n", i+1);
			else
				seq_puts(m, "mode = mon\n");
			if (eem_log_en) {
				seq_printf(m, "0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
					det->dcvalues[i],
					det->freqpct30[i],
					det->eem_26c[i],
					det->vop30[i],
					det->eem_eemEn[i]
				);

				if (det->eem_eemEn[i] == 0x5) {
					seq_printf(m, "EEM_LOG: Bank_number = [%d] (%d) - (",
					det->ctrl_id, det->ops->get_temp(det));

					for (k = 0; k < det->num_freq_tbl - 1; k++)
						seq_printf(m, "%d, ",
						det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k]));
					seq_printf(m, "%d) - (", det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k]));

					for (k = 0; k < det->num_freq_tbl - 1; k++)
						seq_printf(m, "%d, ", det->freq_tbl[k]);
					seq_printf(m, "%d)\n", det->freq_tbl[k]);
				}
			} /* if (eem_log_en) */
			#if DUMP_DATA_TO_DE
			for (j = 0; j < ARRAY_SIZE(reg_dump_addr_off); j++)
				seq_printf(m, "0x%08lx = 0x%08x\n",
					(unsigned long)EEM_BASEADDR + reg_dump_addr_off[j],
					det->reg_dump_data[j][i]
					);
			#endif
		}
	}
	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;
	u32 rdata = 0, i;

	FUNC_ENTER(FUNC_LV_HELP);

	rdata = det->ops->get_volt(det);

	if (rdata != 0)
		seq_printf(m, "%d\n", rdata);
	else
		seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);

	if ((det->ctrl_id != EEM_CTRL_GPU) && (det->ctrl_id != EEM_CTRL_SOC)) {
		for (i = 0; i < det->num_freq_tbl; i++)
			seq_printf(m, "EEM_HW, det->volt_tbl[%d] = [%x], det->volt_tbl_pmic[%d] = [%x]\n",
			i, det->volt_tbl[i], i, det->volt_tbl_pmic[i]);
		#if 0 /* no record table */
		for (i = 0; i < NR_FREQ; i++) {
			seq_printf(m, "(iDVFS, 0x%x)(Vs, 0x%x) (Vp, 0x%x, %d) (F_Setting)(%x, %x, %x, %x, %x)\n",
				(det->recordRef[i*2] >> 14) & 0x3FFFF,
				(det->recordRef[i*2] >> 7) & 0x7F,
				det->recordRef[i*2] & 0x7F,
				det->ops->pmic_2_volt(det, (det->recordRef[i*2] & 0x7F)),
				(det->recordRef[i*2+1] >> 21) & 0x1F,
				(det->recordRef[i*2+1] >> 12) & 0x1FF,
				(det->recordRef[i*2+1] >> 7) & 0x1F,
				(det->recordRef[i*2+1] >> 4) & 0x7,
				det->recordRef[i*2+1] & 0xF
				);
		}
		#endif
	}
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#if 0 /* no record table */
static ssize_t eem_cur_volt_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int voltValue = 0, voltProc = 0, voltSram = 0, voltPmic = 0, index = 0;
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	/* if (!kstrtoint(buf, 10, &voltValue)) { */
	if (sscanf(buf, "%u %u", &voltValue, &index) == 2) {
		if ((det->ctrl_id != EEM_CTRL_GPU) && (det->ctrl_id != EEM_CTRL_SOC)) {
			ret = 0;
			det->recordRef[NR_FREQ * 2] = 0x00000000;
			mb(); /* SRAM writing */
			voltPmic = det->ops->volt_2_pmic(det, voltValue);
			if (det->ctrl_id == EEM_CTRL_BIG)
				voltProc = clamp(
				(unsigned int)voltPmic,
				(unsigned int)det->VMIN,
				(unsigned int)det->VMAX);
			else
				voltProc = clamp(
				(unsigned int)voltPmic,
				(unsigned int)(det->ops->eem_2_pmic(det, det->VMIN)),
				(unsigned int)(det->ops->eem_2_pmic(det, det->VMAX)));

			voltPmic = det->ops->volt_2_pmic(det, voltValue + 10000);
			voltSram = clamp(
				(unsigned int)(voltPmic),
				(unsigned int)(det->ops->volt_2_pmic(det, VMIN_SRAM)),
				(unsigned int)(det->ops->volt_2_pmic(det, VMAX_SRAM)));

			/* for (i = 0; i < NR_FREQ; i++) */
			det->recordRef[index * 2] = (det->recordRef[index * 2]  & (~0x3FFF)) |
				(((PMIC_2_RMIC(det, voltSram) & 0x7F) << 7) | (voltProc & 0x7F));

			det->recordRef[NR_FREQ * 2] = 0xFFFFFFFF;
			mb(); /* SRAM writing */
		}
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! voltage = (80000 ~ 115500), index = (0 ~ 15)\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif /* no record table */
static int eem_status_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "bank = %d, (%d) - (",
		   det->ctrl_id, det->ops->get_temp(det));
	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (", det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));

	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static int eem_log_en_proc_show(struct seq_file *m, void *v)
{
	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", eem_log_en);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_log_en_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &eem_log_en)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (eem_log_en) {
	case 0:
		eem_debug("eem log disabled.\n");
		hrtimer_cancel(&eem_log_timer);
		break;

	case 1:
		eem_debug("eem log enabled.\n");
		hrtimer_start(&eem_log_timer, ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
		break;

	default:
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "%d\n", det->volt_offset);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_offset_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));
	unsigned long flags;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		det->volt_offset = offset;
		mt_ptp_lock(&flags);
		eem_set_eem_volt(det);
		mt_ptp_unlock(&flags);
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}

#if ITurbo
static int eem_iturbo_en_proc_show(struct seq_file *m, void *v)
{
	int i;

	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%s\n", ((ctrl_ITurbo) ? "Enable" : "Disable"));
	for (i = 0; i < NR_FREQ; i++)
		seq_printf(m, "ITurbo_offset[%d] = %d (0.00625 per step)\n", i, ITurbo_offset[i]);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_iturbo_en_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{
	int ret;
	unsigned int value;
	char *buf = (char *) __get_free_page(GFP_USER);

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &value)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (value) {
	case 0:
		eem_debug("eem ITurbo disabled.\n");
		ctrl_ITurbo = 0;
		break;

	case 1:
		eem_debug("eem ITurbo enabled.\n");
		ctrl_ITurbo = 2;
		break;

	default:
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif /* if ITurbo */

static int eem_vcore_volt_proc_show(struct seq_file *m, void *v)
{
	unsigned int i = 0;
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);
	for (i = 0; i < VCORE_NR_FREQ; i++) {
		/* transfer 10uv to uv before showing*/
		seq_printf(m, "%d ", det->ops->pmic_2_volt(det, get_vcore_ptp_volt(i)) * 10);
		/* eem_debug("eem_vcore %d\n", det->ops->pmic_2_volt(det, get_vcore_ptp_volt(i)) * 10); */
	}
	seq_puts(m, "\n");

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_vcore_volt_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int index = 0;
	unsigned int newVolt = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &index, &newVolt) == 2) {
		/* transfer uv to 10uv */
		if (newVolt > 10)
			newVolt = newVolt / 10;

		if (index >= VCORE_NR_FREQ)
			goto out;

		ret = mt_eem_update_vcore_volt(index, newVolt);
		if (ret == 1) {
			ret = -EINVAL;
			if (index == 0)
				eem_debug("volt should be set larger than index %d\n", index+1);
			else if (index == VCORE_NR_FREQ-1)
				eem_debug("volt should be set smaller than index %d\n", index-1);
			else
				eem_debug("volt should be set between index %d - %d\n", index-1, index+1);
		}
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#if 0
static int eem_vcore_enable_proc_show(struct seq_file *m, void *v)
{

	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", ctrl_VCORE_Volt_Enable);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_vcore_enable_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int enable = 0;
	int i = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	switch (enable) {
	case 0:
		eem_debug("vcore eem disabled! Restore to signed off volt.\n");
		ctrl_VCORE_Volt_Enable = 0;
		/* restore vore volt to signed off volt */
		for (i = 0; i < VCORE_NR_FREQ; i++)
			mt_eem_update_vcore_volt(i, vcore_opp[i][0]);
		break;

	case 1:
		eem_debug("vcore eem enabled. Reset to voltage bin volt\n");
		ctrl_VCORE_Volt_Enable = 1;
		/* reset vcore volt to voltage bin value */
		for (i = 0; i < VCORE_NR_FREQ; i++)
			mt_eem_update_vcore_volt(i, vcore_opp[i][eem_vcore_index[i]]);
		/* bottom up compare each volt to ensure each opp is in descending order */
		for (i = VCORE_NR_FREQ - 2; i >= 0; i--) {
			eem_debug("i=%d\n", i);
			eem_vcore[i] = (eem_vcore[i] < eem_vcore[i+1]) ? eem_vcore[i+1] : eem_vcore[i];
			eem_debug("final eem_vcore[%d]=%d\n", i, eem_vcore[i]);
		}
		break;

	default:
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		ret = -EINVAL;
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
		.write		  = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RO(eem_status);
PROC_FOPS_RO(eem_cur_volt);
PROC_FOPS_RW(eem_offset);
PROC_FOPS_RO(eem_dump);
PROC_FOPS_RW(eem_log_en);
PROC_FOPS_RW(eem_vcore_volt);
/* PROC_FOPS_RW(eem_vcore_enable);*/

#if ITurbo
PROC_FOPS_RW(eem_iturbo_en);
#endif

static int create_procfs(void)
{
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct eem_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries_vcore[] = {
		PROC_ENTRY(eem_vcore_volt),
		/* PROC_ENTRY(eem_debug), */
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		PROC_ENTRY(eem_status),
		PROC_ENTRY(eem_cur_volt),
		PROC_ENTRY(eem_offset),
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		PROC_ENTRY(eem_log_en),
		/* PROC_ENTRY(eem_vcore_enable), */
		#if ITurbo
		PROC_ENTRY(eem_iturbo_en),
		#endif
	};

	FUNC_ENTER(FUNC_LV_HELP);

	/* create procfs root /proc/eem */
	eem_dir = proc_mkdir("eem", NULL);

	if (!eem_dir) {
		eem_debug("[%s]: mkdir /proc/eem failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	/* create vcore procfs */
	det  = &eem_detectors[EEM_DET_SOC];
	det_dir = proc_mkdir(det->name, eem_dir);
	if (!det_dir) {
		eem_debug("[%s]: mkdir /proc/eem/%s failed\n", __func__, det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	for (i = 0; i < ARRAY_SIZE(det_entries_vcore); i++) {
		if (!proc_create_data(det_entries_vcore[i].name,
			S_IRUGO | S_IWUSR | S_IWGRP,
			det_dir,
			det_entries_vcore[i].fops, det)) {
			eem_debug("[%s]: create /proc/eem/%s/%s failed\n", __func__,
				det->name, det_entries_vcore[i].name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -3;
				}
	}

	/* if ctrl_EEM_Enable =1, and has efuse value, create other banks procfs */
	if (ctrl_EEM_Enable != 0) {
		for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
			if (!proc_create(eem_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP,
						eem_dir, eem_entries[i].fops)) {
				eem_debug("[%s]: create /proc/eem/%s failed\n", __func__,
							eem_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->ctrl_id == EEM_CTRL_SOC)
				continue;

			if (det->features == 0)
				continue;

			det_dir = proc_mkdir(det->name, eem_dir);

			if (!det_dir) {
				eem_debug("[%s]: mkdir /proc/eem/%s failed\n", __func__, det->name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -2;
			}

			for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
				if (!proc_create_data(det_entries[i].name,
					S_IRUGO | S_IWUSR | S_IWGRP,
					det_dir,
					det_entries[i].fops, det)) {
					eem_debug("[%s]: create /proc/eem/%s/%s failed\n", __func__,
						det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
				}
			}
		}
	} /* if (ctrl_EEM_Enable != 0) */

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
#endif /* CONFIG_PROC_FS */

/* update vcore voltage by index and new volt(10uv) */
static unsigned int  mt_eem_update_vcore_volt(unsigned int index, unsigned int newVolt)
{
	struct eem_det *det;
	unsigned int ret = 0;
	unsigned int volt_pmic = 0;

	FUNC_ENTER(FUNC_LV_MODULE);

	det = &eem_detectors[EEM_DET_SOC];
	volt_pmic = det->ops->volt_2_pmic(det, newVolt);

	#if 0 /* disable vcore opp bound checking */
	/* transfer volt to pmic value */
	if ((index == 0) && (volt_pmic >= eem_vcore[index+1]))
		eem_vcore[index] = volt_pmic;
	else if ((index == VCORE_NR_FREQ-1) && (volt_pmic <= eem_vcore[index-1]))
		eem_vcore[index] = volt_pmic;
	else if ((index > 0) && (index < VCORE_NR_FREQ-1) &&
			(volt_pmic <= eem_vcore[index-1]) &&
			(volt_pmic >= eem_vcore[index+1]))
		eem_vcore[index] = volt_pmic;
	else
		ret = 1;
	#else
	if ((index < VCORE_NR_FREQ) && (index >= 0)) {
		eem_vcore[index] = volt_pmic;
		eem_debug("update volt: new Volt: %d --> eem_vcore[%d] = 0x%x\n",
				newVolt, index, eem_vcore[index]);
	} else {
		/* index illegal */
		ret = 1;
	}
	#endif

	#ifndef EARLY_PORTING_VCORE
	ret = spm_vcorefs_pwarp_cmd();
	#endif

	FUNC_EXIT(FUNC_LV_MODULE);
	return ret;
}

#if 0
void eem_set_pi_offset(enum eem_ctrl_id id, int step)
{
	struct eem_det *det = id_to_eem_det(id);

	det->pi_offset = step;

#if (defined(CONFIG_EEM_AEE_RR_REC) && !(EARLY_PORTING)) /* irene */
	aee_rr_rec_eem_pi_offset(step);
#endif
}
#endif

void eem_set_pi_efuse(enum eem_ctrl_id id, unsigned int pi_efuse)
{
	struct eem_det *det = id_to_eem_det(id);

	eem_debug("[%s] id: %d, pi_efuse: 0x%x\n", __func__, id, pi_efuse);

	det->pi_efuse = pi_efuse;
}

unsigned int get_efuse_status(void)
{
	return eem_checkEfuse;
}

#if 0
static int __init dt_get_ptp_devinfo(unsigned long node, const char *uname, int depth, void *data)
{
	struct devinfo_ptp_tag *tags;
	unsigned int size = 0;

	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	tags = (struct devinfo_ptp_tag *) of_get_flat_dt_prop(node, "atag,ptp", &size);

	if (tags) {
		vcore0 = tags->volt0;
		vcore1 = tags->volt1;
		vcore2 = tags->volt2;
		eem_debug("[EEM][VCORE] - Kernel Got from DT (0x%0X, 0x%0X, 0x%0X)\n",
			vcore0, vcore1, vcore2);
	}
}
static int __init vcore_ptp_init(void)
{
	of_scan_flat_dt(dt_get_ptp_devinfo, NULL);

	return 0;
}
#endif


static int new_eem_val = 1; /* default no change */
static int  __init fn_change_eem_status(char *str)
{
	int new_set_val;

	eem_debug("fn_change_eem_status\n");
	if (get_option(&str, &new_set_val)) {
		new_eem_val = new_set_val;
		eem_debug("new_eem_val = %d\n", new_eem_val);
		return 0;
	}
	return -EINVAL;
}
early_param("eemen", fn_change_eem_status);

static int __init eem_init(void)
{
	int err = 0;

	eem_debug("[EEM] new_eem_val=%d, ctrl_EEM_Enable=%d\n", new_eem_val, ctrl_EEM_Enable);

	get_devinfo();

	#if 0 /* preloader params to control ptp, none use now*/
	if (new_eem_val == 0) {
		ctrl_EEM_Enable = 0;
		eem_debug("Disable EEM by kernel config\n");
	}
	#endif

	create_procfs();
	/* process_voltage_bin(&eem_devinfo); */ /* LTE voltage bin use I-Chang */
	if (ctrl_EEM_Enable == 0) {
		eem_debug("ctrl_EEM_Enable = 0x%X\n", ctrl_EEM_Enable);
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}
	informEEMisReady = 1;

	#if ITurbo
	/* Read E-Fuse to control ITurbo mode */
	ctrl_ITurbo = eem_devinfo.OD18_TURBO;
	#endif

	/* init timer for log / volt */
	hrtimer_init(&eem_log_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	eem_log_timer.function = eem_log_timer_func;

	/*
	 * reg platform device driver
	 */
	err = platform_driver_register(&eem_driver);

	if (err) {
		eem_debug("EEM driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	return 0;
}

static void __exit eem_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	eem_debug("eem de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

/* module_init(eem_conf); */ /*no record table*/
subsys_initcall(vcore_ptp_init); /* I-Chang */
late_initcall(eem_init); /* late_initcall */
#endif /* EN_EEM */

#else /*if EEM_ENABLE_TINYSYS_SSPM */

#define EEM_IPI_SEND_DATA_LEN 4 /* size of cmd and args = 4 slot */
static unsigned int eem_to_sspm(unsigned int cmd, struct eem_ipi_data *eem_data)
{
	unsigned int ackData = 0;
	unsigned int len = EEM_IPI_SEND_DATA_LEN;
	unsigned int ret;

	FUNC_ENTER(FUNC_LV_MODULE);
	switch (cmd) {
	case IPI_EEM_INIT:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_INIT) ret:%d - %d\n", ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	case IPI_EEM_PROBE:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_PROBE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	case IPI_EEM_INIT01:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_INIT01) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	case IPI_EEM_INIT02:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_INIT02) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	#ifdef EEM_VCORE_IN_SSPM
	case IPI_EEM_VCORE_GET_VOLT:
		eem_data->cmd = cmd;
		/* defined at sspm_ipi.h, id is difined at elbrus/sspm_ipi_pin.h */
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_VCORE_GET_VOLT) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;
	#endif

	#if 0
	case IPI_EEM_GPU_DVFS_GET_STATUS:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_GPU_DVFS_GET_STATUS) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;
	#endif

	case IPI_EEM_DEBUG_PROC_WRITE:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_DEBUG_PROC_WRITE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	#ifdef EEM_LOG_EN
	case IPI_EEM_LOGEN_PROC_SHOW:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_LOGEN_PROC_SHOW) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	case IPI_EEM_LOGEN_PROC_WRITE:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_LOGEN_PROC_WRITE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;
	#endif

	#ifdef EEM_OFFSET
	case IPI_EEM_OFFSET_PROC_WRITE:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_OFFSET_PROC_WRITE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;
	#endif

	case IPI_EEM_VCORE_INIT:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_VCORE_INIT) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	case IPI_EEM_VCORE_UPDATE_PROC_WRITE:
		/* ackData will be 0 if set newVolt successfully*/
		/* ackData will be 1 if newVolt is illegal */
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_VCORE_UPDATE_PROC_WRITE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	#ifdef EEM_CUR_VOLT_PROC_SHOW
	case IPI_EEM_CUR_VOLT_PROC_SHOW:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_CUR_VOLT_PROC_SHOW) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;
	#endif

	case IPI_EEM_SEND_UPOWER_TBL_REF:
		eem_data->cmd = cmd;
		ret = sspm_ipi_send_sync(IPI_ID_PTPOD, IPI_OPT_POLLING, eem_data, len, &ackData, 1);
		if (ret != 0)
			eem_debug("sspm_ipi_send_sync error(IPI_EEM_VCORE_UPDATE_PROC_WRITE) ret:%d - %d\n",
						ret, ackData);
		else if (ackData < 0)
			eem_debug("cmd(%d) return error(%d)\n", cmd, ackData);
		break;

	default:
			eem_debug("cmd(%d) wrong!!\n", cmd);
			break;
	}

	FUNC_EXIT(FUNC_LV_MODULE);
	return ackData;
}


#ifdef CONFIG_PROC_FS
static int eem_debug_proc_show(struct seq_file *m, void *v)
{
	int ret = 0;
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_MODULE);

	/* if eem is not enabled yet, show vcore ptp eem_debug only */
	if (eem_disable) {
		seq_printf(m, "[%s] %s (0x1)\n",
		   ((char *)(det->name) + 8),
		   "disabled"
		   );
	} else {
		if ((det->ctrl_id == EEM_CTRL_SOC) && (det->features == 0))
			ret = 1;
		else
			ret = det->disabled;

		seq_printf(m, "[%s] %s (0x%X)\n",
		   ((char *)(det->name) + 8),
		   ret ? "disable" : "enable",
		   ret
		   );
	}
	FUNC_EXIT(FUNC_LV_MODULE);
	return 0;

}

static ssize_t eem_debug_proc_write(struct file *file,
					const char __user *buffer, size_t count, loff_t *pos)
{

	int ret = 0;
	int enabled = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));
	struct eem_ipi_data eem_data;
	int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &enabled)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = enabled;
		ipi_ret = eem_to_sspm(IPI_EEM_DEBUG_PROC_WRITE, &eem_data);
		det->disabled = enabled;
	} else
		ret = -EINVAL;

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;

}
#ifdef EEM_CUR_VOLT_PROC_SHOW
static int eem_cur_volt_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;
	u32 i;
	struct eem_ipi_data eem_data;
	unsigned char num_freq_tbl;
	unsigned int locklimit = 0, ret;
	unsigned char lock;

	FUNC_ENTER(FUNC_LV_HELP);

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = det_to_id(det);
	/* return det current volt by det->ops->get_volt */
	ret = eem_to_sspm(IPI_EEM_CUR_VOLT_PROC_SHOW, &eem_data);

	/* rdata = det->ops->get_volt(det); */
	while (1) {
		lock = eem_logs->det_log[det->ctrl_id].lock;
		locklimit++;
		mdelay(5); /* wait 5 ms */
		eem_debug("det(%d) lock=0x%X\n", det->ctrl_id, lock);
		/* if lock or access less than 5 times, read the lock again until unlock */
		if ((lock & 0x1) && (locklimit < 5))
			continue;

		num_freq_tbl = eem_logs->det_log[det->ctrl_id].num_freq_tbl;

		if (det->ctrl_id != EEM_CTRL_SOC) {
			for (i = 0; i < num_freq_tbl; i++) {
				det->volt_tbl[i] = eem_logs->det_log[det->ctrl_id].volt_tbl[i];
				det->volt_tbl_pmic[i] = eem_logs->det_log[det->ctrl_id].volt_tbl_pmic[i];
			}
		}

		lock = eem_logs->det_log[det->ctrl_id].lock;
		eem_debug("det(%d) lock=0x%X\n", det->ctrl_id, lock);
		if ((lock & 0x1) && (locklimit < 5))
			continue; /* if lock, read dram again */
		else
			break; /* if unlock, break out while loop, read next det*/
	}

	if (ret != 0) {
		seq_printf(m, "current volt:%d, temp:%d\n",
			ret, eem_logs->det_log[det->ctrl_id].temp);
	} else {
		seq_printf(m, "EEM[%s] read current voltage fail\n", det->name);
	}

	if (det->ctrl_id != EEM_CTRL_SOC) {
		for (i = 0; i < num_freq_tbl; i++)
			seq_printf(m, "det->volt_tbl[%d] =0x%X, det->volt_tbl_pmic[%d]=0x%X(%d)\n",
			i, det->volt_tbl[i],
			i, det->volt_tbl_pmic[i], det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
	}

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#endif
static int eem_dump_proc_show(struct seq_file *m, void *v)
{

	struct eem_det *det;
	/* int *val = (int *)&eem_devinfo; */
	int i, k;
	unsigned char lock;
	unsigned int locklimit = 0;
	#if DUMP_DATA_TO_DE
	int j;
	#endif
	/* unsigned int ipi_ret; */

	FUNC_ENTER(FUNC_LV_HELP);
	/*
	*eem_detectors[EEM_DET_BIG].ops->dump_status(&eem_detectors[EEM_DET_BIG]);
	*eem_detectors[EEM_DET_L].ops->dump_status(&eem_detectors[EEM_DET_L]);
	*seq_printf(m, "det->EEMMONEN= 0x%08X,det->EEMINITEN= 0x%08X\n", det->EEMMONEN, det->EEMINITEN);
	*seq_printf(m, "leakage_core\t= %d\n"
	*		"leakage_gpu\t= %d\n"
	*		"leakage_little\t= %d\n"
	*		"leakage_big\t= %d\n",
	*		leakage_core,
	*		leakage_gpu,
	*		leakage_sram2,
	*		leakage_sram1
	*		);
	*/

	for (i = 0; i < NR_HW_RES_FOR_BANK; i++)
		seq_printf(m, "M_HW_RES%d\t=0x%08X\n", i, eem_logs->hw_res[i]);

	for_each_det(det) {
		lock = eem_logs->det_log[det->ctrl_id].lock;
		locklimit++;
		eem_debug("det(%d) lock=%d\n", det->ctrl_id, lock);

		det->num_freq_tbl = eem_logs->det_log[det->ctrl_id].num_freq_tbl;
		#if !EEM_BANK_SOC
		if (det->ctrl_id == EEM_CTRL_SOC)
			break; /* break out while loop, read next det */
		#endif
		for (i = EEM_PHASE_INIT01; i < NR_EEM_PHASE; i++) {
			/* seq_printf(m, "Bank_number = %d\n", det->ctrl_id); */
			seq_printf(m, "Bank_number = %d\n", det->ctrl_id);
			if (i < EEM_PHASE_MON)
				seq_printf(m, "mode = init%d\n", i+1);
			else
				seq_puts(m, "mode = mon\n");

			if (eem_log_en) {
				seq_printf(m, "0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
					eem_logs->det_log[det->ctrl_id].dcvalues[i],
					eem_logs->det_log[det->ctrl_id].freqpct30[i],
					eem_logs->det_log[det->ctrl_id].eem_26c[i],
					eem_logs->det_log[det->ctrl_id].vop30[i],
					eem_logs->det_log[det->ctrl_id].eem_eemEn[i]
					);

				eem_debug("0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
					eem_logs->det_log[det->ctrl_id].dcvalues[i],
					eem_logs->det_log[det->ctrl_id].freqpct30[i],
					eem_logs->det_log[det->ctrl_id].eem_26c[i],
					eem_logs->det_log[det->ctrl_id].vop30[i],
					eem_logs->det_log[det->ctrl_id].eem_eemEn[i]
					);
				if (eem_logs->det_log[det->ctrl_id].eem_eemEn[i] == 0x5) {
					seq_printf(m, "EEM_LOG: Bank_number = [%d] (%d) - (",
					/* det->ctrl_id, det->ops->get_temp(det)); */
					det->ctrl_id, eem_logs->det_log[det->ctrl_id].temp);

					eem_debug("det num_freq_tbl=%d\n",
						eem_logs->det_log[det->ctrl_id].num_freq_tbl);

					for (k = 0; k < det->num_freq_tbl - 1; k++) {
						seq_printf(m, "%d, ", /* %d */
						/* det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k])); */
						eem_logs->det_log[det->ctrl_id].volt_tbl_init2[k]);
						eem_debug("[%d]%d, ", /* %d */
						/* det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k])); */
						k, eem_logs->det_log[det->ctrl_id].volt_tbl_init2[k]);
					}
					seq_printf(m, "%d) - (",
						eem_logs->det_log[det->ctrl_id].volt_tbl_init2[k]);
					eem_debug("[%d] %d) - (",
						k, eem_logs->det_log[det->ctrl_id].volt_tbl_init2[k]);

					for (k = 0; k < det->num_freq_tbl - 1; k++) {
						seq_printf(m, "%d, ",
							eem_logs->det_log[det->ctrl_id].freq_tbl[k]);
						eem_debug("[%d]%d, ",
							k, eem_logs->det_log[det->ctrl_id].freq_tbl[k]);
					}
					seq_printf(m, "%d)\n", eem_logs->det_log[det->ctrl_id].freq_tbl[k]);
					eem_debug("[%d]%d)\n", k, eem_logs->det_log[det->ctrl_id].freq_tbl[k]);
				}
				#if 0
				if (eem_logs->det_log[det->ctrl_id].eem_eemEn[i] == 0x2) {
					seq_printf(m, "EEM_LOG: Bank_number = [%d] (%d)\n",
					/* det->ctrl_id, det->ops->get_temp(det)); */
					det->ctrl_id, eem_logs->det_log[det->ctrl_id].temp);
					seq_puts(m, "volt_tbl = ");
					for (k = 0; k < det->num_freq_tbl; k++) {
						seq_printf(m, "%d, ", /* %d */
						/* det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k])); */
						eem_logs->det_log[det->ctrl_id].volt_tbl_mon[k]);
					}
					seq_puts(m, "\n");
					seq_puts(m, "volt_tbl_pmic = ");
					for (k = 0; k < det->num_freq_tbl; k++) {
						seq_printf(m, "%d, ", /* %d */
						/* det->ops->pmic_2_volt(det, det->volt_tbl_pmic[k])); */
						eem_logs->det_log[det->ctrl_id].volt_tbl_pmic[k]);
					}
					seq_puts(m, "\n");
				}
				#endif
			} /* if (eem_log_en)*/
			#if DUMP_DATA_TO_DE
			mb(); /* SRAM writing */
			for (j = 0; j < ARRAY_SIZE(reg_dump_addr_off); j++)
				seq_printf(m, "0x%08lx = 0x%08x\n",
					(unsigned long)EEM_BASEADDR + reg_dump_addr_off[j],
					/* det->reg_dump_data[j][i] */
					eem_logs->det_log[det->ctrl_id].reg_dump_data[j][i]
					);
			mb(); /* SRAM writing */
			#endif
		} /* for init1 to mon*/
		lock = eem_logs->det_log[det->ctrl_id].lock;
		/* eem_debug("det(%d) lock=%d\n", det->ctrl_id, lock);*/
	} /* for_each_det */
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#if 0
static int eem_status_proc_show(struct seq_file *m, void *v)
{

	int i;
	struct eem_det *det = (struct eem_det *)m->private;
	struct eem_ipi_data eem_data;

	FUNC_ENTER(FUNC_LV_HELP);

	seq_printf(m, "bank = %d, (%d) - (",
		   det->ctrl_id, det->ops->get_temp(det));
	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));
	seq_printf(m, "%d) - (", det->ops->pmic_2_volt(det, det->volt_tbl_pmic[i]));

	for (i = 0; i < det->num_freq_tbl - 1; i++)
		seq_printf(m, "%d, ", det->freq_tbl[i]);
	seq_printf(m, "%d)\n", det->freq_tbl[i]);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}
#endif

#ifdef EEM_LOG_EN
static int eem_log_en_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	ipi_ret = eem_to_sspm(IPI_EEM_LOGEN_PROC_SHOW, &eem_data);
	seq_printf(m, "%d\n", ipi_ret);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_log_en_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = -EINVAL;

	if (kstrtoint(buf, 10, &eem_log_en)) {
		eem_debug("bad argument! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = eem_log_en;
	ipi_ret = eem_to_sspm(IPI_EEM_LOGEN_PROC_WRITE, &eem_data);
out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif /*ifdef EEM_LOG_EN */

#ifdef EEM_OFFSET
static int eem_offset_proc_show(struct seq_file *m, void *v)
{
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);
	seq_printf(m, "%d\n", det->volt_offset);
	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_offset_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	int offset = 0;
	struct eem_det *det = (struct eem_det *)PDE_DATA(file_inode(file));
	unsigned int ipi_ret = 0;
	struct eem_ipi_data eem_data;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (!kstrtoint(buf, 10, &offset)) {
		ret = 0;
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = det_to_id(det);
		eem_data.u.data.arg[1] = offset;
		ipi_ret = eem_to_sspm(IPI_EEM_OFFSET_PROC_WRITE, &eem_data);
		/* to show in eem_offset_proc_show */
		det->volt_offset = (signed char)offset;
		eem_debug("set volt_offset %d(%d)\n", offset, det->volt_offset);
	} else {
		ret = -EINVAL;
		eem_debug("bad argument_1!! argument should be \"0\"\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif

static int eem_vcore_volt_proc_show(struct seq_file *m, void *v)
{

	unsigned int i = 0;
	struct eem_det *det = (struct eem_det *)m->private;

	FUNC_ENTER(FUNC_LV_HELP);

	for (i = 0; i < VCORE_NR_FREQ; i++)
		seq_printf(m, "%d ", det->ops->pmic_2_volt(det, get_vcore_ptp_volt(i)) * 10);

	seq_puts(m, "\n");

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

#if 0
unsigned int check_vcore_volt_boundary(unsigned int index, unsigned int newVolt)
{
	unsigned int volt_pmic = VOLT_2_VCORE_PMIC(newVolt);

	if ((index == 0) && (volt_pmic >= eem_vcore[index+1])) {
		return 0;
	} else if ((index == VCORE_NR_FREQ-1) && (volt_pmic <= eem_vcore[index-1])) {
		return 0;
	} else if ((index > 0) && (index < VCORE_NR_FREQ-1) &&
				(volt_pmic <= eem_vcore[index-1]) &&
				(volt_pmic >= eem_vcore[index+1])) {
		return 0;
	} else {
		return -EINVAL;
	}
}
#endif

static ssize_t eem_vcore_volt_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int index = 0;
	unsigned int newVolt = 0;
	struct eem_ipi_data eem_data;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &index, &newVolt) == 2) {
		/* transfer uv to 10uv */
		if (newVolt > 10)
			newVolt = newVolt / 10;
		/* disable bound checking */
		#if 0
		ret = check_vcore_volt_boundary(index, newVolt);
		if (ret == -EINVAL) {
			if (index == 0)
				eem_debug("volt should be set larger than index %d\n", index+1);
			else if (index == 4) /* VCORE_NR_FREQ = 5 */
				eem_debug("volt should be set smaller than index %d\n", index-1);
			else
				eem_debug("volt should be set between index %d - %d\n", index-1, index+1);
		}
		#endif
		eem_debug("set eem_vcore[%d]=%d\n", index, newVolt);
		memset(&eem_data, 0, sizeof(struct eem_ipi_data));
		eem_data.u.data.arg[0] = index;
		eem_data.u.data.arg[1] = newVolt;
		/* ret 0 */
		ret = eem_to_sspm(IPI_EEM_VCORE_UPDATE_PROC_WRITE, &eem_data);
	} else {
		ret = -EINVAL;
		eem_debug("bad argument!! argument should be \"0\"1\n");
	}

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#if 0
static int eem_vcore_enable_proc_show(struct seq_file *m, void *v)
{
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	/* eem_data.u.data.arg[0] = det_to_id(det); */
	/* return ctrl_VCORE_Volt_Enable from sspm */
	ipi_ret = eem_to_sspm(IPI_EEM_VCORE_ENABLE_PROC_SHOW, &eem_data);
	seq_printf(m, "%d\n", ipi_ret);

	FUNC_EXIT(FUNC_LV_HELP);

	return 0;
}

static ssize_t eem_vcore_enable_proc_write(struct file *file,
					 const char __user *buffer, size_t count, loff_t *pos)
{

	int ret;
	char *buf = (char *) __get_free_page(GFP_USER);
	unsigned int enable = 0;
	/* unsigned int i = 0; */
	struct eem_ipi_data eem_data;
	unsigned int ipi_ret = 0;

	FUNC_ENTER(FUNC_LV_HELP);

	if (!buf) {
		FUNC_EXIT(FUNC_LV_HELP);
		return -ENOMEM;
	}

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		eem_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = enable;
	ipi_ret = eem_to_sspm(IPI_EEM_VCORE_ENABLE_PROC_WRITE, &eem_data);

out:
	free_page((unsigned long)buf);
	FUNC_EXIT(FUNC_LV_HELP);

	return (ret < 0) ? ret : count;
}
#endif

#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
		.write		  = name ## _proc_write,			\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,				\
		.open		   = name ## _proc_open,			\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,			\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(eem_debug);
PROC_FOPS_RW(eem_vcore_volt);
/* PROC_FOPS_RO(eem_status); */
#ifdef EEM_CUR_VOLT_PROC_SHOW
PROC_FOPS_RO(eem_cur_volt);
#endif

#ifdef EEM_OFFSET
PROC_FOPS_RW(eem_offset);
#endif

PROC_FOPS_RO(eem_dump);
#ifdef EEM_LOG_EN
PROC_FOPS_RW(eem_log_en);
#endif
/* PROC_FOPS_RW(eem_vcore_enable); */

static int create_procfs(void)
{
	struct proc_dir_entry *eem_dir = NULL;
	struct proc_dir_entry *det_dir = NULL;
	int i;
	struct eem_det *det;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry det_entries_vcore[] = {
		/* PROC_ENTRY(eem_debug), */
		PROC_ENTRY(eem_vcore_volt),
	};

	struct pentry det_entries[] = {
		PROC_ENTRY(eem_debug),
		/* PROC_ENTRY(eem_status),*/
		#ifdef EEM_CUR_VOLT_PROC_SHOW
		PROC_ENTRY(eem_cur_volt),
		#endif
		#ifdef EEM_OFFSET
		PROC_ENTRY(eem_offset),
		#endif
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_dump),
		#ifdef EEM_LOG_EN
		PROC_ENTRY(eem_log_en),
		#endif
	};

	FUNC_ENTER(FUNC_LV_HELP);

	/* create procfs root /proc/eem */
	eem_dir = proc_mkdir("eem", NULL);

	if (!eem_dir) {
		eem_debug("[%s]: mkdir /proc/eem failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	/* create vcore procfs */
	det  = &eem_detectors[EEM_DET_SOC];
	det_dir = proc_mkdir(det->name, eem_dir);
	if (!det_dir) {
		eem_debug("[%s]: mkdir /proc/eem/%s failed\n", __func__, det->name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -2;
	}

	for (i = 0; i < ARRAY_SIZE(det_entries_vcore); i++) {
		if (!proc_create_data(det_entries_vcore[i].name,
			S_IRUGO | S_IWUSR | S_IWGRP,
			det_dir,
			det_entries_vcore[i].fops, det)) {
			eem_debug("[%s]: create /proc/eem/%s/%s failed\n", __func__,
				det->name, det_entries_vcore[i].name);
		FUNC_EXIT(FUNC_LV_HELP);
		return -3;
				}
	}

	/* if eem is enabled and has efuse value, create other banks procfs */
	if (eem_disable == 0) {
		for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
			if (!proc_create(eem_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP,
						eem_dir, eem_entries[i].fops)) {
				eem_debug("[%s]: create /proc/eem/%s failed\n", __func__,
							eem_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
			}
		}

		for_each_det(det) {
			if (det->ctrl_id == EEM_CTRL_SOC)
				continue;

			det_dir = proc_mkdir(det->name, eem_dir);

			if (!det_dir) {
				eem_debug("[%s]: mkdir /proc/eem/%s failed\n", __func__, det->name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -2;
			}

			for (i = 0; i < ARRAY_SIZE(det_entries); i++) {
				if (!proc_create_data(det_entries[i].name,
					S_IRUGO | S_IWUSR | S_IWGRP,
					det_dir,
					det_entries[i].fops, det)) {
					eem_debug("[%s]: create /proc/eem/%s/%s failed\n", __func__,
						det->name, det_entries[i].name);
				FUNC_EXIT(FUNC_LV_HELP);
				return -3;
				}
			}
		}
	} /* if (eem_disable == 0) */

	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
#if 0
static int create_procfs_vcore(void)
{
	struct proc_dir_entry *eem_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry eem_entries[] = {
		PROC_ENTRY(eem_vcore_volt),
	};

	FUNC_ENTER(FUNC_LV_HELP);
	eem_dir = proc_mkdir("eem", NULL);

	if (!eem_dir) {
		eem_debug("[%s]: mkdir /proc/eem failed\n", __func__);
		FUNC_EXIT(FUNC_LV_HELP);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(eem_entries); i++) {
		if (!proc_create(eem_entries[i].name, S_IRUGO | S_IWUSR | S_IWGRP,
			eem_dir, eem_entries[i].fops)) {
			eem_debug("[%s]: create /proc/eem/%s failed\n", __func__, eem_entries[i].name);
			FUNC_EXIT(FUNC_LV_HELP);
			return -3;
		}
	}
	FUNC_EXIT(FUNC_LV_HELP);
	return 0;
}
#endif
#endif /* CONFIG_PROC_FS */

/* return 0 means success */
int mt_eem_send_upower_table_ref(phys_addr_t phy_addr, unsigned long long size)
{
	int err = 0;
	struct eem_ipi_data eem_data;

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));

	eem_data.u.data.arg[0] = phy_addr;
	eem_data.u.data.arg[1] = size;
	err = eem_to_sspm(IPI_EEM_SEND_UPOWER_TBL_REF, &eem_data);

	return err;
}

#if 0 /* gpu dvfs no need this anymore */
unsigned int get_eem_status_for_gpu(void)
{
	struct eem_ipi_data eem_data;
	int ret = 0;

	FUNC_ENTER(FUNC_LV_MODULE);
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_sspm(IPI_EEM_GPU_DVFS_GET_STATUS, &eem_data);

	FUNC_EXIT(FUNC_LV_MODULE);
	return ret;
}
#endif

static void eem_init_postprocess(void)
{
	mt_eem_disable_mtcmos();
	#if EEM_BANK_BIG
	mt_eem_enable_big_cluster(0);
	#endif

}

/* push reg_dump_addr_off[] into eem log */
#if DUMP_DATA_TO_DE
static void mt_eem_push_reg_off_to_eemlog(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_dump_addr_off); i++)
		eem_logs->reg_dump_data_off[i] = reg_dump_addr_off[i];
}
#endif

static int eem_probe(struct platform_device *pdev)
{
	int ret;
	struct eem_ipi_data eem_data;
	struct eem_det *det;
	#ifdef CONFIG_OF
	struct device_node *node = NULL;
	#endif
	/* unsigned int code = mt_get_chip_hw_code(); */

	FUNC_ENTER(FUNC_LV_MODULE);

	#ifdef CONFIG_OF
	node = pdev->dev.of_node;
	if (!node) {
		eem_debug("get eem device node err\n");
		return -ENODEV;
	}
	/* Setup IO addresses */
	eem_base = of_iomap(node, 0);
	eem_debug("[EEM] eem_base = 0x%p\n", eem_base);
	eem_irq_number = irq_of_parse_and_map(node, 0);
	eem_debug("[THERM_CTRL] eem_irq_number=%d\n", eem_irq_number);
	if (!eem_irq_number) {
		eem_debug("[EEM] get irqnr failed=0x%x\n", eem_irq_number);
		return 0;
	}
	#endif

#if DUMP_DATA_TO_DE
	mt_eem_push_reg_off_to_eemlog();
#endif

	/* let ptp to disable gpu dvfs and enable vgpu buck */
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_sspm(IPI_EEM_PROBE, &eem_data);

	eem_debug("IPI PROBE done\n");
	/* enable big cluster and enable gpu clock */
	#if EEM_BANK_BIG
	mt_eem_enable_big_cluster(1);
	#endif
	mt_eem_get_clk(pdev);
	mt_eem_enable_mtcmos();

	/* let ptp run init01 */
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_sspm(IPI_EEM_INIT01, &eem_data);

	/* disable big cluster and disable gpu clock */
	eem_init_postprocess();

	/* let ptp to enable gpu dvfs*/
	memset(&eem_data, 0, sizeof(struct eem_ipi_data));
	eem_data.u.data.arg[0] = 0;
	ret = eem_to_sspm(IPI_EEM_INIT02, &eem_data);

#if 0
	for_each_det(det)
		inherit_base_det(det);
#endif

	/* print init2 volt */
	for_each_det(det) {
		eem_debug("[%d][%d]init2 volt = 0x%x%x%x%x, 0x%x%x%x%x, 0x%x%x%x%x, 0x%x%x%x%x\n",
					det->ctrl_id, eem_logs->det_log[det->ctrl_id].num_freq_tbl,
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[15],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[14],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[13],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[12],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[11],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[10],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[9],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[8],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[7],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[6],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[5],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[4],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[3],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[2],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[1],
					eem_logs->det_log[det->ctrl_id].volt_tbl_init2[0]);
	}
	eem_debug("eem_probe ok\n");
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}
static int eem_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*
	*kthread_stop(eem_volt_thread);
	*/
	FUNC_ENTER(FUNC_LV_MODULE);
	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}
/* tell sspm to call eem_init02 */
static int eem_resume(struct platform_device *pdev)
{
	/*
	* eem_volt_thread = kthread_run(eem_volt_thread_handler, 0, "eem volt");
	*if (IS_ERR(eem_volt_thread))
	*{
	*  eem_debug("[%s]: failed to create eem volt thread\n", __func__);
	*}
	*/

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id mt_eem_of_match[] = {
	{ .compatible = "mediatek,eem_fsm", },
	{},
};
#endif

static struct platform_driver eem_driver = {
	.remove	 = NULL,
	.shutdown   = NULL,
	.probe	  = eem_probe,
	.suspend	= eem_suspend,
	.resume	 = eem_resume,
	.driver	 = {
		.name   = "mt-eem",
#ifdef CONFIG_OF
		.of_match_table = mt_eem_of_match,
#endif
	},
};
static int __init eem_init(void)
{
	int err = 0;
	struct eem_ipi_data eem_data;

	FUNC_ENTER(FUNC_LV_MODULE);

	memset(&eem_data, 0, sizeof(struct eem_ipi_data));

	eem_data.u.data.arg[0] = eem_log_phy_addr;
	eem_data.u.data.arg[1] = eem_log_size;
	eem_debug("eem_log_phy_addr=0x%llx\n", (unsigned long long)eem_log_phy_addr);

	/* ret 1: eem disabled at sspm
	 * ret 0: eem enabled at sspm
	 * eem_disable default is 1
	 */
	/* read efuse data to dram log */
	get_devinfo();

	eem_disable = eem_to_sspm(IPI_EEM_INIT, &eem_data);

	create_procfs();

	/* use eem_disable to decide what procfs to create */
	if (eem_disable > 0) {
		eem_debug("EEM disabled\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return 0;
	}
	informEEMisReady = 1;
	/* reg platform device driver */
	err = platform_driver_register(&eem_driver);

	if (err) {
		eem_debug("EEM driver callback register failed..\n");
		FUNC_EXIT(FUNC_LV_MODULE);
		return err;
	}

	FUNC_EXIT(FUNC_LV_MODULE);

	return 0;

}

static void __exit eem_exit(void)
{
	FUNC_ENTER(FUNC_LV_MODULE);
	eem_debug("eem de-initialization\n");
	FUNC_EXIT(FUNC_LV_MODULE);
}

subsys_initcall(vcore_ptp_init);
late_initcall(eem_init);
#endif/* EEM_ENABLE_TINYSYS_SSPM */

MODULE_DESCRIPTION("MediaTek EEM Driver v0.3");
MODULE_LICENSE("GPL");
#ifdef EARLY_PORTING
	#undef EARLY_PORTING
#endif

#undef __MTK_EEM_C__
