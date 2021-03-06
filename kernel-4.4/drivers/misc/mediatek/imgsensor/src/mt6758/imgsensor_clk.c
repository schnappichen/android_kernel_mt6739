
#include <linux/clk.h>
#include "imgsensor_common.h"
#include "imgsensor_clk.h"

/*by platform settings and elements should not be reordered */
char *gimgsensor_mclk_name[IMGSENSOR_CCF_MAX_NUM] = {
	"CLK_TOP_CAMTG_SEL",
	"CLK_TOP_CAMTG2_SEL",
	"CLK_TOP_CAMTG3_SEL",
	"CLK_TOP_CAMTG4_SEL",
	"CLK_TOP_UNIVPLL_D416",
	"CLK_TOP_UNIVPLL_D208",
	"CLK_TOP_UNIVPLL_D104",
	"CLK_TOP_CLK26M",
	"CLK_TOP_UNIVPLL_D52",
	"CLK_TOP_UNIVPLL2_D16",
	"CLK_CAM_SENINF",
};


enum {
	MCLK_ENU_START,
	MCLK_6MHZ =	MCLK_ENU_START,
	MCLK_12MHZ,
	MCLK_24MHZ,
	MCLK_26MHZ,
	MCLK_48MHZ,
	MCLK_52MHZ,
	MCLK_MAX,
};

enum {
	FREQ_6MHZ  =  6,
	FREQ_12MHZ = 12,
	FREQ_24MHZ = 24,
	FREQ_26MHZ = 26,
	FREQ_48MHZ = 48,
	FREQ_52MHZ = 52,
};


static inline void imgsensor_clk_check(struct IMGSENSOR_CLK *pclk)
{
	int i;
	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++)
		WARN_ON(IS_ERR(pclk->imgsensor_ccf[i]));
}

enum IMGSENSOR_RETURN imgsensor_clk_init(struct IMGSENSOR_CLK *pclk)
{
	int i;
	struct platform_device *pplatform_dev = gpimgsensor_hw_platform_device;

	if (pplatform_dev == NULL) {
		PK_PR_ERR("[%s] pdev is null\n", __func__);
		return IMGSENSOR_RETURN_ERROR;
	}
	/* get all possible using clocks */
	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++)
		pclk->imgsensor_ccf[i] = devm_clk_get(&pplatform_dev->dev, gimgsensor_mclk_name[i]);

	return IMGSENSOR_RETURN_SUCCESS;
}

int imgsensor_clk_set(struct IMGSENSOR_CLK *pclk, ACDK_SENSOR_MCLK_STRUCT *pmclk)
{
	int ret = 0;
	int mclk_index = MCLK_ENU_START;
	const int supported_mclk_freq[MCLK_MAX] = {
		FREQ_6MHZ, FREQ_12MHZ, FREQ_24MHZ, FREQ_26MHZ, FREQ_48MHZ, FREQ_52MHZ };

	for (mclk_index = MCLK_ENU_START; mclk_index < MCLK_MAX; mclk_index++) {
		if (pmclk->freq == supported_mclk_freq[mclk_index])
			break;
	}
	if (pmclk->TG >= IMGSENSOR_CCF_MCLK_TG_MAX_NUM ||
		pmclk->TG < IMGSENSOR_CCF_MCLK_TG_MIN_NUM ||
		mclk_index == MCLK_MAX) {
		PK_PR_ERR("[CAMERA SENSOR]kdSetSensorMclk out of range, tg=%d, freq= %d\n",
			pmclk->TG, pmclk->freq);
		return -EFAULT;
	}
	mclk_index += IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM;
	imgsensor_clk_check(pclk);

	if (pmclk->on) {

		/* Workaround for timestamp: TG1 always ON */
		if (clk_prepare_enable(pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]))
			PK_PR_ERR("[CAMERA SENSOR] failed tg=%d\n", IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL);
		else
			atomic_inc(&pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[pmclk->TG]))
			PK_PR_ERR("[CAMERA SENSOR] failed tg=%d\n", pmclk->TG);
		else
			atomic_inc(&pclk->enable_cnt[pmclk->TG]);

		if (clk_prepare_enable(pclk->imgsensor_ccf[mclk_index]))
			PK_PR_ERR("[CAMERA SENSOR]imgsensor_ccf failed freq= %d, mclk_index %d\n",
					pmclk->freq, mclk_index);
		else
			atomic_inc(&pclk->enable_cnt[mclk_index]);

		ret = clk_set_parent(pclk->imgsensor_ccf[pmclk->TG], pclk->imgsensor_ccf[mclk_index]);
	} else {

		/* Workaround for timestamp: TG1 always ON */
		clk_disable_unprepare(pclk->imgsensor_ccf[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);
		atomic_dec(&pclk->enable_cnt[IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL]);

		clk_disable_unprepare(pclk->imgsensor_ccf[pmclk->TG]);
		atomic_dec(&pclk->enable_cnt[pmclk->TG]);
		clk_disable_unprepare(pclk->imgsensor_ccf[mclk_index]);
		atomic_dec(&pclk->enable_cnt[mclk_index]);
	}

	return ret;
}

void imgsensor_clk_enable_all(struct IMGSENSOR_CLK *pclk)
{
}

void imgsensor_clk_disable_all(struct IMGSENSOR_CLK *pclk)
{
	int i;

	PK_DBG("imgsensor_clk_disable_all\n");
	for (i = 0; i < IMGSENSOR_CCF_MAX_NUM; i++) {
		for (; atomic_read(&pclk->enable_cnt[i]) > 0; ) {
			clk_disable_unprepare(pclk->imgsensor_ccf[i]);
			atomic_dec(&pclk->enable_cnt[i]);
		}
	}
}

int imgsensor_clk_ioctrl_handler(void *pbuff)
{
	*(unsigned int *)pbuff = mt_get_ckgen_freq(*(unsigned int *)pbuff);
	PK_DBG("f_fcamtg_ck = %d\n", mt_get_ckgen_freq(11));
	PK_DBG("hf_fcam_ck = %d\n", mt_get_ckgen_freq(38));
	PK_DBG("f_fseninf_ck = %d\n", mt_get_ckgen_freq(56));
	return 0;
}
