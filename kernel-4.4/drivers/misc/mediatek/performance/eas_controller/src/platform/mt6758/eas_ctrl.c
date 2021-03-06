
#define TAG "[Boost Controller]"

#include <asm/div64.h>
#include <linux/kernel.h>

#include <mach/mtk_cpufreq_api.h>
#include <mach/mtk_ppm_api.h>
#include <fpsgo_common.h>

static long long cpi_ll_boost_threshold[2], cpi_l_boost_threshold[2];

void update_pwd_tbl(void)
{
	long long max_freq;

	max_freq = mt_cpufreq_get_freq_by_idx(1, 0);
	if (max_freq <= 0) {
		cpi_ll_boost_threshold[0] = 101;
		cpi_ll_boost_threshold[1] = 101;
		cpi_l_boost_threshold[0] = 101;
		cpi_l_boost_threshold[1] = 101;
		pr_debug(TAG" max_freq:%lld, ll_0:%lld, ll_1:%lld, l_0:%lld, l_1:%lld\n",
			max_freq, cpi_ll_boost_threshold[0], cpi_ll_boost_threshold[1],
			cpi_l_boost_threshold[0], cpi_l_boost_threshold[1]);
		return;
	}

	cpi_ll_boost_threshold[0] = 80600000LL;
	do_div(cpi_ll_boost_threshold[0], max_freq);
	cpi_ll_boost_threshold[1] = 160000000LL;
	do_div(cpi_ll_boost_threshold[1], max_freq);
	cpi_l_boost_threshold[0] = 100000000LL;
	do_div(cpi_l_boost_threshold[0], max_freq);
	cpi_l_boost_threshold[1] = 200000000LL;
	do_div(cpi_l_boost_threshold[1], max_freq);

	pr_debug(TAG" max_freq:%lld, ll_0:%lld, ll_1:%lld, l_0:%lld, l_1:%lld\n",
			max_freq, cpi_ll_boost_threshold[0], cpi_ll_boost_threshold[1],
			cpi_l_boost_threshold[0], cpi_l_boost_threshold[1]);
}

int reduce_stall(int boost_value, int cpi_thres, int vcore_high)
{
	unsigned int cpi_ll, cpi_l;
	int vcore_opp = 0;

	if (boost_value < 3000) {
		vcore_opp = -1;
	/*LL*/
	} else if (boost_value >= 3101 && boost_value <= 3200) {
		cpi_ll = ppm_get_cluster_cpi(0);
		fpsgo_systrace_c_fbt_gm(-400, cpi_ll, "cpi_ll");
		if (cpi_ll < cpi_thres)
			vcore_opp = -1;
		else if (boost_value - 3100 > cpi_ll_boost_threshold[1] && vcore_high)
			vcore_opp = 0;
		else if (boost_value - 3100 > cpi_ll_boost_threshold[0])
			vcore_opp = 1;
		else
			vcore_opp = -1;
	/*L*/
	} else if (boost_value >= 3201 && boost_value <= 3300) {
		cpi_l = ppm_get_cluster_cpi(1);
		fpsgo_systrace_c_fbt_gm(-400, cpi_l, "cpi_l");
		if (cpi_l < cpi_thres)
			vcore_opp = -1;
		else if (boost_value - 3200 > cpi_l_boost_threshold[1] && vcore_high)
			vcore_opp = 0;
		else if (boost_value - 3200 > cpi_l_boost_threshold[0])
			vcore_opp = 1;
		else
			vcore_opp = -1;
	}

	return vcore_opp;
}
