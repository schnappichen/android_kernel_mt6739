
#include <linux/platform_device.h>

#include "mt_emi.h"

enum {
	MASTER_ARM0 = 0,
	MASTER_ARM1 = 1,
	MASTER_MM0 = 2,
	MASTER_MDMCU = 3,
	MASTER_MD_PERI = 4,
	MASTER_MM1 = 5,
	MASTER_GPU0 = 6,
	MASTER_GPU1 = 7,
	MASTER_ALL = 8
};

int is_md_master(unsigned int master_id)
{
	if ((master_id & 0x7) == MASTER_MDMCU)
		return 1;

	if ((master_id & 0x7) == MASTER_MD_PERI) {
		if (master_id & 0xC000)
			return 1;
	}

	return 0;
}

void set_ap_region_permission(unsigned int apc[EMI_MPU_DGROUP_NUM])
{
	SET_ACCESS_PERMISSION(apc, LOCK,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, FORBIDDEN,
		FORBIDDEN, FORBIDDEN, NO_PROTECTION, NO_PROTECTION,
		FORBIDDEN, SEC_R_NSEC_RW, FORBIDDEN, NO_PROTECTION,
		FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);
}

