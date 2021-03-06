
#ifndef __MMDVFS_CONFIG_MT6775_H__
#define __MMDVFS_CONFIG_MT6775_H__

#include "mmdvfs_config_util.h"
#include "mtk_vcorefs_manager.h"

/* Part I MMSVFS HW Configuration (OPP)*/
/* Define the number of mmdvfs, vcore and mm clks opps */

#define MT6775_MMDVFS_OPP_MAX 4    /* Max total MMDVFS opps of the profile support */

/* CLK source configuration */

/* CLK source IDs */
/* Define the internal index of each CLK source*/
#define MT6775_MMDVFS_CLK_SYSPLL_D2_CK 0
#define MT6775_MMDVFS_CLK_MMPLL_D6_CK 1
#define MT6775_MMDVFS_CLK_UNIVPLL_D5_CK 2
#define MT6775_MMDVFS_CLK_MMPLL_D7_CK 3
#define MT6775_MMDVFS_CLK_MMPLL_D8_CK 4
#define MT6775_MMDVFS_CLK_SYSPLL_D3_CK 5
#define MT6775_MMDVFS_CLK_MMPLL_D10_CK 6
#define MT6775_MMDVFS_CLK_UNIVPLL1_D4_CK 7
#define MT6775_MMDVFS_CLK_SOURCE_NUM 8

/* CLK Source definiation */
/* Define the clk source description */
struct mmdvfs_clk_source_desc mt6775_mmdvfs_clk_sources_setting[MT6775_MMDVFS_CLK_SOURCE_NUM] = {
		{NULL, "mmdvfs_clk_top_syspll_d2_ck", 546},
		{NULL, "mmdvfs_clk_top_mmpll_d6_ck", 533},
		{NULL, "mmdvfs_clk_top_univpll_d5_ck", 499},
		{NULL, "mmdvfs_clk_top_mmpll_d7_ck", 457},
		{NULL, "mmdvfs_clk_top_mmpll_d8_ck", 400},
		{NULL, "mmdvfs_clk_top_syspll_d3_ck", 364},
		{NULL, "mmdvfs_clk_top_mmpll_d10_ck", 320},
		{NULL, "mmdvfs_clk_top_univpll1_d4_ck", 312},
};

