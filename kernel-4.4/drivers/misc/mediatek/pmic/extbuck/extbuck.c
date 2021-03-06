
#include <linux/regulator/consumer.h>
#include <mt-plat/upmu_common.h>

int is_ext_buck_exist(void)
{
#ifndef CONFIG_MTK_EXTBUCK
	return 0;
#else
	#ifdef CONFIG_REGULATOR_ISL91302A
	struct regulator *reg;

	reg = regulator_get(NULL, "ext_buck_proc1");
	if (reg == NULL)
		return 0;
	regulator_put(reg);
	return 1;
	#endif /* CONFIG_REGULATOR_ISL91302A */
	#ifdef CONFIG_EXTBUCK_MT6311
	if ((is_mt6311_exist() == 1))
		return 1;
	#endif /* CONFIG_EXTBUCK_MT6311 */
	#if defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_exist() == 1))
		return 1;
	#endif /* CONFIG_MTK_PMIC_CHIP_MT6313 */
#endif /* if not CONFIG_MTK_EXTBUCK */
	return 0;
}

int is_ext_buck2_exist(void)
{
#ifndef CONFIG_MTK_EXTBUCK
	return 0;
#else
	#ifdef CONFIG_REGULATOR_RT5738
	struct regulator *reg;

#if defined(CONFIG_MACH_MT6775)
	reg = regulator_get(NULL, "ext_buck_vpu");
#else
	reg = regulator_get(NULL, "ext_buck_lp4x");
#endif
	if (reg == NULL)
		return 0;
	regulator_put(reg);
	return 1;
	#endif /* CONFIG_REGULATOR_RT5738 */
	return 0;
#endif /* if not CONIFG_MTK_EXTBUCK */
}

int is_ext_buck_sw_ready(void)
{
#if defined(CONFIG_EXTBUCK_MT6311)
	if ((is_mt6311_sw_ready() == 1))
		return 1;
#elif defined(CONFIG_MTK_PMIC_CHIP_MT6313)
	if ((is_mt6313_sw_ready() == 1))
		return 1;
#endif
	return 0;
}

