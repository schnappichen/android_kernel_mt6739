#ifndef __SH_SVP_H__
#define __SH_SVP_H__

#define SVP_REGION_IOC_MAGIC		'S'

#define SVP_REGION_IOC_ONLINE		_IOR(SVP_REGION_IOC_MAGIC, 2, int)
#define SVP_REGION_IOC_OFFLINE		_IOR(SVP_REGION_IOC_MAGIC, 4, int)

#define SVP_REGION_ACQUIRE			_IOR(SVP_REGION_IOC_MAGIC, 6, int)
#define SVP_REGION_RELEASE			_IOR(SVP_REGION_IOC_MAGIC, 8, int)

void show_pte(struct mm_struct *mm, unsigned long addr);

#define UPPER_LIMIT32 (1ULL << 32)
#define UPPER_LIMIT64 (1ULL << 63)

extern int _tui_region_offline(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit);

int tui_region_offline64(phys_addr_t *pa, unsigned long *size)
{
	return _tui_region_offline(pa, size, UPPER_LIMIT64);
}
EXPORT_SYMBOL(tui_region_offline64);

int tui_region_offline(phys_addr_t *pa, unsigned long *size)
{
	return _tui_region_offline(pa, size, UPPER_LIMIT32);
}
EXPORT_SYMBOL(tui_region_offline);

extern int _svp_region_offline(phys_addr_t *pa, unsigned long *size,
		u64 upper_limit);

int svp_region_offline64(phys_addr_t *pa, unsigned long *size)
{
	return _svp_region_offline(pa, size, UPPER_LIMIT64);
}
EXPORT_SYMBOL(svp_region_offline64);

int svp_region_offline(phys_addr_t *pa, unsigned long *size)
{
	return _svp_region_offline(pa, size, UPPER_LIMIT32);
}
EXPORT_SYMBOL(svp_region_offline);

#ifdef CONFIG_MTK_ION
extern void ion_sec_heap_dump_info(void);
#else
static inline void ion_sec_heap_dump_info(void)
{
	pr_info("%s is not supported\n", __func__);
}
#endif

#endif
