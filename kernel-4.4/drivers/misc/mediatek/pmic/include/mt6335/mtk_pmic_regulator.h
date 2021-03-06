
#ifndef _MT_PMIC_REGULATOR_H_
#define _MT_PMIC_REGULATOR_H_

enum BUCK_TYPE {
	VCORE,
	VDRAM,
	VMODEM,
	VMD1,
	VS1,
	VS2,
	VPA1,
	VIMVO,
	VSRAM_DVFS1,
	VSRAM_DVFS2,
	VSRAM_VGPU,
	VSRAM_VCORE,
	VSRAM_VMD,
};

struct mtk_bucks_t {
	const char *name;
	unsigned n_voltages;
	unsigned int min_uV;
	unsigned int max_uV;
	unsigned int uV_step;
	unsigned int stb;
	struct device_attribute en_att;
	struct device_attribute voltage_att;
	PMU_FLAGS_LIST_ENUM en;
	PMU_FLAGS_LIST_ENUM mode;
	PMU_FLAGS_LIST_ENUM vosel;
	PMU_FLAGS_LIST_ENUM da_qi_en;
	PMU_FLAGS_LIST_ENUM da_ni_vosel;
	bool isUsedable;
	const char *type;
};

#define PMIC_BUCK_GEN1(_name, _en, _mode, _vosel, _da_qi_en, _da_ni_vosel, min, max, step, _stb, _id)	\
	{	\
		.name = #_name,	\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.min_uV = (min),	\
		.max_uV = (max),	\
		.uV_step = (step),	\
		.en = (_en),	\
		.mode = (_mode),	\
		.vosel = (_vosel),	\
		.da_qi_en = (_da_qi_en),	\
		.da_ni_vosel = (_da_ni_vosel),	\
		.stb = (_stb),	\
		.isUsedable = 0,	\
		.type = "BUCK",	\
	}
#endif				/* _MT_PMIC_REGULATOR_H_ */