/* B. CLK Change adaption configurtion */
/* B.1 Define the clk change method and related infomration of each MM CLK step */
/* Field decscription: */
/* 1. config_method: MMDVFS_CLK_CONFIG_BY_MUX/ MMDVFS_CLK_CONFIG_PLL / MMDVFS_CLK_CONFIG_NONE */
/* 2. pll_id: MTK' PLL ID, please set -1 if PLL hopping is not used */
/* 3. clk mux desc {hanlde, name}, the please handle -1 and it will be initialized by driver automaticlly */
/* 4. total step: the number of the steps supported by this sub sys */
/* 5. hopping dss of each steps: please set -1 if it is not used */
/* 6. clk sources id of each steps: please set -1 if it is not used */
struct mmdvfs_clk_hw_map mt6775_mmdvfs_clk_hw_map_setting[MMDVFS_CLK_MUX_NUM] = {
		{ MMDVFS_CLK_CONFIG_NONE, { NULL, "MMDVFS_CLK_MUX_TOP_SMI0_2X_SEL"}, -1, 3,
			{-1, -1, -1},
			{-1, -1, -1}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX, { NULL, "MMDVFS_CLK_MUX_TOP_MM_SEL"}, -1, 3,
			{-1, -1, -1},
			{MT6775_MMDVFS_CLK_MMPLL_D7_CK,
			MT6775_MMDVFS_CLK_MMPLL_D8_CK,
			MT6775_MMDVFS_CLK_MMPLL_D10_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX, { NULL, "MMDVFS_CLK_MUX_TOP_CAM_SEL"}, -1, 3,
			{-1, -1, -1},
			{MT6775_MMDVFS_CLK_SYSPLL_D2_CK,
			MT6775_MMDVFS_CLK_SYSPLL_D3_CK,
			MT6775_MMDVFS_CLK_MMPLL_D10_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX, { NULL, "MMDVFS_CLK_MUX_TOP_IMG_SEL"}, -1, 3,
			{-1, -1, -1},
			{MT6775_MMDVFS_CLK_MMPLL_D6_CK,
			MT6775_MMDVFS_CLK_SYSPLL_D3_CK,
			MT6775_MMDVFS_CLK_MMPLL_D10_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX, { NULL, "MMDVFS_CLK_MUX_TOP_VENC_SEL"}, -1, 3,
			{-1, -1, -1},
			{MT6775_MMDVFS_CLK_UNIVPLL_D5_CK,
			MT6775_MMDVFS_CLK_UNIVPLL1_D4_CK,
			MT6775_MMDVFS_CLK_UNIVPLL1_D4_CK}
		},
		{ MMDVFS_CLK_CONFIG_BY_MUX, { NULL, "MMDVFS_CLK_MUX_TOP_VDEC_SEL"}, -1, 3,
			{-1, -1, -1},
			{MT6775_MMDVFS_CLK_UNIVPLL_D5_CK,
			MT6775_MMDVFS_CLK_MMPLL_D10_CK,
			MT6775_MMDVFS_CLK_MMPLL_D10_CK}
		},
		{ MMDVFS_CLK_CONFIG_NONE, { NULL, "MMDVFS_CLK_MUX_TOP_MJC_SEL"}, -1, 3,
			{-1, -1, -1},
			{-1, -1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE, { NULL, "MMDVFS_CLK_MUX_TOP_VPU_IF_SEL"}, -1, 3,
			{-1, -1, -1},
			{-1, -1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE,   { NULL, "MMDVFS_CLK_MUX_TOP_VPU_IF_SEL"}, -1, 3,
			{-1, -1, -1},
			{-1, -1, -1}
		},
		{ MMDVFS_CLK_CONFIG_NONE, { NULL, "MMDVFS_CLK_MUX_TOP_VPU_SEL"}, -1, 2,
			{-1, -1, -1},
			{-1, -1, -1}
		}
};

/* Part II MMDVFS Scenario's Step Confuguration */

#define MT6775_MMDVFS_SENSOR_MIN (16000000)
/* A.1 [LP4 2-ch] Scenarios of each MM DVFS Step (force kicker) */
/* OPP 0 scenarios */
#define MT6775_MMDVFS_OPP0_NUM 16
struct mmdvfs_profile mt6775_mmdvfs_opp0_profiles[MT6775_MMDVFS_OPP0_NUM] = {
	{"SMVR", SMI_BWC_SCEN_VR_SLOW, {0, 0, 0}, {0, 0, 0 } },
	{"ICFP", SMI_BWC_SCEN_ICFP, {0, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Capture (ZSD)", SMI_BWC_SCEN_CAM_CP, {MT6775_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Preview (ZSD)", SMI_BWC_SCEN_CAM_PV, {MT6775_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Full Sensor Camera Recording", SMI_BWC_SCEN_VR, {MT6775_MMDVFS_SENSOR_MIN, 0, 0}, {0, 0, 0 } },
	{"Stereo Feature Preview", SMI_BWC_SCEN_CAM_PV, {0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Capture", SMI_BWC_SCEN_CAM_CP, {0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Stereo Feature Recording", SMI_BWC_SCEN_VR, {0, MMDVFS_CAMERA_MODE_FLAG_STEREO, 0}, {0, 0, 0 } },
	{"Dual zoom preview", SMI_BWC_SCEN_CAM_PV, {0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_CAM_CP,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"Dual zoom preview (reserved)", SMI_BWC_SCEN_VR,
		{0, MMDVFS_CAMERA_MODE_FLAG_DUAL_ZOOM, 0}, {0, 0, 0 } },
	{"EIS 4K Feature Recording", SMI_BWC_SCEN_VR,
		{MT6775_MMDVFS_SENSOR_MIN, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"4K VR/ VSS (VENC)", SMI_BWC_SCEN_VENC, {0, 0, 0}, {4096, 1716, 0} },
	{"High resolution video playback", SMI_BWC_SCEN_VP_HIGH_RESOLUTION, {0, 0, 0}, {0, 0, 0 } },
	{"Camera 4K VR Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0, 3840*2160}, {0, 0, 0 } },
	{"Camera 4K VR Recording", SMI_BWC_SCEN_VR, {0, 0, 0, 3840*2160}, {0, 0, 0 } },
};

/* OPP 1 scenarios */
#define MT6775_MMDVFS_OPP1_NUM 0
struct mmdvfs_profile mt6775_mmdvfs_opp1_profiles[MT6775_MMDVFS_OPP1_NUM] = {
};

/* OPP 2 scenarios */
#define MT6775_MMDVFS_OPP2_NUM 0
struct mmdvfs_profile mt6775_mmdvfs_opp2_profiles[MT6775_MMDVFS_OPP2_NUM] = {
};

/* OPP 3 scenarios */
#define MT6775_MMDVFS_OPP3_NUM 12
struct mmdvfs_profile mt6775_mmdvfs_opp3_profiles[MT6775_MMDVFS_OPP3_NUM] = {
	{"EIS Feature Recording", SMI_BWC_SCEN_VR, {0, MMDVFS_CAMERA_MODE_FLAG_EIS_2_0, 0}, {0, 0, 0 } },
	{"Camera Preview", SMI_BWC_SCEN_CAM_PV, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Capture", SMI_BWC_SCEN_CAM_CP, {0, 0, 0}, {0, 0, 0 } },
	{"Camera Recording", SMI_BWC_SCEN_VR, {0, 0, 0}, {0, 0, 0 } },
	{"VSS", SMI_BWC_SCEN_VSS, {0, 0, 0}, {0, 0, 0 } },
	{"VENC", SMI_BWC_SCEN_VENC, {0, 0, 0}, {0, 0, 0} },
	{"MHL", MMDVFS_SCEN_MHL, {0, 0, 0}, {0, 0, 0 } },
	{"WFD", SMI_BWC_SCEN_WFD, {0, 0, 0}, {0, 0, 0 } },
	{"High frame rate video playback", SMI_BWC_SCEN_VP_HIGH_FPS, {0, 0, 0}, {0, 0, 0 } },
	{"vFB Feature Preview", SMI_BWC_SCEN_CAM_PV, {0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
	{"vFB Feature Capture", SMI_BWC_SCEN_CAM_CP, {0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
	{"vFB Feature Recording", SMI_BWC_SCEN_VR, {0, MMDVFS_CAMERA_MODE_FLAG_VFB, 0}, {0, 0, 0 } },
};

/* Defined the smi scenarios whose DVFS is controlled by low-level driver */
/* directly, not by BWC scenario change event */
#define MT6775_MMDVFS_SMI_USER_CONTROL_SCEN_MASK (1 << SMI_BWC_SCEN_VP_HIGH_FPS)

/* Part III Scenario and MMSVFS HW configuration mapping */
/* 1. For a single mmdvfs step's profiles and associated hardware configuration */
/* LP4 2-ch */
struct mmdvfs_step_to_profile_mapping mt6775_step_profile[MT6775_MMDVFS_OPP_MAX] = {
		{0, mt6775_mmdvfs_opp0_profiles, MT6775_MMDVFS_OPP0_NUM,
		{OPP_1,
		{MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0,
		MMDVFS_MMCLK_OPP0, MMDVFS_MMCLK_OPP0}, MMDVFS_CLK_MUX_NUM
		}
		},
		{1, mt6775_mmdvfs_opp1_profiles, MT6775_MMDVFS_OPP1_NUM,
		{OPP_2,
		{MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1,
		MMDVFS_MMCLK_OPP1, MMDVFS_MMCLK_OPP1}, MMDVFS_CLK_MUX_NUM
		}
		},
		{2, mt6775_mmdvfs_opp2_profiles, MT6775_MMDVFS_OPP2_NUM,
		{OPP_4,
		{MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2,
		MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2,
		MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2}, MMDVFS_CLK_MUX_NUM
		}
		},
		{3, mt6775_mmdvfs_opp3_profiles, MT6775_MMDVFS_OPP3_NUM,
		{OPP_6,
		{MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2,
		MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2,
		MMDVFS_MMCLK_OPP2, MMDVFS_MMCLK_OPP2}, MMDVFS_CLK_MUX_NUM
		}
		},
};

/* Part III Scenario and MMSVFS HW configuration mapping */

#define MT6775_MMDVFS_VOLTAGE_LOW_OPP	2
#define MT6775_MMDVFS_VOLTAGE_HIGH_OPP	1
#define MT6775_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP	-1
#define MT6775_MMDVFS_VOLTAGE_LOW_LOW_OPP 3

int mt6775_mmdvfs_legacy_step_to_opp[MMDVFS_VOLTAGE_COUNT] = {MT6775_MMDVFS_VOLTAGE_LOW_OPP,
	MT6775_MMDVFS_VOLTAGE_HIGH_OPP, MT6775_MMDVFS_VOLTAGE_DEFAULT_STEP_OPP,
	MT6775_MMDVFS_VOLTAGE_LOW_LOW_OPP
};

#define MT6775_MMCLK_OPP0_LEGACY_STEP	MMSYS_CLK_HIGH
#define MT6775_MMCLK_OPP1_LEGACY_STEP	MMSYS_CLK_LOW
/* MMCLK_OPP3 and OPP2 is not used in this configuration */
#define MT6775_MMCLK_OPP2_LEGACY_STEP	MMSYS_CLK_LOW
#define MT6775_MMCLK_OPP3_LEGACY_STEP	MMSYS_CLK_LOW

int mt6775_mmdvfs_mmclk_opp_to_legacy_mmclk_step[MT6775_MMDVFS_OPP_MAX] = {
	MT6775_MMCLK_OPP0_LEGACY_STEP, MT6775_MMCLK_OPP1_LEGACY_STEP,
	MT6775_MMCLK_OPP2_LEGACY_STEP
};


/* Part IV VPU association */
/* There is no VPU DVFS in MT6775 */

/* Part V ISP DVFS configuration */
#define MMDVFS_ISP_THRESHOLD_NUM 3
int mt6775_mmdvs_isp_threshold_setting[MMDVFS_ISP_THRESHOLD_NUM] = {546, 364, 320};
int mt6775_mmdvs_isp_threshold_opp[MMDVFS_ISP_THRESHOLD_NUM] = {MMDVFS_FINE_STEP_OPP0,
	MMDVFS_FINE_STEP_OPP1, MMDVFS_FINE_STEP_OPP3};

struct mmdvfs_threshold_setting mt6775_mmdvfs_threshold_settings[MMDVFS_PM_QOS_SUB_SYS_NUM] = {
	{ MMDVFS_PM_QOS_SUB_SYS_CAMERA, mt6775_mmdvs_isp_threshold_setting,
	mt6775_mmdvs_isp_threshold_opp, MMDVFS_ISP_THRESHOLD_NUM, MMDVFS_PMQOS_ISP},
};


#endif /* __MMDVFS_CONFIG_MT6775_H__ */
