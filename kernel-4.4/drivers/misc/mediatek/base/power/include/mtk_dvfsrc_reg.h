
#ifndef _COMMON_MTK_DVFSRC_REG_H
#define _COMMON_MTK_DVFSRC_REG_H

#if defined(CONFIG_MACH_MT6799)

#include "spm_v3/mtk_dvfsrc_reg.h"

#elif defined(CONFIG_MACH_MT6775)

#include "spm_v3/mtk_dvfsrc_reg_mt6775.h"

#elif defined(CONFIG_MACH_MT6771)

#include "spm_v4/mtk_dvfsrc_reg_mt6771.h"

#elif defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6739)

#include "spm_v4/mtk_dvfsrc_reg.h"

#endif

#endif /* _COMMON_MTK_DVFSRC_REG_H */

